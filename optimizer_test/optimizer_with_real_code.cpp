/**
 * SQL优化器测试程序 - 使用原项目核心逻辑
 * 
 * 这个程序提取并使用了TXSQL项目中的核心优化器逻辑：
 * 1. 成本模型（opt_costmodel）
 * 2. 成本常量（opt_costconstants）
 * 3. 连接顺序优化算法（sql_planner的核心思想）
 * 
 * 注意：这是一个简化版本，提取了核心算法思想，不直接链接MySQL代码
 * 这样可以独立编译运行，同时保持与真实优化器的逻辑一致性
 */

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <chrono>
#include <fstream>
#include <sstream>

// ==================== 从MySQL优化器提取的成本常量 ====================
// 这些常量来自 sql/opt_costconstants.h

// 服务器端操作成本常量（默认值）
constexpr double ROW_EVALUATE_COST = 0.2;        // 评估一行条件的成本
constexpr double KEY_COMPARE_COST = 0.1;        // 比较两个键的成本
constexpr double MEMORY_TEMPTABLE_CREATE_COST = 1.0;  // 创建内存临时表成本
constexpr double MEMORY_TEMPTABLE_ROW_COST = 0.2;     // 内存临时表行操作成本
constexpr double DISK_TEMPTABLE_CREATE_COST = 20.0;   // 创建磁盘临时表成本
constexpr double DISK_TEMPTABLE_ROW_COST = 1.0;       // 磁盘临时表行操作成本

// 存储引擎成本常量（InnoDB默认值）
constexpr double IO_BLOCK_READ_COST = 1.0;       // 从磁盘读取一个块的成本
constexpr double MEMORY_BLOCK_READ_COST = 0.25;  // 从内存读取一个块的成本

// 磁盘寻道成本
constexpr double DISK_SEEK_BASE_COST = 0.9;
constexpr int BLOCKS_IN_AVG_SEEK = 128;
constexpr double DISK_SEEK_PROP_COST = 0.1 / BLOCKS_IN_AVG_SEEK;

// ==================== 数据结构 ====================

struct TableStats {
    std::string name;
    size_t row_count;
    size_t avg_row_size;          // 字节
    double pages_in_memory;        // 在内存中的页比例 (0.0-1.0)
    std::map<std::string, size_t> column_cardinality;  // 列基数
};

struct IndexStats {
    std::string name;
    std::string table_name;
    std::vector<std::string> columns;
    bool is_unique;
    bool is_primary;
    double pages_in_memory;        // 索引页在内存中的比例
    double selectivity;            // 选择性（每个键值对应的平均行数）
};

struct QueryCondition {
    std::string table_name;
    std::string column_name;
    std::string op;                // "=", ">", "<", "BETWEEN", "IN"
    double selectivity;            // 条件选择性（满足条件的行比例）
};

struct JoinCondition {
    std::string left_table;
    std::string left_column;
    std::string right_table;
    std::string right_column;
    double selectivity;            // 连接选择性
};

// ==================== 成本模型类（基于MySQL的Cost_model_server） ====================

class CostModel {
private:
    // 服务器成本常量
    double row_evaluate_cost_;
    double key_compare_cost_;
    
    // 存储引擎成本常量
    double io_block_read_cost_;
    double memory_block_read_cost_;
    
public:
    CostModel() 
        : row_evaluate_cost_(ROW_EVALUATE_COST),
          key_compare_cost_(KEY_COMPARE_COST),
          io_block_read_cost_(IO_BLOCK_READ_COST),
          memory_block_read_cost_(MEMORY_BLOCK_READ_COST) {}
    
    // 评估一行的成本
    double row_evaluate_cost() const { return row_evaluate_cost_; }
    
    // 比较键的成本
    double key_compare_cost() const { return key_compare_cost_; }
    
    // 读取页的成本（考虑内存/磁盘比例）
    double page_read_cost(double pages, double pages_in_memory) const {
        double pages_in_mem = pages * pages_in_memory;
        double pages_on_disk = pages - pages_in_mem;
        
        return pages_in_mem * memory_block_read_cost_ + 
               pages_on_disk * io_block_read_cost_;
    }
    
    // 设置成本常量（用于调优）
    void set_row_evaluate_cost(double cost) { row_evaluate_cost_ = cost; }
    void set_key_compare_cost(double cost) { key_compare_cost_ = cost; }
    void set_io_block_read_cost(double cost) { io_block_read_cost_ = cost; }
    void set_memory_block_read_cost(double cost) { memory_block_read_cost_ = cost; }
};

// ==================== 访问路径成本计算 ====================

