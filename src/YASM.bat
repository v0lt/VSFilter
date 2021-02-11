IF EXIST "%~dp0..\environments.bat" CALL "%~dp0..\environments.bat"

IF DEFINED YASM_PATH (
%YASM_PATH% %*
) ELSE (
yasm.exe %*
)
