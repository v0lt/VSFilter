IF EXIST "%~dp0..\environments.bat" CALL "%~dp0..\environments.bat"

IF DEFINED MPCBE_YASM_PATH SET PATH=%PATH%;%MPCBE_YASM_PATH%

yasm.exe %*