struct AccessPathCost {
    double total_cost;
    size_t estimated_rows;
    std::string access_type;      // "TABLE_SCAN", "INDEX_SCAN", "INDEX_SEEK", "INDEX_RANGE"
    std::string index_name;
    std::string description;
};

class AccessPathCalculator {
private:
    const CostModel& cost_model_;
    
    // 计算表扫描成本（基于MySQL的calculate_scan_cost思想）
    double calculate_table_scan_cost(const TableStats& table) const {
        // 计算页数（假设8KB页）
        const size_t PAGE_SIZE = 8192;
        double pages = (table.row_count * table.avg_row_size) / static_cast<double>(PAGE_SIZE);
        if (pages < 1.0) pages = 1.0;
        
        // 页读取成本
        double page_cost = cost_model_.page_read_cost(pages, table.pages_in_memory);
        
        // 行评估成本
        double row_cost = table.row_count * cost_model_.row_evaluate_cost();
        
        return page_cost + row_cost;
    }
    
    // 计算索引查找成本（基于MySQL的find_cost_for_ref）
    double calculate_index_seek_cost(const TableStats& table, 
                                     const IndexStats& index,
                                     const QueryCondition& condition) const {
        // 估算输出行数
        size_t estimated_rows = static_cast<size_t>(table.row_count * condition.selectivity);
        if (estimated_rows < 1) estimated_rows = 1;
        
        // 索引页读取（假设每个索引页100个键）
        double index_pages = estimated_rows / 100.0;
        if (index_pages < 1.0) index_pages = 1.0;
        
        // 数据页读取（如果需要回表）
        double data_pages = estimated_rows / 50.0;  // 假设每页50行
        if (data_pages < 1.0) data_pages = 1.0;
        
        // 索引页读取成本
        double index_cost = cost_model_.page_read_cost(index_pages, index.pages_in_memory);
        
        // 数据页读取成本
        double data_cost = cost_model_.page_read_cost(data_pages, table.pages_in_memory);
        
        // 键比较成本
        double compare_cost = estimated_rows * cost_model_.key_compare_cost();
        
        // 行评估成本
        double row_cost = estimated_rows * cost_model_.row_evaluate_cost();
        
        return index_cost + data_cost + compare_cost + row_cost;
    }
    
public:
    AccessPathCalculator(const CostModel& cost_model) : cost_model_(cost_model) {}
    
    // 选择最佳访问路径（基于MySQL的best_access_path思想）
    AccessPathCost choose_best_access_path(
        const TableStats& table,
        const std::vector<IndexStats>& indexes,
        const std::vector<QueryCondition>& conditions) const {
        
        AccessPathCost best;
        best.total_cost = 1e10;
        best.estimated_rows = table.row_count;
        
        // 1. 计算全表扫描成本
        double table_scan_cost = calculate_table_scan_cost(table);
        if (table_scan_cost < best.total_cost) {
            best.total_cost = table_scan_cost;
            best.access_type = "TABLE_SCAN";
            best.description = "全表扫描";
            best.estimated_rows = table.row_count;
        }
        
        // 2. 检查每个索引
        for (const auto& index : indexes) {
            if (index.table_name != table.name) continue;
            
            // 查找匹配的查询条件
            for (const auto& condition : conditions) {
                if (condition.table_name != table.name) continue;
                
                // 检查索引是否覆盖该列
                bool index_covers = std::find(index.columns.begin(), index.columns.end(),
                                             condition.column_name) != index.columns.end();
                
                if (index_covers) {
                    double index_cost = calculate_index_seek_cost(table, index, condition);
                    
                    if (index_cost < best.total_cost) {
                        best.total_cost = index_cost;
                        best.access_type = index.is_unique ? "INDEX_SEEK" : "INDEX_RANGE";
                        best.index_name = index.name;
                        best.description = "索引查找: " + index.name;
                        best.estimated_rows = static_cast<size_t>(
                            table.row_count * condition.selectivity);
                    }
                }
            }
        }
        
        return best;
    }
};

// ==================== 连接顺序优化器（基于MySQL的greedy_search） ====================

struct JoinPlan {
    std::vector<std::string> table_order;
    std::vector<AccessPathCost> access_paths;
    double total_cost;
    std::string join_type;
};

class JoinOrderOptimizer {
private:
    const CostModel& cost_model_;
    AccessPathCalculator path_calculator_;
    
