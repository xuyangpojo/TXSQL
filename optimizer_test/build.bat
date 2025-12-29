@echo off
setlocal enabledelayedexpansion

echo ========================================
echo SQL Optimizer Test - Windows Build Script
echo ========================================
echo.

if not exist build mkdir build
if %ERRORLEVEL% NEQ 0 (
    echo Error: Cannot create build directory!
    pause
    exit /b 1
)
cd build
if %ERRORLEVEL% NEQ 0 (
    echo Error: Cannot change to build directory!
    pause
    exit /b 1
)

echo.
echo [1/3] Generating CMake build files...
echo.

where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using MinGW (g++) compiler...
    if exist CMakeCache.txt del /Q CMakeCache.txt >nul 2>&1
    if exist CMakeFiles rmdir /S /Q CMakeFiles >nul 2>&1
    cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ .. || (
        echo CMake failed, trying direct compilation...
        cd ..
        goto :direct_compile
    )
) else (
    if exist CMakeCache.txt del /Q CMakeCache.txt >nul 2>&1
    if exist CMakeFiles rmdir /S /Q CMakeFiles >nul 2>&1
    cmake .. || (
        echo CMake failed, trying direct compilation...
        cd ..
        goto :direct_compile
    )
)

echo.
echo [2/3] Building project...
cmake --build . || (
    echo CMake build failed, trying direct compilation...
    cd ..
    goto :direct_compile
)

echo.
echo [3/3] Running test program...
echo.
if exist bin\optimizer_test.exe (
    bin\optimizer_test.exe
) else if exist Debug\optimizer_test.exe (
    Debug\optimizer_test.exe
) else if exist Release\optimizer_test.exe (
    Release\optimizer_test.exe
) else if exist optimizer_test.exe (
    optimizer_test.exe
) else (
    echo Error: Cannot find executable file!
    cd ..
    exit /b 1
)

cd ..
goto :end

:direct_compile
echo.
echo Using direct compilation method...
echo.

where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: g++ compiler not found!
    echo Please install MinGW-w64 or configure Visual Studio.
    cd ..
    pause
    exit /b 1
)

if exist test_cases.cpp (
    echo Compiling optimizer.cpp...
    g++ -std=c++11 -c optimizer.cpp -o optimizer.o
    if %ERRORLEVEL% NEQ 0 (
        echo Compilation failed!
        cd ..
        pause
        exit /b 1
    )
    
    echo Compiling test_cases.cpp...
    g++ -std=c++11 test_cases.cpp optimizer.o -o optimizer_test.exe
    if exist optimizer_test.exe (
        echo.
        echo Running test program...
        echo.
        optimizer_test.exe
    ) else (
        echo Compilation failed! Please check your compiler installation.
    )
) else (
    echo Error: test_cases.cpp not found!
)

:end
echo.
echo ========================================
echo Build process completed!
echo ========================================
echo.
echo If you encountered errors, please check MANUAL_BUILD.md
echo for manual build instructions.
echo.
pause