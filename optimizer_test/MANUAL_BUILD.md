# 手动构建命令列表

如果 `build.bat` 无法正常工作，可以按照以下步骤手动执行：

## 方法1：直接编译（最简单，推荐）

**重要：必须在项目根目录执行，不是在 build 目录！**

打开命令提示符（CMD）或 PowerShell：

```bash
# 1. 进入项目根目录（不是 build 目录！）
cd C:\projects\TXSQL\optimizer_test

# 2. 确认你在正确的目录（应该能看到 optimizer.cpp, test_cases.cpp 等文件）
dir *.cpp

# 3. 检查编译器是否可用
g++ --version

# 4. 编译 optimizer.cpp 为目标文件
g++ -std=c++11 -c optimizer.cpp -o optimizer.o

# 5. 编译并链接测试程序
g++ -std=c++11 test_cases.cpp optimizer.o -o optimizer_test.exe

# 6. 运行测试程序
optimizer_test.exe
```

**注意：** 如果你当前在 `build` 目录中，需要先返回上一级：
```bash
cd ..
```

## 方法2：使用 CMake（如果方法1失败）

```bash
# 1. 进入项目目录
cd C:\projects\TXSQL\optimizer_test

# 2. 创建构建目录
mkdir build
cd build

# 3. 清理旧的 CMake 缓存（如果存在）
del CMakeCache.txt 2>nul
rmdir /S /Q CMakeFiles 2>nul

# 4. 生成构建文件（使用 MinGW）
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..

# 5. 编译项目
cmake --build .

# 6. 运行测试程序
bin\optimizer_test.exe
```

## 方法3：使用 Visual Studio（如果安装了 VS）

```bash
# 1. 进入项目目录
cd C:\projects\TXSQL\optimizer_test

# 2. 创建构建目录
mkdir build
cd build

# 3. 生成 Visual Studio 项目文件
cmake ..

# 4. 编译项目
cmake --build . --config Release

# 5. 运行测试程序
Release\optimizer_test.exe
```

## 常见问题解决

### 问题1：找不到 g++ 命令

**解决方案：**
- 确保 MinGW-w64 已安装并添加到 PATH
- 检查 PATH 环境变量：`echo %PATH%`
- 或使用完整路径：`C:\MinGW\bin\g++.exe`

### 问题2：CMake 找不到编译器

**解决方案：**
- 删除 build 目录中的 `CMakeCache.txt`
- 或运行：`clean.bat`
- 然后重新运行 CMake，明确指定编译器

### 问题3：编译错误

**检查：**
- 确保所有源文件都在当前目录
- 检查文件：`optimizer.h`, `optimizer.cpp`, `test_cases.cpp`
- 确保编译器支持 C++11 标准

## 快速验证

运行以下命令验证环境：

```bash
# 检查 CMake
cmake --version

# 检查 C++ 编译器
g++ --version

# 检查文件是否存在
dir optimizer.h optimizer.cpp test_cases.cpp
```

## 一键命令（复制粘贴执行）

**最简单的方式（推荐）：**

```bash
cd C:\projects\TXSQL\optimizer_test && g++ -std=c++11 -c optimizer.cpp -o optimizer.o && g++ -std=c++11 test_cases.cpp optimizer.o -o optimizer_test.exe && optimizer_test.exe
```