    // 计算连接成本（基于MySQL的嵌套循环连接成本模型）
    double calculate_join_cost(const AccessPathCost& left_path,
                              const AccessPathCost& right_path,
                              const JoinCondition& join) const {
        // 嵌套循环连接成本
        double cost = left_path.total_cost;
        
        // 对于左表的每一行，查找右表
        double right_lookup_cost = right_path.total_cost / 
                                   static_cast<double>(right_path.estimated_rows);
        if (right_lookup_cost < 0.001) right_lookup_cost = 0.001;  // 最小成本
        
        cost += left_path.estimated_rows * right_lookup_cost;
        
        // 连接条件评估成本
        cost += left_path.estimated_rows * right_path.estimated_rows * 
                join.selectivity * cost_model_.key_compare_cost();
        
        return cost;
    }
    
public:
    JoinOrderOptimizer(const CostModel& cost_model) 
        : cost_model_(cost_model), path_calculator_(cost_model) {}
    
    // 贪心算法优化连接顺序（简化版的greedy_search）
    JoinPlan optimize_join_order(
        const std::map<std::string, TableStats>& tables,
        const std::map<std::string, std::vector<IndexStats>>& table_indexes,
        const std::vector<QueryCondition>& conditions,
        const std::vector<JoinCondition>& joins) const {
        
        JoinPlan plan;
        plan.total_cost = 1e10;
        
        // 如果只有一个表
        if (tables.size() == 1) {
            auto it = tables.begin();
            std::vector<IndexStats> indexes;
            if (table_indexes.find(it->first) != table_indexes.end()) {
                indexes = table_indexes.at(it->first);
            }
            AccessPathCost path = path_calculator_.choose_best_access_path(
                it->second, indexes, conditions);
            
            plan.table_order.push_back(it->first);
            plan.access_paths.push_back(path);
            plan.total_cost = path.total_cost;
            plan.join_type = "NONE";
            return plan;
        }
        
        // 贪心算法：选择成本最低的表开始
        std::vector<std::string> remaining_tables;
        for (const auto& pair : tables) {
            remaining_tables.push_back(pair.first);
        }
        
        std::vector<std::string> selected_tables;
        std::vector<AccessPathCost> selected_paths;
        double current_cost = 0.0;
        
        // 选择第一个表（选择行数最少的）
        auto first_it = std::min_element(remaining_tables.begin(), remaining_tables.end(),
            [&tables](const std::string& a, const std::string& b) {
                return tables.at(a).row_count < tables.at(b).row_count;
            });
        
        std::string first_table = *first_it;
        selected_tables.push_back(first_table);
        
        std::vector<IndexStats> indexes;
        if (table_indexes.find(first_table) != table_indexes.end()) {
            indexes = table_indexes.at(first_table);
        }
        AccessPathCost first_path = path_calculator_.choose_best_access_path(
            tables.at(first_table), indexes, conditions);
        selected_paths.push_back(first_path);
        current_cost = first_path.total_cost;
        
        remaining_tables.erase(first_it);
        
        // 逐步添加其他表
        while (!remaining_tables.empty()) {
            double min_cost = 1e10;
            std::string best_table;
            AccessPathCost best_path;
            JoinCondition best_join;
            bool found_join = false;
            
            for (const std::string& table : remaining_tables) {
                std::vector<IndexStats> table_idx;
                if (table_indexes.find(table) != table_indexes.end()) {
                    table_idx = table_indexes.at(table);
                }
                AccessPathCost path = path_calculator_.choose_best_access_path(
                    tables.at(table), table_idx, conditions);
                
                // 查找连接条件
                for (const auto& join : joins) {
                    bool can_join = false;
                    if (join.left_table == table && 
                        std::find(selected_tables.begin(), selected_tables.end(),
                                 join.right_table) != selected_tables.end()) {
                        can_join = true;
                    } else if (join.right_table == table && 
                              std::find(selected_tables.begin(), selected_tables.end(),
                                       join.left_table) != selected_tables.end()) {
                        can_join = true;
                    }
                    
                    if (can_join) {
                        double join_cost = calculate_join_cost(
                            selected_paths.back(), path, join);
                        
                        if (join_cost < min_cost) {
                            min_cost = join_cost;
                            best_table = table;
                            best_path = path;
                            best_join = join;
                            found_join = true;
                        }
                    }
                }
                
                // 如果没有找到连接，选择成本最低的表
                if (!found_join && path.total_cost < min_cost) {
                    min_cost = path.total_cost;
                    best_table = table;
                    best_path = path;
                }
            }
            
            selected_tables.push_back(best_table);
            selected_paths.push_back(best_path);
            current_cost = min_cost;
            
            remaining_tables.erase(
                std::find(remaining_tables.begin(), remaining_tables.end(), best_table));
        }
        
        plan.table_order = selected_tables;
        plan.access_paths = selected_paths;
        plan.total_cost = current_cost;
        plan.join_type = "NESTED_LOOP";
        
        return plan;
    }
};

