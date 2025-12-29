#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <string>
#include <vector>
#include <map>

struct TableStats {
    std::string name;
    size_t row_count;
    size_t avg_row_size;
    double pages_in_memory;
    std::map<std::string, size_t> column_cardinality;
};

struct IndexStats {
    std::string name;
    std::string table_name;
    std::vector<std::string> columns;
    bool is_unique;
    bool is_primary;
    double pages_in_memory;
    double selectivity;
};

struct QueryCondition {
    std::string table_name;
    std::string column_name;
    std::string op;
    double selectivity;
};

struct JoinCondition {
    std::string left_table;
    std::string left_column;
    std::string right_table;
    std::string right_column;
    double selectivity;
};

struct AccessPathCost {
    double total_cost;
    size_t estimated_rows;
    std::string access_type;
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

class CostModel {
private:
    double row_evaluate_cost_;
    double key_compare_cost_;
    double io_block_read_cost_;
    double memory_block_read_cost_;
    
public:
    CostModel();
    double row_evaluate_cost() const;
    double key_compare_cost() const;
    double page_read_cost(double pages, double pages_in_memory) const;
    void set_row_evaluate_cost(double cost);
    void set_key_compare_cost(double cost);
    void set_io_block_read_cost(double cost);
    void set_memory_block_read_cost(double cost);
};

class AccessPathCalculator {
private:
    const CostModel& cost_model_;
    
    double calculate_table_scan_cost(const TableStats& table) const;
    double calculate_index_seek_cost(const TableStats& table, 
                                     const IndexStats& index,
                                     const QueryCondition& condition) const;
    
public:
    AccessPathCalculator(const CostModel& cost_model);
    AccessPathCost choose_best_access_path(
        const TableStats& table,
        const std::vector<IndexStats>& indexes,
        const std::vector<QueryCondition>& conditions) const;
};

class JoinOrderOptimizer {
private:
    const CostModel& cost_model_;
    AccessPathCalculator path_calculator_;
    
    double calculate_join_cost(const AccessPathCost& left_path,
                              const AccessPathCost& right_path,
                              const JoinCondition& join) const;
    
public:
    JoinOrderOptimizer(const CostModel& cost_model);
    
    JoinPlan optimize_join_order(
        const std::map<std::string, TableStats>& tables,
        const std::map<std::string, std::vector<IndexStats>>& table_indexes,
        const std::vector<QueryCondition>& conditions,
        const std::vector<JoinCondition>& joins) const;
};

class PerformanceCollector {
private:
    std::vector<PerformanceMetrics> metrics_;
    
public:
    void record(const PerformanceMetrics& metrics);
    void print_summary() const;
    void save_to_file(const std::string& filename) const;
};

#endif