# SQL优化器独立测试程序

这是一个简化的SQL优化器测试程序，可以在Windows上独立运行，不需要编译整个MySQL项目。

## 功能

该程序模拟了MySQL SQL优化器的核心功能：

1. **访问路径选择**
   - 全表扫描 vs 索引扫描
   - 根据成本选择最佳访问方式

2. **连接顺序优化**
   - 使用贪心算法选择最优连接顺序
   - 考虑表大小和连接条件

3. **成本估算**
   - 磁盘I/O成本
   - CPU计算成本
   - 基于统计信息的行数估算

## 编译方法

### 方法1: 使用CMake（推荐）

```bash
# 创建构建目录
mkdir build
cd build

# 生成构建文件
cmake ..

# 编译
cmake --build .

# 运行
./bin/optimizer_test.exe  # Windows
# 或
./optimizer_test  # Linux/Mac
```

### 方法2: 直接使用编译器

**Windows (MSVC):**
```cmd
cl /EHsc /std:c++11 optimizer_test.cpp /Fe:optimizer_test.exe
optimizer_test.exe
```

**Windows (MinGW):**
```bash
g++ -std=c++11 optimizer_test.cpp -o optimizer_test.exe
./optimizer_test.exe
```

**Linux/Mac:**
```bash
g++ -std=c++11 optimizer_test.cpp -o optimizer_test
./optimizer_test
```

## 测试用例

程序包含三个测试用例：

### 测试1: 单表查询
- 测试索引选择
- 比较全表扫描和索引扫描的成本

### 测试2: 两表连接
- 测试连接顺序优化
- 从小表到大表的连接策略

### 测试3: 三表连接
- 测试多表连接顺序
- 优化器会选择成本最低的连接顺序

## 输出说明

程序会输出：
- 总成本（越小越好）
- 连接类型
- 每个表的访问方式
- 估计行数
- 各步骤的成本

## 扩展

你可以修改代码来：
1. 添加更多测试用例
2. 调整成本模型参数
3. 实现更复杂的优化算法（如动态规划）
4. 添加更多访问路径类型（如哈希连接）

## 注意事项

这是一个**简化版本**，用于演示优化器的基本概念。真实的MySQL优化器要复杂得多，包括：
- 更复杂的成本模型
- 更多优化策略
- 统计信息收集
- 查询重写
- 子查询优化
等等。

## 依赖

- C++11或更高版本
- CMake 3.10+（如果使用CMake）
- 任何支持C++11的编译器（MSVC, GCC, Clang等）

