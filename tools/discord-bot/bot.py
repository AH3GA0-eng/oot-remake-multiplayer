"""Squadala! bug-report bot.

One slash command, /report, in one server. A report is posted to the private
dev channel, saved under coord-multiplayer/player-logs/<user>-<UTC time>/ and
added to coord-multiplayer/reports/player-reports.md as 'NEW (bot)'.

No privileged intents and no message content: the bot only sees the slash
commands sent to it. Runs on the owner's PC (run-bot.ps1).

Usage: python bot.py --config <path to discord-bot.json>
"""

import argparse
import asyncio
import datetime as dt
import io
import logging
import logging.handlers
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import report_core as core  # noqa: E402

try:
    import discord
    from discord import app_commands
except ImportError:  # pragma: no cover - the tests never import this file
    print("discord.py is not installed; see tools/discord-bot/README.md", file=sys.stderr)
    sys.exit(3)

log = logging.getLogger("report-bot")


class _HideToken(logging.Filter):
    """Belt and braces: the token never reaches a log line, whatever logs it."""

    def __init__(self, token):
        super().__init__()
        self.token = token

    def filter(self, record):
        msg = record.getMessage()
        if self.token and self.token in msg:
            record.msg = msg.replace(self.token, "[token]")
            record.args = ()
        return True


def setup_logging(log_path, token):
    handler = logging.handlers.RotatingFileHandler(log_path, maxBytes=1_000_000, backupCount=3, encoding="utf-8")
    handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))
    handler.addFilter(_HideToken(token))
    console = logging.StreamHandler()
    console.setFormatter(handler.formatter)
    console.addFilter(_HideToken(token))
    root = logging.getLogger()
    root.setLevel(logging.INFO)
    root.addHandler(handler)
    root.addHandler(console)


