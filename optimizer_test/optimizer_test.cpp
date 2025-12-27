/**
 * SQL优化器独立测试程序
 * 在Windows上运行，不依赖整个MySQL项目
 * 模拟SQL优化器的核心功能：连接顺序优化、索引选择、成本估算
 */

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>

// ==================== 数据结构定义 ====================

// 表信息
struct TableInfo {
    std::string name;
    size_t row_count;           // 行数
    size_t avg_row_size;         // 平均行大小（字节）
    std::vector<std::string> columns;  // 列名
    std::map<std::string, size_t> column_cardinality;  // 列基数（不同值的数量）
};

// 索引信息
struct IndexInfo {
    std::string name;
    std::string table_name;
    std::vector<std::string> columns;  // 索引列
    bool is_unique;
    size_t key_parts;           // 键部分数
    double selectivity;         // 选择性（0-1）
};

// 查询条件
struct QueryCondition {
    std::string table_name;
    std::string column_name;
    std::string operator_type;  // "=", ">", "<", "BETWEEN", "IN"
    std::vector<std::string> values;
    double selectivity;         // 条件选择性
};

// 连接条件
struct JoinCondition {
    std::string left_table;
    std::string left_column;
    std::string right_table;
    std::string right_column;
    double selectivity;         // 连接选择性
};

// 访问路径类型
enum class AccessPathType {
    TABLE_SCAN,      // 全表扫描
    INDEX_SCAN,      // 索引扫描
    INDEX_SEEK,      // 索引查找
    INDEX_RANGE      // 索引范围扫描
};

// 访问路径
struct AccessPath {
    std::string table_name;
    AccessPathType type;
    std::string index_name;
    double cost;                 // 成本
    size_t rows;                 // 估计行数
    std::string description;     // 描述
};

// 连接顺序
struct JoinOrder {
    std::vector<std::string> tables;
    double total_cost;
    std::vector<AccessPath> access_paths;
    std::string join_type;       // "NESTED_LOOP", "HASH_JOIN", "SORT_MERGE"
};

// ==================== 优化器类 ====================

class SimpleOptimizer {
private:
    std::map<std::string, TableInfo> tables_;
    std::map<std::string, IndexInfo> indexes_;
    std::vector<QueryCondition> conditions_;
    std::vector<JoinCondition> joins_;
    
    // 成本常量
    static constexpr double DISK_SEEK_COST = 0.9;
    static constexpr double DISK_READ_COST = 0.1;
    static constexpr double CPU_COMPARE_COST = 0.001;
    static constexpr double CPU_ROW_COST = 0.002;
    
public:
    // 添加表
    void addTable(const TableInfo& table) {
        tables_[table.name] = table;
    }
    
    // 添加索引
    void addIndex(const IndexInfo& index) {
        indexes_[index.name] = index;
    }
    
    // 添加查询条件
    void addCondition(const QueryCondition& condition) {
        conditions_.push_back(condition);
    }
    
    // 添加连接条件
    void addJoin(const JoinCondition& join) {
        joins_.push_back(join);
    }
    
    // 计算表扫描成本
    double calculateTableScanCost(const std::string& table_name) {
        if (tables_.find(table_name) == tables_.end()) {
            return 1e10;  // 表不存在，返回极大值
        }
        
        const TableInfo& table = tables_[table_name];
        size_t pages = (table.row_count * table.avg_row_size) / 8192 + 1;  // 假设8KB页
        
        // 全表扫描成本 = 磁盘寻道 + 读取所有页
        return DISK_SEEK_COST + pages * DISK_READ_COST + 
               table.row_count * CPU_ROW_COST;
    }
    
    // 计算索引扫描成本
    double calculateIndexScanCost(const std::string& table_name, 
                                  const std::string& index_name,
                                  const QueryCondition& condition) {
        if (tables_.find(table_name) == tables_.end() ||
            indexes_.find(index_name) == indexes_.end()) {
            return 1e10;
        }
        
        const TableInfo& table = tables_[table_name];
        const IndexInfo& index = indexes_[index_name];
        
        // 估算扫描的行数
        size_t estimated_rows = static_cast<size_t>(
            table.row_count * condition.selectivity);
        
        // 索引扫描成本 = 索引页读取 + 数据页读取（如果需要回表）
        size_t index_pages = estimated_rows / 100;  // 假设每页100个索引项
        size_t data_pages = estimated_rows / 50;    // 假设每页50行
        
        double cost = DISK_SEEK_COST + 
                     index_pages * DISK_READ_COST * 0.5 +  // 索引页读取更快
                     data_pages * DISK_READ_COST +
                     estimated_rows * CPU_COMPARE_COST;
        
        return cost;
    }
    
