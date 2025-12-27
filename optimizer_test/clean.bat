@echo off
echo Cleaning build directory...

if exist build\CMakeCache.txt (
    del /Q build\CMakeCache.txt
    echo Deleted CMakeCache.txt
)

if exist build\CMakeFiles (
    rmdir /S /Q build\CMakeFiles
    echo Deleted CMakeFiles directory
)

if exist build\bin (
    echo Note: bin directory still exists (contains compiled executables)
)

if exist build\*.exe (
    del /Q build\*.exe
    echo Deleted .exe files
)

if exist build\*.o (
    del /Q build\*.o
    echo Deleted .o files
)

echo.
echo Clean complete!
echo You can now run build.bat or cmake again.

