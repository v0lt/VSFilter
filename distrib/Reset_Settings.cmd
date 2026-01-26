@echo off
echo.
echo.
title Restore VSFilterBE default settings...
start /min reg delete "HKEY_CURRENT_USER\SOFTWARE\MPC-BE Filters\VSFilter" /f
echo    settings were reset to default
echo.
pause >NUL