    // 选择最佳访问路径
    AccessPath chooseBestAccessPath(const std::string& table_name) {
        AccessPath best_path;
        best_path.table_name = table_name;
        best_path.cost = 1e10;
        
        // 1. 计算全表扫描成本
        double table_scan_cost = calculateTableScanCost(table_name);
        if (table_scan_cost < best_path.cost) {
            best_path.cost = table_scan_cost;
            best_path.type = AccessPathType::TABLE_SCAN;
            best_path.description = "全表扫描";
            best_path.rows = tables_[table_name].row_count;
        }
        
        // 2. 检查是否有适用的索引
        for (const auto& index_pair : indexes_) {
            const IndexInfo& index = index_pair.second;
            if (index.table_name != table_name) continue;
            
            // 查找匹配的查询条件
            for (const auto& condition : conditions_) {
                if (condition.table_name == table_name &&
                    std::find(index.columns.begin(), index.columns.end(), 
                             condition.column_name) != index.columns.end()) {
                    
                    double index_cost = calculateIndexScanCost(
                        table_name, index.name, condition);
                    
                    if (index_cost < best_path.cost) {
                        best_path.cost = index_cost;
                        best_path.type = AccessPathType::INDEX_SEEK;
                        best_path.index_name = index.name;
                        best_path.description = "索引查找: " + index.name;
                        best_path.rows = static_cast<size_t>(
                            tables_[table_name].row_count * condition.selectivity);
                    }
                }
            }
        }
        
        return best_path;
    }
    
    // 计算连接成本
    double calculateJoinCost(const AccessPath& left_path,
                            const AccessPath& right_path,
                            const JoinCondition& join) {
        // 嵌套循环连接成本估算
        double cost = left_path.cost;
        
        // 对于左表的每一行，需要查找右表
        double right_lookup_cost = right_path.cost / right_path.rows;  // 单次查找成本
        cost += left_path.rows * right_lookup_cost;
        
        // 连接操作本身的CPU成本
        cost += left_path.rows * right_path.rows * CPU_COMPARE_COST * join.selectivity;
        
        return cost;
    }
    
    // 贪心算法选择连接顺序
    JoinOrder optimizeJoinOrder(const std::vector<std::string>& table_list) {
        JoinOrder best_order;
        best_order.total_cost = 1e10;
        
        // 如果只有一个表，直接返回
        if (table_list.size() == 1) {
            best_order.tables = table_list;
            AccessPath path = chooseBestAccessPath(table_list[0]);
            best_order.access_paths.push_back(path);
            best_order.total_cost = path.cost;
            best_order.join_type = "NONE";
            return best_order;
        }
        
        // 贪心算法：每次选择成本最低的表加入
        std::vector<std::string> remaining_tables = table_list;
        std::vector<std::string> selected_tables;
        std::vector<AccessPath> selected_paths;
        double total_cost = 0.0;
        
        // 选择第一个表（选择行数最少的）
        std::string first_table = *std::min_element(
            remaining_tables.begin(), remaining_tables.end(),
            [this](const std::string& a, const std::string& b) {
                return tables_[a].row_count < tables_[b].row_count;
            });
        
        selected_tables.push_back(first_table);
        AccessPath first_path = chooseBestAccessPath(first_table);
        selected_paths.push_back(first_path);
        total_cost = first_path.cost;
        remaining_tables.erase(
            std::find(remaining_tables.begin(), remaining_tables.end(), first_table));
        
        // 逐步添加其他表
        while (!remaining_tables.empty()) {
            double min_cost = 1e10;
            std::string best_table;
            AccessPath best_path;
            JoinCondition best_join;
            
            for (const std::string& table : remaining_tables) {
                AccessPath path = chooseBestAccessPath(table);
                
                // 查找连接条件
                for (const auto& join : joins_) {
                    bool can_join = false;
                    if ((join.left_table == table && 
                         std::find(selected_tables.begin(), selected_tables.end(), 
                                  join.right_table) != selected_tables.end()) ||
                        (join.right_table == table && 
                         std::find(selected_tables.begin(), selected_tables.end(), 
                                  join.left_table) != selected_tables.end())) {
                        can_join = true;
                        
                        double join_cost = calculateJoinCost(
                            selected_paths.back(), path, join);
                        
                        if (join_cost < min_cost) {
                            min_cost = join_cost;
                            best_table = table;
                            best_path = path;
                            best_join = join;
                        }
                    }
                }
            }
            
            if (best_table.empty()) {
                // 没有找到连接条件，选择成本最低的表
                for (const std::string& table : remaining_tables) {
                    AccessPath path = chooseBestAccessPath(table);
                    if (path.cost < min_cost) {
                        min_cost = path.cost;
                        best_table = table;
                        best_path = path;
                    }
                }
            }
            
            selected_tables.push_back(best_table);
            selected_paths.push_back(best_path);
            total_cost += min_cost - selected_paths[selected_paths.size()-2].cost;
            remaining_tables.erase(
                std::find(remaining_tables.begin(), remaining_tables.end(), best_table));
        }
        
        best_order.tables = selected_tables;
        best_order.access_paths = selected_paths;
        best_order.total_cost = total_cost;
        best_order.join_type = "NESTED_LOOP";
        
        return best_order;
    }
    
