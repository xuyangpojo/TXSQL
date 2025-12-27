/**
 * SQL优化器核心模块
 * 
 * 这个模块实现了SQL查询优化器的核心功能：
 * 1. 成本模型（基于MySQL的Cost_model_server）
 * 2. 访问路径选择（索引 vs 全表扫描）
 * 3. 连接顺序优化（基于贪心算法）
 * 4. 性能数据收集
 * 
 * 基于TXSQL项目中的优化器逻辑，提取核心算法思想
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <string>
#include <vector>
#include <map>

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

struct AccessPathCost {
    double total_cost;
    size_t estimated_rows;
    std::string access_type;      // "TABLE_SCAN", "INDEX_SCAN", "INDEX_SEEK", "INDEX_RANGE"
    std::string index_name;
    std::string description;
};

struct JoinPlan {
    std::vector<std::string> table_order;
    std::vector<AccessPathCost> access_paths;
    double total_cost;
    std::string join_type;
};

struct PerformanceMetrics {
    double optimization_time_ms;
    double total_cost;
    size_t estimated_rows;
    size_t num_tables;
    size_t num_indexes_used;
    std::map<std::string, double> cost_breakdown;
};

// ==================== 成本模型类 ====================

class CostModel {
private:
    double row_evaluate_cost_;
    double key_compare_cost_;
    double io_block_read_cost_;
    double memory_block_read_cost_;
    
public:
    CostModel();
    
    // 获取成本常量
    double row_evaluate_cost() const;
    double key_compare_cost() const;
    
    // 计算页读取成本（考虑内存/磁盘比例）
    double page_read_cost(double pages, double pages_in_memory) const;
    
    // 设置成本常量（用于调优）
    void set_row_evaluate_cost(double cost);
    void set_key_compare_cost(double cost);
    void set_io_block_read_cost(double cost);
    void set_memory_block_read_cost(double cost);
};

// ==================== 访问路径计算器 ====================

class AccessPathCalculator {
private:
    const CostModel& cost_model_;
    
    double calculate_table_scan_cost(const TableStats& table) const;
    double calculate_index_seek_cost(const TableStats& table, 
                                     const IndexStats& index,
                                     const QueryCondition& condition) const;
    
public:
    AccessPathCalculator(const CostModel& cost_model);
    
    // 选择最佳访问路径
    AccessPathCost choose_best_access_path(
        const TableStats& table,
        const std::vector<IndexStats>& indexes,
        const std::vector<QueryCondition>& conditions) const;
};

// ==================== 连接顺序优化器 ====================

class JoinOrderOptimizer {
private:
    const CostModel& cost_model_;
    AccessPathCalculator path_calculator_;
    
    double calculate_join_cost(const AccessPathCost& left_path,
                              const AccessPathCost& right_path,
                              const JoinCondition& join) const;
    
public:
    JoinOrderOptimizer(const CostModel& cost_model);
    
    // 优化连接顺序
    JoinPlan optimize_join_order(
        const std::map<std::string, TableStats>& tables,
        const std::map<std::string, std::vector<IndexStats>>& table_indexes,
        const std::vector<QueryCondition>& conditions,
        const std::vector<JoinCondition>& joins) const;
};

// ==================== 性能数据收集器 ====================

class PerformanceCollector {
private:
    std::vector<PerformanceMetrics> metrics_;
    
public:
    void record(const PerformanceMetrics& metrics);
    void print_summary() const;
    void save_to_file(const std::string& filename) const;
};

#endif // OPTIMIZER_H

