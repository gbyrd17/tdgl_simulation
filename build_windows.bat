@echo off
REM Build script for Windows using vcpkg and CMake

echo ============================================
echo TDGL Simulation - Windows Build Script
echo ============================================
echo.

REM Check if vcpkg is installed
if not defined VCPKG_ROOT (
    echo Error: VCPKG_ROOT environment variable is not set.
    echo Please install vcpkg and set VCPKG_ROOT to the installation directory.
    echo.
    echo Example:
    echo   git clone https://github.com/microsoft/vcpkg.git
    echo   cd vcpkg
    echo   bootstrap-vcpkg.bat
    echo   setx VCPKG_ROOT "%CD%"
    echo.
    pause
    exit /b 1
)

echo Using vcpkg from: %VCPKG_ROOT%
echo.

REM Create build directory
if not exist "build" mkdir build
cd build

echo Running CMake configuration...
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" ^
         -DCMAKE_BUILD_TYPE=Release  ^
         -G "Visual Studio 16 2019" 

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ============================================
echo Build completed successfully!
echo Executable: build\Release\tdgl_simulation.exe
echo ============================================
echo.
cd ..
pause
