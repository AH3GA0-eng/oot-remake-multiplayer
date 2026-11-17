"""Compiles the Longshot files our patches touch, syntax only, with Longshot's own compiler and build.py's own flags.

Run by Longshot's Python on Longshot's source with our patches applied (tools/hookshot/host_compile_check.ps1 on Windows,
tools/hookshot/host_compile_check.sh on Linux):

    python -I host_compile_check.py <patched source> <hookshot/patches folder>

nx2/build.py --standalone --dry-run is run in this process (no game needed: it stops where the compile would start), and
its list of compiles is taken from it at that point: each job's compiler, source and flags, exactly as a game build would
run them on this system. Of those, the files a patch changes and the files that include a header a patch changes are
compiled again here with -fsyntax-only (host/*.c, host/posix/*.c on Linux, gpu/vk/*.cpp). Unknown escape sequences and
multi-character constants are errors (a lost backslash makes either). Then every .py a patch changes is compiled
(py_compile). A patched C or C++ file the dry run would not compile on this system is listed as such (host/posix on
Windows, display.c on Linux), never silently left out.

Exit status 0 only when every file compiled. Needs the toolchain environment of a kit build (Windows: LONGSHOT_MINGW and
RX_CPM; Linux: linux_tools.py env) and OOT_WORK (an empty work folder).
"""
import builtins
import os
import py_compile
import re
import subprocess
import sys
import tempfile

EXTRA = ["-Wunknown-escape-sequence", "-Werror=unknown-escape-sequence", "-Wmultichar", "-Werror=multichar"]
C_EXT = (".c", ".cpp", ".cc")
H_EXT = (".h", ".hpp", ".inc")


class Captured(Exception):
    pass


def dry_run_jobs(src):
    """build.py's compile jobs (cc, source, object, flags, deps) for a --standalone build on this system"""
    nx2 = os.path.join(src, "nx2")
    sys.path.insert(0, nx2)
    saved = sys.argv
    sys.argv = [os.path.join(nx2, "build.py"), "--standalone", "--dry-run"]
    try:
        import build
        got = {}

        def capture(*args, **kw):
            # (build.py's dry run says "dry run: N files to compile ..." with its jobs in the caller's locals)
            if args and isinstance(args[0], str) and args[0].startswith("dry run: ") and "files to compile" in args[0]:
                got["jobs"] = list(sys._getframe(1).f_locals["jobs"])
                raise Captured()
            builtins.print(*args, **kw)

        build.print = capture
        try:
            status = build.main()
        except Captured:
            return got["jobs"]
        raise SystemExit("host_compile_check: build.py --dry-run ended (status %r) before its list of compiles" % (status,))
    finally:
        sys.argv = saved


def touched_files(patches):
    out = set()
    for name in sorted(os.listdir(patches)):
        if name.endswith(".patch"):
            with open(os.path.join(patches, name), encoding="utf-8", errors="replace") as f:
                for line in f:
                    m = re.match(r"\+\+\+ b/(\S+)", line)
                    if m:
                        out.add(m.group(1))
    return out


def main():
    src, patches = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    touched = touched_files(patches)
    jobs = dry_run_jobs(src)
    rel = lambda p: os.path.relpath(os.path.abspath(p), src).replace(os.sep, "/")
    headers = {os.path.basename(t) for t in touched if t.endswith(H_EXT)}
    inc = re.compile(r'#\s*include\s*"(?:[^"]*/)?([^"/]+)"')
    picked, seen = [], set()
    for job in jobs:
        cc, path, _obj, flags = job[0], job[1], job[2], job[3]
        r = rel(path)
        if r in seen or not r.endswith(C_EXT) or not (r.startswith("host/") or r.startswith("gpu/vk/")) or not os.path.isfile(path):
            continue
        why = "patched" if r in touched else ""
        if not why and headers:
            with open(path, encoding="utf-8", errors="replace") as f:
                names = set(inc.findall(f.read()))
            hit = sorted(names & headers)
            if hit:
                why = "includes " + ", ".join(hit)
        if why:
            seen.add(r)
            picked.append((r, why, cc, path, flags))
    failed = []
    for r, why, cc, path, flags in sorted(picked):
        # (the job's own flags; -c/-o and dependency-file options are not in them: compile_one adds those)
        cmd = [cc] + list(flags) + ["-fsyntax-only"] + EXTRA + [path]
        p = subprocess.run(cmd, cwd=os.path.join(src, "nx2"), capture_output=True, text=True, errors="replace")
        if p.returncode:
            failed.append(r)
            print("FAIL %s (%s)\n%s" % (r, why, (p.stdout + p.stderr)[-4000:]), flush=True)
        else:
            print("ok   %s (%s)" % (r, why), flush=True)
    for t in sorted(touched):
        if t.endswith(C_EXT) and t not in seen:
            print("not compiled on this system (build.py has no job for it here): %s" % t, flush=True)
    with tempfile.TemporaryDirectory() as tmp:
        for t in sorted(x for x in touched if x.endswith(".py")):
            try:
                py_compile.compile(os.path.join(src, t), cfile=os.path.join(tmp, "x.pyc"), doraise=True)
                print("ok   %s" % t, flush=True)
            except py_compile.PyCompileError as e:
                failed.append(t)
                print("FAIL %s\n%s" % (t, e.msg), flush=True)
    if not picked:
        print("host_compile_check: no file to compile was found (the dry run's list had %d jobs)" % len(jobs))
        return 1
    if failed:
        print("host_compile_check: %d file(s) did not compile: %s" % (len(failed), ", ".join(failed)))
        return 1
    print("host_compile_check: %d C/C++ file(s) and the patched Python files compile" % len(picked))
    return 0


if __name__ == "__main__":
    sys.exit(main())
