@echo off
rem Goes back to the version this kit had before its last update (docs\SETUP.md, "Going back to an older version").
rem Your saves, keys, settings and coop-options.ini are kept; Update.bat goes forward again. Every player in a
rem session needs the same version, so tell your friends. It runs Update.bat -Revert; Update.bat -Version v0.10.1
rem installs any older release instead. One line: cmd reads a .bat as it runs.
if not exist "%~dp0Update.bat" (echo Update.bat is missing from this kit: get it from https://github.com/daurosk/oot-remake-multiplayer/releases and put it beside this file.& pause & exit /b 1) else call "%~dp0Update.bat" -Revert %* & (if not errorlevel 1 pause) & exit /b 0
