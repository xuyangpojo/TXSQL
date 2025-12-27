# SQL优化器测试项目

本项目实现了SQL查询优化器的核心功能，包括成本模型、访问路径选择和连接顺序优化。基于TXSQL项目中的优化器逻辑，提取核心算法思想，可以独立编译运行。
C:\projects\TXSQL\optimizer_test
## 项目结构

```
optimizer_test/
├── optimizer.h              # 优化器核心头文件
├── optimizer.cpp            # 优化器核心实现
├── test_cases.cpp           # 测试用例
├── test_data_generator.cpp  # 测试数据生成器
├── CMakeLists.txt          # CMake构建配置
├── build.bat               # Windows构建脚本
└── README.md              # 本文件
```

## 输入输出

### 输入

优化器通过程序接口接收以下输入：

1. **表统计信息 (TableStats)**
   - 表名
   - 行数
   - 平均行大小（字节）
   - 内存页比例 (0.0-1.0)
   - 列基数（不同值的数量）

2. **索引统计信息 (IndexStats)**
   - 索引名
   - 所属表名
   - 索引列
   - 是否唯一/主键
   - 索引页内存比例
   - 选择性

3. **查询条件 (QueryCondition)**
   - 表名
   - 列名
   - 操作符（=, >, <, BETWEEN, IN等）
   - 条件选择性（满足条件的行比例）

4. **连接条件 (JoinCondition)**
   - 左表/右表名
   - 左列/右列名
   - 连接选择性

### 输出

优化器输出执行计划，包括：

1. **总成本** - 执行计划的总体成本估算
2. **连接类型** - 使用的连接算法（NESTED_LOOP等）
3. **表访问顺序** - 优化后的表连接顺序
4. **访问路径** - 每个表的访问方式（全表扫描/索引扫描/索引查找）
5. **估计行数** - 每个步骤估计处理的行数
6. **性能指标** - 优化时间、成本分解等

## 示例输入输出

### 示例1：单表查询

**输入：**
```cpp
TableStats orders;
orders.name = "orders";
orders.row_count = 1000000;
orders.avg_row_size = 200;
orders.pages_in_memory = 0.3;

IndexStats idx_customer;
idx_customer.name = "idx_customer_id";
idx_customer.table_name = "orders";
idx_customer.columns = {"customer_id"};
idx_customer.selectivity = 1.0 / 50000.0;

QueryCondition cond;
cond.table_name = "orders";
cond.column_name = "customer_id";
cond.op = "=";
cond.selectivity = 1.0 / 50000.0;
```

**输出：**
```
========== 测试1: 单表查询（索引 vs 全表扫描） ==========
总成本: 240.50
连接类型: NONE
优化时间: 0.125 ms

表访问顺序:
  1. orders
     访问方式: 索引查找: idx_customer_id
     估计行数: 20
     成本: 240.50
```

### 示例2：两表连接

**输入：**
```cpp
// orders表：100万行
// customers表：5万行
// 连接条件：orders.customer_id = customers.customer_id
// 查询条件：orders.order_date > '2023-01-01' (选择性0.3)
```

**输出：**
```
========== 测试2: 两表连接（测试连接顺序） ==========
总成本: 150000.25
连接类型: NESTED_LOOP
优化时间: 0.234 ms

表访问顺序:
  1. customers
     访问方式: 全表扫描
     估计行数: 50000
     成本: 12500.00

  2. orders
     访问方式: 全表扫描
     估计行数: 300000
     成本: 137500.25
```

### 示例3：多表连接

**输入：**
```cpp
// 4个表：orders, customers, order_items, products
// 多个连接条件
```

**输出：**
```
========== 测试3: 多表连接（4表） ==========
总成本: 2500000.50
连接类型: NESTED_LOOP
优化时间: 0.456 ms

表访问顺序:
  1. products
     访问方式: 全表扫描
     估计行数: 10000
     成本: 2500.00

  2. customers
     访问方式: 全表扫描
     估计行数: 50000
     成本: 12500.00

  3. orders
     访问方式: 全表扫描
     估计行数: 1000000
     成本: 250000.00

  4. order_items
     访问方式: 全表扫描
     估计行数: 5000000
     成本: 2225000.50
```

