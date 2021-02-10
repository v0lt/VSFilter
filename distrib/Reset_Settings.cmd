@echo off
echo.
echo.
title Restore VSFilter default settings...
start /min reg delete "HKEY_CURRENT_USER\Software\Gabest\VSFilter" /f
echo    settings were reset to default
echo.
pause >NUL