    // 优化查询
    JoinOrder optimizeQuery(const std::vector<std::string>& table_list) {
        return optimizeJoinOrder(table_list);
    }
    
    // 打印执行计划
    void printExecutionPlan(const JoinOrder& plan) {
        std::cout << "\n========== 执行计划 ==========\n";
        std::cout << "总成本: " << std::fixed << std::setprecision(2) 
                  << plan.total_cost << "\n";
        std::cout << "连接类型: " << plan.join_type << "\n\n";
        
        std::cout << "表访问顺序:\n";
        for (size_t i = 0; i < plan.tables.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << plan.tables[i] << "\n";
            if (i < plan.access_paths.size()) {
                const AccessPath& path = plan.access_paths[i];
                std::cout << "     访问方式: " << path.description << "\n";
                std::cout << "     估计行数: " << path.rows << "\n";
                std::cout << "     成本: " << std::fixed << std::setprecision(2) 
                          << path.cost << "\n";
            }
            std::cout << "\n";
        }
    }
};

// ==================== 测试函数 ====================

void testSingleTableQuery() {
    std::cout << "\n========== 测试1: 单表查询 ==========\n";
    std::cout << "SQL: SELECT * FROM orders WHERE customer_id = 12345\n";
    
    SimpleOptimizer optimizer;
    
    // 添加表
    TableInfo orders;
    orders.name = "orders";
    orders.row_count = 1000000;  // 100万行
    orders.avg_row_size = 200;
    orders.columns = {"order_id", "customer_id", "order_date", "amount"};
    orders.column_cardinality["customer_id"] = 50000;  // 5万个不同客户
    optimizer.addTable(orders);
    
    // 添加索引
    IndexInfo idx_customer;
    idx_customer.name = "idx_customer_id";
    idx_customer.table_name = "orders";
    idx_customer.columns = {"customer_id"};
    idx_customer.is_unique = false;
    idx_customer.selectivity = 1.0 / 50000.0;  // 选择性 = 1/基数
    optimizer.addIndex(idx_customer);
    
    // 添加查询条件
    QueryCondition condition;
    condition.table_name = "orders";
    condition.column_name = "customer_id";
    condition.operator_type = "=";
    condition.selectivity = 1.0 / 50000.0;  // 等值查询，选择性 = 1/基数
    optimizer.addCondition(condition);
    
    // 优化查询
    std::vector<std::string> tables = {"orders"};
    JoinOrder plan = optimizer.optimizeQuery(tables);
    optimizer.printExecutionPlan(plan);
}

