/* SPDX-License-Identifier: GPL-3.0-or-later
   (oot-remake-multiplayer's patch 0027) Co-op's log lines stay out of the crash report.

   The co-op module logs through hos_log (patch 0001's bridge_log), so its lines are in the run's log: the server's address,
   the players' nicknames, a microphone's name. Longshot's crash report (host/hos_crash_dialog.c, rc.6's #983) takes the log's
   last lines into what Send posts (crash_lines) and into the report it saves and copies (read_log); its redaction takes out
   the home folder, the user name and the chosen microphone, not those. Both readers ask this first and skip such a line.

   A line as hos_log writes it: "[hos <thread>] [<level>] <text>" (the level tag only when not info); the module's own level
   word ("warning: ", "error: ", "critical: ", hookshot/compat/common/logging.h) may lead the text. Co-op's text begins with
   "coop:" (the module, patch 0001's bridge) or "coop voice:" (patch 0009). Plain C. */
#ifndef HOS_COOP_CRASH_H
#define HOS_COOP_CRASH_H
#include <string.h>

static inline const char *hos_coop_crash_skip_tag(const char *p) {   /* (one "[...] " at p, when there is one) */
    if (*p != '[') return p;
    const char *end = strchr(p, ']');
    if (!end) return p;
    for (p = end + 1; *p == ' '; p++) {}
    return p;
}

static inline int hos_coop_crash_private(const char *line) {
    const char *p = hos_coop_crash_skip_tag(hos_coop_crash_skip_tag(line));
    static const char *const levels[] = {"warning: ", "error: ", "critical: "};
    for (size_t i = 0; i < sizeof levels / sizeof *levels; i++)
        if (!strncmp(p, levels[i], strlen(levels[i]))) {
            p += strlen(levels[i]);
            break;
        }
    return !strncmp(p, "coop:", 5) || !strncmp(p, "coop voice:", 11);
}
#endif
