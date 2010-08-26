@ECHO OFF

SET BOOST_VERSION=1_40
SET BOOST_DIR=%BUILD_TOOLS_DIR%\Boost\%BOOST_VERSION%

ECHO %LIB% | FIND "%BOOST_DIR%" > NUL && GOTO Quit

SET BOOSTLIB=%BOOST_DIR%\lib%1%
SET LIB=%BOOSTLIB%;%LIB%
SET INCLUDE=%BOOST_DIR%;%INCLUDE%
SET BOOSTLIB=
SET PATH=%TOOLS_DIR%;%PATH%
SET TOOLS_DIR=
:Quit
