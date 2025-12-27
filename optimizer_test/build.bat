@echo off
REM Windows批处理脚本，用于快速编译和运行测试程序

echo ========================================
echo SQL优化器测试程序 - Windows构建脚本
echo ========================================

REM 检查是否存在build目录
if not exist build mkdir build
cd build

REM 使用CMake生成构建文件
echo.
echo [1/3] 生成CMake构建文件...
cmake .. || (
    echo CMake失败，尝试直接编译...
    cd ..
    goto :direct_compile
)

REM 编译
echo.
echo [2/3] 编译项目...
cmake --build . || (
    echo CMake构建失败，尝试直接编译...
    cd ..
    goto :direct_compile
)

REM 运行
echo.
echo [3/3] 运行测试程序...
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
    echo 找不到可执行文件！
    cd ..
    exit /b 1
)

cd ..
goto :end

:direct_compile
echo.
echo 使用直接编译方式...
if exist optimizer_test.cpp (
    g++ -std=c++11 optimizer_test.cpp -o optimizer_test.exe
    if exist optimizer_test.exe (
        echo.
        echo 运行测试程序...
        echo.
        optimizer_test.exe
    ) else (
        echo 编译失败！请确保已安装MinGW或MSVC编译器。
    )
) else (
    echo 找不到optimizer_test.cpp文件！
)

:end
echo.
echo 完成！
pause