void testTwoTableJoin() {
    std::cout << "\n========== 测试2: 两表连接 ==========\n";
    std::cout << "SQL: SELECT o.order_id, c.customer_name\n";
    std::cout << "     FROM orders o JOIN customers c ON o.customer_id = c.customer_id\n";
    std::cout << "     WHERE o.order_date > '2023-01-01'\n";
    
    SimpleOptimizer optimizer;
    
    // 添加orders表
    TableInfo orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    orders.avg_row_size = 200;
    optimizer.addTable(orders);
    
    // 添加customers表
    TableInfo customers;
    customers.name = "customers";
    customers.row_count = 50000;  // 5万客户，比orders小
    customers.avg_row_size = 150;
    optimizer.addTable(customers);
    
    // 添加索引
    IndexInfo idx_customer;
    idx_customer.name = "idx_customer_id";
    idx_customer.table_name = "orders";
    idx_customer.columns = {"customer_id"};
    optimizer.addIndex(idx_customer);
    
    IndexInfo idx_customer_pk;
    idx_customer_pk.name = "PRIMARY";
    idx_customer_pk.table_name = "customers";
    idx_customer_pk.columns = {"customer_id"};
    idx_customer_pk.is_unique = true;
    optimizer.addIndex(idx_customer_pk);
    
    // 添加查询条件
    QueryCondition condition;
    condition.table_name = "orders";
    condition.column_name = "order_date";
    condition.operator_type = ">";
    condition.selectivity = 0.3;  // 30%的数据满足条件
    optimizer.addCondition(condition);
    
    // 添加连接条件
    JoinCondition join;
    join.left_table = "orders";
    join.left_column = "customer_id";
    join.right_table = "customers";
    join.right_column = "customer_id";
    join.selectivity = 1.0;  // 外键连接，选择性为1
    optimizer.addJoin(join);
    
    // 优化查询
    std::vector<std::string> tables = {"orders", "customers"};
    JoinOrder plan = optimizer.optimizeQuery(tables);
    optimizer.printExecutionPlan(plan);
}

void testThreeTableJoin() {
    std::cout << "\n========== 测试3: 三表连接 ==========\n";
    std::cout << "SQL: SELECT o.order_id, c.customer_name, p.product_name\n";
    std::cout << "     FROM orders o\n";
    std::cout << "     JOIN customers c ON o.customer_id = c.customer_id\n";
    std::cout << "     JOIN order_items oi ON o.order_id = oi.order_id\n";
    std::cout << "     JOIN products p ON oi.product_id = p.product_id\n";
    
    SimpleOptimizer optimizer;
    
    // 添加表
    TableInfo orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    optimizer.addTable(orders);
    
    TableInfo customers;
    customers.name = "customers";
    customers.row_count = 50000;
    optimizer.addTable(customers);
    
    TableInfo order_items;
    order_items.name = "order_items";
    order_items.row_count = 5000000;  // 订单项最多
    optimizer.addTable(order_items);
    
    TableInfo products;
    products.name = "products";
    products.row_count = 10000;  // 产品表最小
    optimizer.addTable(products);
    
    // 添加连接条件
    JoinCondition join1;
    join1.left_table = "orders";
    join1.right_table = "customers";
    join1.selectivity = 1.0;
    optimizer.addJoin(join1);
    
    JoinCondition join2;
    join2.left_table = "orders";
    join2.right_table = "order_items";
    join2.selectivity = 1.0;
    optimizer.addJoin(join2);
    
    JoinCondition join3;
    join3.left_table = "order_items";
    join3.right_table = "products";
    join3.selectivity = 1.0;
    optimizer.addJoin(join3);
    
    // 优化查询
    std::vector<std::string> tables = {"orders", "customers", "order_items", "products"};
    JoinOrder plan = optimizer.optimizeQuery(tables);
    optimizer.printExecutionPlan(plan);
}

// ==================== 主函数 ====================

int main() {
    std::cout << "========================================\n";
    std::cout << "   SQL优化器独立测试程序\n";
    std::cout << "   模拟MySQL优化器的核心功能\n";
    std::cout << "========================================\n";
    
    // 运行测试
    testSingleTableQuery();
    testTwoTableJoin();
    testThreeTableJoin();
    
    std::cout << "\n========== 测试完成 ==========\n";
    std::cout << "说明:\n";
    std::cout << "1. 成本值越小越好\n";
    std::cout << "2. 优化器会选择成本最低的执行计划\n";
    std::cout << "3. 索引通常比全表扫描成本更低\n";
    std::cout << "4. 连接顺序会影响总成本\n";
    
    return 0;
}

