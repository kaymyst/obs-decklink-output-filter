echo y | rd /s dist
del release.zip

set QTDIR32=-DQTDIR="C:/QtDep/5.15.2/msvc2019"
set QTDIR64=-DQTDIR="C:/QtDep/5.15.2/msvc2019_64"
set LIBOBS_INCLUDE_DIR=-DLIBOBS_INCLUDE_DIR="obs-src\libobs"
set LIBOBS_LIB32=-DLIBOBS_LIB="build_x86\libobs.lib"
set OBS_FRONTEND_LIB32=-DOBS_FRONTEND_LIB="build_x86\obs-frontend-api.lib"


set LIBOBS_LIB64=-DLIBOBS_LIB="C:\Users\stephanel\source\repos\obs-decklink-output-filter\obs-src\build\libobs\RelWithDebInfo\libobs.lib"
set OBS_FRONTEND_LIB64=-DOBS_FRONTEND_LIB="C:\Users\stephanel\source\repos\obs-decklink-output-filter\obs-src\build\UI\obs-frontend-api\RelWithDebInfo\obs-frontend-api.lib"
set DLibObs_DIR64=-DLibObs_DIR="C:\Users\stephanel\source\repos\obs-decklink-output-filter\obs-src\build\libobs"

cmake %QTDIR64% -G "Visual Studio 16 2019" -A x64 -B build_x64 -S . -DCMAKE_INSTALL_PREFIX=dist %LIBOBS_INCLUDE_DIR% %LIBOBS_LIB64% %OBS_FRONTEND_LIB64% %DLibObs_DIR64%
cmake --build build_x64 --config RelWithDebInfo
cmake --install build_x64 --config RelWithDebInfo
