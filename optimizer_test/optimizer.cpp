/**
 * SQL优化器核心模块实现
 */

#include "optimizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

// ==================== 成本常量 ====================

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

// ==================== CostModel 实现 ====================

CostModel::CostModel() 
    : row_evaluate_cost_(ROW_EVALUATE_COST),
      key_compare_cost_(KEY_COMPARE_COST),
      io_block_read_cost_(IO_BLOCK_READ_COST),
      memory_block_read_cost_(MEMORY_BLOCK_READ_COST) {}

double CostModel::row_evaluate_cost() const { 
    return row_evaluate_cost_; 
}

double CostModel::key_compare_cost() const { 
    return key_compare_cost_; 
}

double CostModel::page_read_cost(double pages, double pages_in_memory) const {
    double pages_in_mem = pages * pages_in_memory;
    double pages_on_disk = pages - pages_in_mem;
    
    return pages_in_mem * memory_block_read_cost_ + 
           pages_on_disk * io_block_read_cost_;
}

void CostModel::set_row_evaluate_cost(double cost) { 
    row_evaluate_cost_ = cost; 
}

void CostModel::set_key_compare_cost(double cost) { 
    key_compare_cost_ = cost; 
}

void CostModel::set_io_block_read_cost(double cost) { 
    io_block_read_cost_ = cost; 
}

void CostModel::set_memory_block_read_cost(double cost) { 
    memory_block_read_cost_ = cost; 
}

// ==================== AccessPathCalculator 实现 ====================

AccessPathCalculator::AccessPathCalculator(const CostModel& cost_model) 
    : cost_model_(cost_model) {}

double AccessPathCalculator::calculate_table_scan_cost(const TableStats& table) const {
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

double AccessPathCalculator::calculate_index_seek_cost(const TableStats& table, 
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

AccessPathCost AccessPathCalculator::choose_best_access_path(
    const TableStats& table,
    const std::vector<IndexStats>& indexes,
    const std::vector<QueryCondition>& conditions) const {
    
    AccessPathCost best;
    best.total_cost = 1e10;
    best.estimated_rows = table.row_count;
    
        // 1. Calculate table scan cost
        double table_scan_cost = calculate_table_scan_cost(table);
        if (table_scan_cost < best.total_cost) {
            best.total_cost = table_scan_cost;
            best.access_type = "TABLE_SCAN";
            best.description = "Table Scan";
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
                        best.description = "Index Seek: " + index.name;
                        best.estimated_rows = static_cast<size_t>(
                            table.row_count * condition.selectivity);
                    }
            }
        }
    }
    
    return best;
}

// ==================== JoinOrderOptimizer 实现 ====================

JoinOrderOptimizer::JoinOrderOptimizer(const CostModel& cost_model) 
    : cost_model_(cost_model), path_calculator_(cost_model) {}

