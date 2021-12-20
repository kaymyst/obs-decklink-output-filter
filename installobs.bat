set OBSBIN_VER=26.1.1
set OBSSRC_VER=26.1.2
set WINDOWS_DEPS_VERSION=2019

curl -o obs-bin-x86.zip "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-%OBSBIN_VER%-Full-x86.zip"
curl -o obs-bin-x64.zip "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-%OBSBIN_VER%-Full-x64.zip"
git clone --depth=1 -b %OBSSRC_VER% "https://github.com/obsproject/obs-studio.git" obs-src
call obs-src\CI\install-qt-win.cmd
cmake -E make_directory obs-bin
pushd obs-bin
cmake -E tar zxf ..\obs-bin-x86.zip
cmake -E tar zxf ..\obs-bin-x64.zip
popd

curl -kLO https://cdn-fastly.obsproject.com/downloads/dependencies%WINDOWS_DEPS_VERSION%.zip -f --retry 5 -C -
"C:\Program Files\7-Zip\7z" x dependencies%WINDOWS_DEPS_VERSION%.zip -o"cmbuild/deps"

set QTDIR32=-DQTDIR="C:/QtDep/5.15.2/msvc2019"
set QTDIR64=-DQTDIR="C:/QtDep/5.15.2/msvc2019_64"

cd obs-src
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION="10.0.19041.0" -DDepsPath="C:\Users\stephanel\source\repos\obs-decklink-output-filter\cmbuild\deps\win64" %QTDIR64% -DDISABLE_PLUGINS=TRUE -DCOPIED_DEPENDENCIES=FALSE -DCOPY_DEPENDENCIES=TRUE
msbuild /m /p:Configuration=RelWithDebInfo C:\Users\stephanel\source\repos\obs-decklink-output-filter\obs-src\build\obs-studio.sln
