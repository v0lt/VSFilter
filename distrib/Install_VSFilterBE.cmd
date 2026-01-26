@cd /d "%~dp0"
@regsvr32.exe "%~dp0/VSFilterBE.dll" /s
@if %errorlevel% NEQ 0 goto error
:success
@echo.
@echo.
@echo    Installation succeeded.
@echo.
@echo    Please do not delete the VSFilterBE.dll file.
@echo    The installer has not copied the files anywhere.
@echo.
@goto done
:error
@echo.
@echo.
@echo    Installation failed.
@echo.
@echo    You need to right click "Install_VSFilterBE.cmd" and choose "run as admin".
@echo.
:done
@pause >NUL