double JoinOrderOptimizer::calculate_join_cost(const AccessPathCost& left_path,
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

JoinPlan JoinOrderOptimizer::optimize_join_order(
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

// ==================== PerformanceCollector 实现 ====================

void PerformanceCollector::record(const PerformanceMetrics& metrics) {
    metrics_.push_back(metrics);
}

void PerformanceCollector::print_summary() const {
    if (metrics_.empty()) return;
    
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "                          PERFORMANCE SUMMARY\n";
    std::cout << "================================================================================\n";
    std::cout << "Test Count: " << metrics_.size() << "\n\n";
    
    double avg_cost = 0.0;
    double avg_time = 0.0;
    double min_cost = 1e10;
    double max_cost = 0.0;
    size_t total_rows = 0;
    size_t total_indexes = 0;
    
    for (const auto& m : metrics_) {
        avg_cost += m.total_cost;
        avg_time += m.optimization_time_ms;
        if (m.total_cost < min_cost) min_cost = m.total_cost;
        if (m.total_cost > max_cost) max_cost = m.total_cost;
        total_rows += m.estimated_rows;
        total_indexes += m.num_indexes_used;
    }
    avg_cost /= metrics_.size();
    avg_time /= metrics_.size();
    
    double min_time = 1e10;
    double max_time = 0.0;
    for (const auto& m : metrics_) {
        if (m.optimization_time_ms < min_time) min_time = m.optimization_time_ms;
        if (m.optimization_time_ms > max_time) max_time = m.optimization_time_ms;
    }
    
    // Cost Statistics Table
    std::cout << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "COST STATISTICS" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    std::vector<std::string> cost_headers = {"Metric", "Value"};
    std::vector<int> cost_widths = {30, 45};
    
    std::cout << "| " << std::left << std::setw(30) << "Metric" 
              << " | " << std::right << std::setw(45) << "Value" << " |\n";
    std::cout << "|" << std::string(30, '-') << "-|" << std::string(45, '-') << "-|\n";
    
    std::stringstream ss_avg;
    std::stringstream ss_min;
    std::stringstream ss_max;
    std::stringstream ss_range;
    ss_avg << std::fixed << std::setprecision(2) << avg_cost;
    ss_min << std::fixed << std::setprecision(2) << min_cost;
    ss_max << std::fixed << std::setprecision(2) << max_cost;
    ss_range << std::fixed << std::setprecision(2) << (max_cost - min_cost);
    
    std::cout << "| " << std::left << std::setw(30) << "Average Cost" 
              << " | " << std::right << std::setw(45) << ss_avg.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Minimum Cost" 
              << " | " << std::right << std::setw(45) << ss_min.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Maximum Cost" 
              << " | " << std::right << std::setw(45) << ss_max.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Cost Range" 
              << " | " << std::right << std::setw(45) << ss_range.str() << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n\n";
    
    // Time Statistics Table
    std::cout << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "TIME STATISTICS" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    std::stringstream ss_avg_time;
    std::stringstream ss_min_time;
    std::stringstream ss_max_time;
    ss_avg_time << std::fixed << std::setprecision(3) << avg_time << " ms";
    ss_min_time << std::fixed << std::setprecision(3) << min_time << " ms";
    ss_max_time << std::fixed << std::setprecision(3) << max_time << " ms";
    
    std::cout << "| " << std::left << std::setw(30) << "Metric" 
              << " | " << std::right << std::setw(45) << "Value" << " |\n";
    std::cout << "|" << std::string(30, '-') << "-|" << std::string(45, '-') << "-|\n";
    std::cout << "| " << std::left << std::setw(30) << "Average Optimization Time" 
              << " | " << std::right << std::setw(45) << ss_avg_time.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Minimum Time" 
              << " | " << std::right << std::setw(45) << ss_min_time.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Maximum Time" 
              << " | " << std::right << std::setw(45) << ss_max_time.str() << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n\n";
    
    // Data Statistics Table
    std::cout << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "DATA STATISTICS" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    std::stringstream ss_total_rows;
    std::stringstream ss_avg_rows;
    std::stringstream ss_avg_idx;
    ss_total_rows << std::fixed << std::setprecision(2) 
                  << (total_rows / 1000000.0) << "M (" 
                  << total_rows << " total)";
    ss_avg_rows << std::fixed << std::setprecision(0) 
                << (total_rows / static_cast<double>(metrics_.size()));
    ss_avg_idx << std::fixed << std::setprecision(2) 
               << (total_indexes / static_cast<double>(metrics_.size()));
    
    std::cout << "| " << std::left << std::setw(30) << "Metric" 
              << " | " << std::right << std::setw(45) << "Value" << " |\n";
    std::cout << "|" << std::string(30, '-') << "-|" << std::string(45, '-') << "-|\n";
    std::cout << "| " << std::left << std::setw(30) << "Total Estimated Rows" 
              << " | " << std::right << std::setw(45) << ss_total_rows.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Average Rows per Test" 
              << " | " << std::right << std::setw(45) << ss_avg_rows.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Total Indexes Used" 
              << " | " << std::right << std::setw(45) << total_indexes << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "Average Indexes per Test" 
              << " | " << std::right << std::setw(45) << ss_avg_idx.str() << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    // Performance Comparison Table
    std::cout << "\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "PERFORMANCE COMPARISON BY TEST" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    std::cout << "| " << std::left << std::setw(6) << "Test" 
              << " | " << std::right << std::setw(15) << "Cost" 
              << " | " << std::right << std::setw(12) << "Time (ms)" 
              << " | " << std::right << std::setw(12) << "Rows" 
              << " | " << std::right << std::setw(8) << "Tables" 
              << " | " << std::right << std::setw(8) << "Indexes" << " |\n";
    std::cout << "|" << std::string(6, '-') << "-|" << std::string(15, '-') 
              << "-|" << std::string(12, '-') << "-|" << std::string(12, '-') 
              << "-|" << std::string(8, '-') << "-|" << std::string(8, '-') << "-|\n";
    
    for (size_t i = 0; i < metrics_.size(); ++i) {
        const auto& m = metrics_.at(i);
        std::stringstream ss_cost;
        std::stringstream ss_time;
        std::stringstream ss_rows;
        ss_cost << std::fixed << std::setprecision(2) << m.total_cost;
        ss_time << std::fixed << std::setprecision(3) << m.optimization_time_ms;
        ss_rows << std::fixed << std::setprecision(2) << (m.estimated_rows / 1000000.0) << "M";
        
        std::cout << "| " << std::left << std::setw(6) << ("#" + std::to_string(i + 1))
                  << " | " << std::right << std::setw(15) << ss_cost.str()
                  << " | " << std::right << std::setw(12) << ss_time.str()
                  << " | " << std::right << std::setw(12) << ss_rows.str()
                  << " | " << std::right << std::setw(8) << m.num_tables
                  << " | " << std::right << std::setw(8) << m.num_indexes_used << " |\n";
    }
    std::cout << "+" << std::string(78, '-') << "+\n";
}

void PerformanceCollector::save_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << "\n";
        return;
    }
    
    file << "Optimization Time(ms),Total Cost,Estimated Rows,Table Count,Indexes Used\n";
    for (const auto& m : metrics_) {
        file << m.optimization_time_ms << ","
             << m.total_cost << ","
             << m.estimated_rows << ","
             << m.num_tables << ","
             << m.num_indexes_used << "\n";
    }
    
    file.close();
    std::cout << "Performance data saved to: " << filename << "\n";
}

