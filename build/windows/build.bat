@echo off

:: Install build dependencies
call %~dp0ci-common.bat
if errorlevel 1 exit /b %errorlevel%

:: Ensure that the necessary build tools are on the PATH
call %~dp0validate-build-dependencies.bat
if errorlevel 1 exit /b %errorlevel%

set PATH=%PATH%;C:\Program Files (x86)\Windows Kits\8.1\bin\x64
set PATH=%PATH%;C:\Program Files (x86)\Windows Kits\10\bin\x64

echo DEBUG: Path=
set PATH

ninja --version
clang-cl --version
lld-link --version
cmake --version
rc /?

cd github\filament

mkdir out\cmake-release
cd out\cmake-release
if errorlevel 1 exit /b %errorlevel%

cmake ..\.. -G Ninja ^
    -DCMAKE_CXX_COMPILER:PATH="clang-cl.exe" ^
    -DCMAKE_C_COMPILER:PATH="clang-cl.exe" ^
    -DCMAKE_LINKER:PATH="lld-link.exe" ^
    -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%

ninja
if errorlevel 1 exit /b %errorlevel%

ninja install
if errorlevel 1 exit /b %errorlevel%

:: Create an archive
dir .\install
where 7z
7z a -ttar -so ..\filament-release.tar .\install\* | 7z a -si ..\filament.tgz

echo Done!
