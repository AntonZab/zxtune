@echo off

SET VS_PATH=%PROGRAMFILES%\Microsoft Visual Studio 9.0
ECHO %PATH% | FIND "%VS_PATH%" > NUL && GOTO Quit
call "%VS_PATH%\VC\bin\vcvars64.bat"

:Quit
SET INSTPATH=E:\Build\qt-5.1.0-windows-x86_64
mkdir %INSTPATH%

SET QMAKEPATH=%CD%
..\build.bat -platform win32-msvc2008 -prefix %INSTPATH% -mp