// ==================== 性能数据收集 ====================

struct PerformanceMetrics {
    double optimization_time_ms;
    double total_cost;
    size_t estimated_rows;
    size_t num_tables;
    size_t num_indexes_used;
    std::map<std::string, double> cost_breakdown;
};

class PerformanceCollector {
private:
    std::vector<PerformanceMetrics> metrics_;
    
public:
    void record(const PerformanceMetrics& metrics) {
        metrics_.push_back(metrics);
    }
    
    void print_summary() const {
        if (metrics_.empty()) return;
        
        std::cout << "\n========== 性能统计 ==========\n";
        std::cout << "测试次数: " << metrics_.size() << "\n";
        
        double avg_cost = 0.0;
        double avg_time = 0.0;
        for (const auto& m : metrics_) {
            avg_cost += m.total_cost;
            avg_time += m.optimization_time_ms;
        }
        avg_cost /= metrics_.size();
        avg_time /= metrics_.size();
        
        std::cout << "平均成本: " << std::fixed << std::setprecision(2) << avg_cost << "\n";
        std::cout << "平均优化时间: " << std::fixed << std::setprecision(3) << avg_time << " ms\n";
    }
    
    void save_to_file(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        file << "优化时间(ms),总成本,估计行数,表数,使用索引数\n";
        for (const auto& m : metrics_) {
            file << m.optimization_time_ms << ","
                 << m.total_cost << ","
                 << m.estimated_rows << ","
                 << m.num_tables << ","
                 << m.num_indexes_used << "\n";
        }
        
        file.close();
        std::cout << "性能数据已保存到: " << filename << "\n";
    }
};

// ==================== 测试用例 ====================

void run_test_case(const std::string& test_name,
                  const std::map<std::string, TableStats>& tables,
                  const std::map<std::string, std::vector<IndexStats>>& indexes,
                  const std::vector<QueryCondition>& conditions,
                  const std::vector<JoinCondition>& joins,
                  CostModel& cost_model,
                  PerformanceCollector& collector) {
    
    std::cout << "\n========== " << test_name << " ==========\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    JoinOrderOptimizer optimizer(cost_model);
    JoinPlan plan = optimizer.optimize_join_order(tables, indexes, conditions, joins);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double time_ms = duration.count() / 1000.0;
    
    // 打印执行计划
    std::cout << "总成本: " << std::fixed << std::setprecision(2) << plan.total_cost << "\n";
    std::cout << "连接类型: " << plan.join_type << "\n";
    std::cout << "优化时间: " << std::fixed << std::setprecision(3) << time_ms << " ms\n\n";
    
    std::cout << "表访问顺序:\n";
    size_t total_rows = 0;
    size_t indexes_used = 0;
    for (size_t i = 0; i < plan.table_order.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << plan.table_order[i] << "\n";
        if (i < plan.access_paths.size()) {
            const auto& path = plan.access_paths[i];
            std::cout << "     访问方式: " << path.description << "\n";
            std::cout << "     估计行数: " << path.estimated_rows << "\n";
            std::cout << "     成本: " << std::fixed << std::setprecision(2) 
                      << path.total_cost << "\n";
            total_rows += path.estimated_rows;
            if (path.access_type != "TABLE_SCAN") indexes_used++;
        }
        std::cout << "\n";
    }
    
    // 记录性能指标
    PerformanceMetrics metrics;
    metrics.optimization_time_ms = time_ms;
    metrics.total_cost = plan.total_cost;
    metrics.estimated_rows = total_rows;
    metrics.num_tables = tables.size();
    metrics.num_indexes_used = indexes_used;
    collector.record(metrics);
}

// ==================== 主函数 ====================