## 环境要求

### 必需软件

1. **CMake** (版本 3.10 或更高)
   - 下载地址：https://cmake.org/download/
   - 安装时选择"Add CMake to system PATH"

2. **C++编译器**（支持C++11标准）

   **Windows选项：**
   - **MinGW-w64** (推荐，轻量级)
     - 下载地址：https://www.mingw-w64.org/downloads/
     - 或使用 MSYS2：https://www.msys2.org/
     - 确保 `g++` 命令可用
   - **Visual Studio** (2015或更高版本)
     - 下载地址：https://visualstudio.microsoft.com/
     - 安装时选择"使用C++的桌面开发"工作负载
     - 包含 MSVC 编译器

   **Linux/Mac选项：**
   - **GCC** (4.8或更高) 或 **Clang** (3.3或更高)
   - 通常系统已预装，可通过包管理器安装：
     ```bash
     # Ubuntu/Debian
     sudo apt-get install build-essential
     
     # CentOS/RHEL
     sudo yum install gcc-c++
     
     # macOS (使用Homebrew)
     brew install gcc
     ```

### 验证安装

**检查CMake：**
```bash
cmake --version
```

**检查C++编译器：**
```bash
# Windows (MinGW)
g++ --version

# Windows (Visual Studio)
cl

# Linux/Mac
g++ --version
# 或
clang++ --version
```

### 项目依赖

本项目**只使用C++标准库**，无需安装其他第三方库：
- `<iostream>`, `<vector>`, `<string>`, `<map>`
- `<algorithm>`, `<cmath>`, `<chrono>`, `<fstream>`

## 项目编译启动方法

### Windows环境

#### 方法1：使用构建脚本（推荐）

```bash
# 直接运行构建脚本
build.bat
```

脚本会自动：
1. 创建build目录
2. 运行CMake生成构建文件
3. 编译项目
4. 运行测试程序

#### 方法2：使用CMake手动构建

**如果使用MinGW编译器：**
```bash
# 创建构建目录
mkdir build
cd build

# 生成构建文件（指定MinGW）
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..

# 编译
cmake --build .

# 运行测试
bin\optimizer_test.exe
```

**如果使用Visual Studio：**
```bash
# 创建构建目录
mkdir build
cd build

# 生成构建文件（Visual Studio会自动检测）
cmake ..

# 编译
cmake --build .

# 运行测试
bin\optimizer_test.exe
```

**注意：** 如果遇到编译器检测错误，可以：
1. 删除 `build` 目录中的 `CMakeCache.txt` 文件
2. 或运行 `clean.bat` 清理构建缓存
3. 或使用 `build.bat` 脚本（会自动处理）

#### 方法3：直接编译（无CMake）

```bash
# 编译优化器库
g++ -std=c++11 -c optimizer.cpp -o optimizer.o

# 编译测试程序
g++ -std=c++11 test_cases.cpp optimizer.o -o optimizer_test.exe

# 运行
optimizer_test.exe
```

### Linux/Mac环境

```bash
# 创建构建目录
mkdir build
cd build

# 生成构建文件
cmake ..

# 编译
make

# 运行测试
./optimizer_test
```

### 生成测试数据

```bash
# 编译数据生成器
g++ -std=c++11 test_data_generator.cpp -o test_data_generator.exe

# 运行（使用默认规模）
test_data_generator.exe

# 或指定数据规模
test_data_generator.exe 50000 1000000 10000 5000000
```

## 测试结果收集方法

### 1. 控制台输出

程序运行时会直接在控制台输出：
- 每个测试用例的执行计划
- 性能统计摘要
- 优化时间、成本等信息

### 2. CSV文件输出

程序会自动生成 `optimizer_performance.csv` 文件，包含以下列：

| 列名 | 说明 |
|------|------|
| 优化时间(ms) | 优化器执行时间（毫秒） |
| 总成本 | 执行计划总成本 |
| 估计行数 | 估计处理的总行数 |
| 表数 | 查询涉及的表数量 |
| 使用索引数 | 使用的索引数量 |

