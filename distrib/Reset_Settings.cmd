@echo off
echo.
echo.
title Restore VSFilter default settings...
REM start /min reg delete "HKEY_CURRENT_USER\Software\MPC-BE Filters\MPC Image Source" /f
echo    settings were reset to default
echo.
pause >NUL
