xcopy /Y /I ..\win32-g++-32 qtbase\mkspecs\win32-g++-32
xcopy /Y /I ..\win32-g++-64 qtbase\mkspecs\win32-g++-64
xcopy /Y ..\win32-msvc2008\qmake.conf qtbase\mkspecs\win32-msvc2008\qmake.conf

set TARGETS=-nomake demos -nomake examples -nomake docs -nomake translations -no-vcproj -no-incredibuild-xge -no-ltcg
set STYLES=-qt-style-windows -qt-style-windowsxp -qt-style-windowsvista
set VERSIONS=-opensource -debug-and-release -static -confirm-license -no-c++11
set FORMATS=-no-gif -no-libjpeg -no-freetype
set FEATURES=-no-accessibility -no-native-gestures -no-opengl -no-openvg -no-openssl
set PARTS=-no-nis -no-iconv -no-icu -no-angle -qt-zlib
configure %TARGETS% %STYLES% %VERSIONS% %FORMATS% %FEATURES% %PARTS% %1 %2 %3 %4 %5

