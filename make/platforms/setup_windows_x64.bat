@echo off

SET VS_PATH=%PROGRAMFILES%\Microsoft Visual Studio 9.0
ECHO %PATH% | FIND "%VS_PATH%" > NUL && GOTO SetDx
::vc9.0 has broken vcvarsall for x64
call "%VS_PATH%\VC\bin\vcvars64.bat"

:SetDx

SET | FIND "DXSDK_DIR" > NUL || GOTO SetMode
ECHO %INCLUDE% | FIND "%DXSDK_DIR%" > NUL && GOTO SetMode
SET INCLUDE=%INCLUDE%;%DXSDK_DIR%\Include

:SetMode
SET platform=windows
SET arch=x86_64