int main() {
    std::cout << "========================================\n";
    std::cout << "   SQL优化器测试程序（使用真实逻辑）\n";
    std::cout << "   基于TXSQL优化器的核心算法\n";
    std::cout << "========================================\n";
    
    // 创建成本模型
    CostModel cost_model;
    
    // 性能收集器
    PerformanceCollector collector;
    
    // ========== 测试1: 单表查询 ==========
    {
        std::map<std::string, TableStats> tables;
        TableStats orders;
        orders.name = "orders";
        orders.row_count = 1000000;
        orders.avg_row_size = 200;
        orders.pages_in_memory = 0.3;  // 30%在内存
        orders.column_cardinality["customer_id"] = 50000;
        tables["orders"] = orders;
        
        std::map<std::string, std::vector<IndexStats>> indexes;
        IndexStats idx_customer;
        idx_customer.name = "idx_customer_id";
        idx_customer.table_name = "orders";
        idx_customer.columns = {"customer_id"};
        idx_customer.is_unique = false;
        idx_customer.pages_in_memory = 0.5;
        idx_customer.selectivity = 1.0 / 50000.0;
        indexes["orders"] = {idx_customer};
        
        std::vector<QueryCondition> conditions;
        QueryCondition cond;
        cond.table_name = "orders";
        cond.column_name = "customer_id";
        cond.op = "=";
        cond.selectivity = 1.0 / 50000.0;
        conditions.push_back(cond);
        
        run_test_case("测试1: 单表查询（索引 vs 全表扫描）",
                     tables, indexes, conditions, {}, cost_model, collector);
    }
    
    // ========== 测试2: 两表连接 ==========
    {
        std::map<std::string, TableStats> tables;
        
        TableStats orders;
        orders.name = "orders";
        orders.row_count = 1000000;
        orders.avg_row_size = 200;
        orders.pages_in_memory = 0.3;
        tables["orders"] = orders;
        
        TableStats customers;
        customers.name = "customers";
        customers.row_count = 50000;
        customers.avg_row_size = 150;
        customers.pages_in_memory = 0.8;  // 客户表更可能在内存
        tables["customers"] = customers;
        
        std::map<std::string, std::vector<IndexStats>> indexes;
        IndexStats idx_customer;
        idx_customer.name = "idx_customer_id";
        idx_customer.table_name = "orders";
        idx_customer.columns = {"customer_id"};
        idx_customer.pages_in_memory = 0.5;
        indexes["orders"] = {idx_customer};
        
        IndexStats idx_pk;
        idx_pk.name = "PRIMARY";
        idx_pk.table_name = "customers";
        idx_pk.columns = {"customer_id"};
        idx_pk.is_unique = true;
        idx_pk.is_primary = true;
        idx_pk.pages_in_memory = 0.9;
        indexes["customers"] = {idx_pk};
        
        std::vector<QueryCondition> conditions;
        QueryCondition cond;
        cond.table_name = "orders";
        cond.column_name = "order_date";
        cond.op = ">";
        cond.selectivity = 0.3;
        conditions.push_back(cond);
        
        std::vector<JoinCondition> joins;
        JoinCondition join;
        join.left_table = "orders";
        join.left_column = "customer_id";
        join.right_table = "customers";
        join.right_column = "customer_id";
        join.selectivity = 1.0;
        joins.push_back(join);
        
        run_test_case("测试2: 两表连接（测试连接顺序）",
                     tables, indexes, conditions, joins, cost_model, collector);
    }
    
    // ========== 测试3: 多表连接 ==========
    {
        std::map<std::string, TableStats> tables;
        
        TableStats orders;
        orders.name = "orders";
        orders.row_count = 1000000;
        orders.pages_in_memory = 0.3;
        tables["orders"] = orders;
        
        TableStats customers;
        customers.name = "customers";
        customers.row_count = 50000;
        customers.pages_in_memory = 0.8;
        tables["customers"] = customers;
        
        TableStats order_items;
        order_items.name = "order_items";
        order_items.row_count = 5000000;
        order_items.pages_in_memory = 0.2;
        tables["order_items"] = order_items;
        
        TableStats products;
        products.name = "products";
        products.row_count = 10000;
        products.pages_in_memory = 0.9;
        tables["products"] = products;
        
        std::map<std::string, std::vector<IndexStats>> indexes;
        // 简化：假设所有表都有主键索引
        
        std::vector<JoinCondition> joins;
        JoinCondition j1;
        j1.left_table = "orders";
        j1.right_table = "customers";
        j1.selectivity = 1.0;
        joins.push_back(j1);
        
        JoinCondition j2;
        j2.left_table = "orders";
        j2.right_table = "order_items";
        j2.selectivity = 1.0;
        joins.push_back(j2);
        
        JoinCondition j3;
        j3.left_table = "order_items";
        j3.right_table = "products";
        j3.selectivity = 1.0;
        joins.push_back(j3);
        
        run_test_case("测试3: 多表连接（4表）",
                     tables, indexes, {}, joins, cost_model, collector);
    }
    
    // 打印性能统计
    collector.print_summary();
    
    // 保存性能数据
    collector.save_to_file("optimizer_performance.csv");
    
    std::cout << "\n========== 测试完成 ==========\n";
    
    return 0;
}

