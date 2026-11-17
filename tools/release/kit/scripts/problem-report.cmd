@echo off
rem Makes a problem report now: the co-op problems in your game logs since the last report, masked as collect-logs
rem masks them, in coop-report-<date>.zip in the kit folder. Send it with /report in the Squadala! Discord.
rem Nothing is sent from here (docs\problem-reports.md).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0problem-report.ps1" -Now %*
pause
