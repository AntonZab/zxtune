@echo off

SET VS_PATH=%PROGRAMFILES%\Microsoft Visual Studio 9.0
ECHO %PATH% | FIND "%VS_PATH%" > NUL && GOTO Quit
call "%VS_PATH%\VC\vcvarsall.bat" x86

:Quit
SET INSTPATH=E:\Build\qt-5.1.0-windows-x86
mkdir %INSTPATH%

SET QMAKEPATH=%CD%
..\build.bat -platform win32-msvc2008 -prefix %INSTPATH% -mp