**示例CSV内容：**
```csv
优化时间(ms),总成本,估计行数,表数,使用索引数
0.125,240.50,20,1,1
0.234,150000.25,350000,2,0
0.456,2500000.50,6060000,4,0
```

### 3. 性能分析

可以使用以下工具分析CSV文件：

**Python示例：**
```python
import pandas as pd
import matplotlib.pyplot as plt

# 读取数据
df = pd.read_csv('optimizer_performance.csv')

# 统计分析
print(df.describe())

# 可视化
df.plot(x='表数', y='总成本', kind='scatter')
plt.show()
```

**Excel：**
- 直接打开CSV文件进行数据分析
- 创建图表对比不同测试场景

### 4. 自定义性能收集

可以在测试代码中添加自定义的性能指标收集：

```cpp
PerformanceMetrics metrics;
metrics.optimization_time_ms = time_ms;
metrics.total_cost = plan.total_cost;
// ... 添加更多指标
collector.record(metrics);
```

## 后续优化方向

### 1. 算法优化

- **动态规划连接顺序优化**
  - 当前使用贪心算法，可以改进为动态规划算法
  - 支持更优的连接顺序选择
  - 适用于表数量较多（>10）的场景

- **索引选择优化**
  - 支持复合索引
  - 索引覆盖扫描优化
  - 索引合并（Index Merge）支持

- **连接算法扩展**
  - 支持Hash Join成本估算
  - 支持Sort-Merge Join
  - 根据数据特征自动选择连接算法

### 2. 成本模型改进

- **更精确的统计信息**
  - 直方图（Histogram）支持
  - 相关性统计
  - 数据倾斜检测

- **存储引擎特性**
  - 不同存储引擎的成本模型
  - 缓存命中率估算
  - 预读（Prefetch）成本

- **并行执行成本**
  - 多线程执行成本
  - 并行扫描成本
  - 并行连接成本

### 3. 功能扩展

- **子查询优化**
  - 子查询转连接
  - 相关子查询优化
  - EXISTS/IN优化

- **聚合优化**
  - GROUP BY优化
  - 聚合下推
  - 聚合索引利用

- **分区表支持**
  - 分区裁剪
  - 分区连接优化

### 4. 性能优化

- **优化器本身性能**
  - 减少不必要的成本计算
  - 剪枝优化（Pruning）
  - 缓存中间结果

- **内存优化**
  - 减少内存分配
  - 对象池复用
  - 大数据集处理优化

### 5. 测试和验证

- **基准测试套件**
  - TPC-H基准测试
  - TPC-DS基准测试
  - 自定义测试场景

- **结果验证**
  - 与实际执行时间对比
  - 成本模型校准
  - 回归测试

### 6. 工具和可视化

- **执行计划可视化**
  - 图形化显示执行计划树
  - 成本分解可视化
  - 交互式分析工具

- **性能分析工具**
  - 性能瓶颈识别
  - 优化建议生成
  - 成本模型调优工具

### 7. 集成和扩展

- **SQL解析集成**
  - 从SQL语句自动生成优化器输入
  - SQL语法支持扩展

- **数据库集成**
  - 与TXSQL项目集成
  - 统计信息自动收集
  - 实时优化

## 技术说明

### 成本模型

基于MySQL/TXSQL的成本模型，包括：
- 服务器端操作成本（行评估、键比较等）
- 存储引擎成本（磁盘/内存读取）
- 连接操作成本

### 优化算法

- **访问路径选择**：比较全表扫描和索引扫描的成本
- **连接顺序优化**：使用贪心算法选择最优连接顺序
- **成本估算**：基于统计信息估算执行成本

### 性能指标

- 优化时间：优化器执行时间
- 总成本：执行计划总成本
- 估计行数：各步骤估计处理的行数

## 许可证

本项目基于TXSQL项目，遵循相应的开源许可证。

## 贡献

欢迎提交Issue和Pull Request来改进本项目。

