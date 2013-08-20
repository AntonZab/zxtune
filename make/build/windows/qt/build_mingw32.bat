@echo off

SET MINGW_PATH=%CD:~0,2%\Build\MinGW-4.8.1\bin
ECHO %PATH% | FIND "%MINGW_PATH%" > NUL && GOTO Quit

::disable msvc
SET INCLUDE=
SET LIB=
SET PATH=%MINGW_PATH%;%PATH%

:Quit
SET INSTPATH=E:\Build\qt-5.1.0-mingw-x86
mkdir %INSTPATH%

..\build.bat -xplatform win32-g++-32 -prefix %INSTPATH% 
