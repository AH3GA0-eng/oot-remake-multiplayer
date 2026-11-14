@echo off
rem Goes back to the version this kit had before its last update (docs\SETUP.md, "Going back to an older version").
rem It runs Update.bat -Revert. Update.bat -Version v0.10.1 installs any older release instead.
echo Revert.bat puts back the version this kit had before its last update.
echo.
echo   Every player in a session needs the SAME version: the versions do not talk to each other.
echo   Tell your friends to go back too (their own Revert.bat), or you cannot play together.
echo.
echo   Your saves, keys, settings and coop-options.ini are kept, and so is this version:
echo   Update.bat goes forward again. Starting the kit does not update back by itself.
echo.
if not exist "%~dp0Update.bat" (echo Update.bat is missing from this kit: get it from https://github.com/daurosk/oot-remake-multiplayer/releases and put it beside this file.& pause & exit /b 1) else call "%~dp0Update.bat" -Revert %* & (if not errorlevel 1 pause) & exit /b 0