class ReportBot(discord.Client):
    def __init__(self, cfg):
        intents = discord.Intents.none()
        intents.guilds = True  # channel cache only; not privileged
        super().__init__(intents=intents, allowed_mentions=discord.AllowedMentions.none(),
                         application_id=cfg.get("application_id"))
        self.cfg = cfg
        self.guild = None  # set in setup_hook
        self.tree = app_commands.CommandTree(self)
        self.limiter = core.RateLimiter(limit=3, window=3600.0)
        self.lock = asyncio.Lock()
        self.logs_dir = cfg["coord_dir"] / "player-logs"
        self.tracker = cfg["coord_dir"] / "reports" / core.TRACKER_NAME

    async def setup_hook(self):
        # The server: from the config, else from the dev channel (the bot must see it).
        guild_id = self.cfg["guild_id"]
        if guild_id is None:
            try:
                channel = await self.fetch_channel(self.cfg["dev_channel_id"])
                guild_id = getattr(channel, "guild_id", None) or getattr(getattr(channel, "guild", None), "id", None)
            except discord.HTTPException as e:
                log.warning("cannot read the dev channel %s (%s); is the bot in the server?",
                            self.cfg["dev_channel_id"], type(e).__name__)
        self.guild = discord.Object(id=guild_id) if guild_id else None
        self._add_commands()
        # Server commands show up at once; global ones (fallback) can take up to an hour.
        synced = await self.tree.sync(guild=self.guild)
        where = f"server {guild_id}" if self.guild else "all servers (global)"
        log.info("commands synced to %s: %s", where, ", ".join(c.name for c in synced))

    async def on_ready(self):
        log.info("online as %s", self.user)

    def _add_commands(self):
        @app_commands.command(name="report", description="Send a bug report (and your logs zip) to the Squadala! team.")
        @app_commands.describe(
            description="What happened, and what you were doing (solo or co-op, host or guest).",
            logs="Optional: the zip from the kit's collect-logs (zip only, 25 MB max).",
            version="Optional: your kit version, e.g. 0.11.3.",
        )
        @app_commands.guild_only()
        async def report(
            interaction: discord.Interaction,
            description: app_commands.Range[str, 5, core.MAX_DESCRIPTION],
            logs: discord.Attachment = None,
            version: app_commands.Range[str, 1, 32] = None,
        ):
            await self.handle_report(interaction, description, logs, version)

        self.tree.add_command(report, guild=self.guild)  # guild=None: global

    async def handle_report(self, interaction, description, logs, version):
        user = interaction.user
        allowed = self.cfg["report_channel_id"]
        if allowed and interaction.channel_id not in (allowed, self.cfg["dev_channel_id"]):
            await interaction.response.send_message(f"Please use /report in <#{allowed}>.", ephemeral=True)
            return
        wait = self.limiter.retry_after(user.id)
        if wait > 0:
            await interaction.response.send_message(
                f"You sent several reports recently; please try again in {int(wait // 60) + 1} min.", ephemeral=True)
            return
        if logs is not None:
            problem = core.check_attachment_meta(logs.filename, logs.size)
            if problem:
                await interaction.response.send_message(problem, ephemeral=True)
                return

        await interaction.response.defer(ephemeral=True, thinking=True)
        data = None
        if logs is not None:
            try:
                data = await logs.read()
            except discord.HTTPException:
                log.exception("download of the attachment failed")
                await interaction.followup.send("I could not download the zip; please try again.", ephemeral=True)
                return
            problem = core.check_zip_bytes(data)
            if problem:
                await interaction.followup.send(problem, ephemeral=True)
                return

        self.limiter.record(user.id)
        text = core.clean_description(description)
        version = core.clean_description(version, 32) if version else None
        when = dt.datetime.now(dt.timezone.utc)
        username = user.name
        display = getattr(user, "display_name", None) or username

        try:
            async with self.lock:
                folder = core.unique_dir(self.logs_dir, core.folder_name(username, when))
                rel = f"player-logs/{folder.name}"
                number = core.append_row(
                    self.tracker,
                    lambda n: core.format_row(n, when, username, version, text, rel),
                )
                if data is not None:
                    (folder / "logs.zip").write_bytes(data)
                (folder / "description.txt").write_text(
                    core.description_file(number, when, username, display, version,
                                          logs.filename if logs else None, text),
                    encoding="utf-8")
        except OSError:
            log.exception("saving report failed")
            await interaction.followup.send("Sorry, saving the report failed. Please try again later.", ephemeral=True)
            return
        log.info("report #%d from %s saved to %s (%s)", number, username, rel,
                 f"{len(data)} bytes" if data is not None else "no zip")

        await self.post_to_dev(number, when, user, display, version, text, logs, data, rel)
        await interaction.followup.send(f"Thanks, report #{number} received.", ephemeral=True)

    async def post_to_dev(self, number, when, user, display, version, text, logs, data, rel):
        try:
            channel = self.get_channel(self.cfg["dev_channel_id"]) or await self.fetch_channel(self.cfg["dev_channel_id"])
            embed = discord.Embed(title=f"Bug report #{number}", description=text, timestamp=when,
                                  colour=discord.Colour.orange())
            embed.add_field(name="Player", value=f"{discord.utils.escape_markdown(display)} (@{user.name})")
            embed.add_field(name="Kit version", value=version or "?")
            embed.add_field(name="Saved", value=f"`{rel}`", inline=False)
            files = []
            if data is not None:
                limit = getattr(getattr(channel, "guild", None), "filesize_limit", 10 * 1024 * 1024)
                if len(data) <= limit:
                    files.append(discord.File(io.BytesIO(data), filename=f"report-{number}-logs.zip"))
                else:
                    embed.add_field(name="Logs", value="Zip too big to re-post here; saved on the PC.", inline=False)
            else:
                embed.add_field(name="Logs", value="None attached.", inline=False)
            await channel.send(embed=embed, files=files)
        except discord.HTTPException:
            # The report is already saved; the dev post is a copy.
            log.exception("posting report #%d to the dev channel failed", number)


def main():
    here = Path(__file__).resolve().parent
    default_cfg = here.parent.parent.parent / "coord-multiplayer" / "secrets" / "discord-bot.json"
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--config", default=os.environ.get("DISCORD_BOT_CONFIG", str(default_cfg)))
    args = ap.parse_args()
    try:
        cfg = core.load_config(args.config)
    except core.ConfigError as e:
        print(f"discord-bot: {e}", file=sys.stderr)
        return 2
    cfg["coord_dir"].joinpath("player-logs").mkdir(parents=True, exist_ok=True)
    setup_logging(cfg["coord_dir"] / "player-logs" / "discord-bot.log", cfg["token"])
    bot = ReportBot(cfg)
    try:
        bot.run(cfg["token"], log_handler=None)
    except discord.LoginFailure:
        log.error("Discord refused the token (check secrets/discord-bot.json)")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
