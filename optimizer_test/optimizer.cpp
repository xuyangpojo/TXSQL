#include "optimizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

namespace Color {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
}

constexpr double ROW_EVALUATE_COST = 0.2;
constexpr double KEY_COMPARE_COST = 0.1;
constexpr double MEMORY_TEMPTABLE_CREATE_COST = 1.0;
constexpr double MEMORY_TEMPTABLE_ROW_COST = 0.2;
constexpr double DISK_TEMPTABLE_CREATE_COST = 20.0;
constexpr double DISK_TEMPTABLE_ROW_COST = 1.0;

constexpr double IO_BLOCK_READ_COST = 1.0;
constexpr double MEMORY_BLOCK_READ_COST = 0.25;

constexpr double DISK_SEEK_BASE_COST = 0.9;
constexpr int BLOCKS_IN_AVG_SEEK = 128;
constexpr double DISK_SEEK_PROP_COST = 0.1 / BLOCKS_IN_AVG_SEEK;

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

AccessPathCalculator::AccessPathCalculator(const CostModel& cost_model) 
    : cost_model_(cost_model) {}

double AccessPathCalculator::calculate_table_scan_cost(const TableStats& table) const {
    const size_t PAGE_SIZE = 8192;
    double pages = (table.row_count * table.avg_row_size) / static_cast<double>(PAGE_SIZE);
    if (pages < 1.0) pages = 1.0;
    double page_cost = cost_model_.page_read_cost(pages, table.pages_in_memory);
    double row_cost = table.row_count * cost_model_.row_evaluate_cost();
    return page_cost + row_cost;
}

double AccessPathCalculator::calculate_index_seek_cost(const TableStats& table, 
                                                      const IndexStats& index,
                                                      const QueryCondition& condition) const {
    double selectivity = condition.selectivity;
    if (index.selectivity > 0 && index.selectivity < selectivity) {
        selectivity = index.selectivity;
    }
    
    size_t estimated_rows = static_cast<size_t>(table.row_count * selectivity);
    if (estimated_rows < 1) estimated_rows = 1;
    
    double index_pages = estimated_rows / 100.0;
    if (index_pages < 1.0) index_pages = 1.0;
    double data_pages = estimated_rows / 50.0;
    if (data_pages < 1.0) data_pages = 1.0;
    
    double index_cost = cost_model_.page_read_cost(index_pages, index.pages_in_memory);
    double data_cost = cost_model_.page_read_cost(data_pages, table.pages_in_memory);
    double compare_cost = estimated_rows * cost_model_.key_compare_cost();
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
    
    double table_scan_cost = calculate_table_scan_cost(table);
    if (table_scan_cost < best.total_cost) {
        best.total_cost = table_scan_cost;
        best.access_type = "TABLE_SCAN";
        best.description = "Table Scan";
        best.estimated_rows = table.row_count;
    }
    
    for (const auto& index : indexes) {
        if (index.table_name != table.name) continue;
        
        std::vector<QueryCondition> matching_conditions;
        double combined_selectivity = 1.0;
        
        for (const auto& condition : conditions) {
            if (condition.table_name != table.name) continue;
            bool index_covers = std::find(index.columns.begin(), index.columns.end(),
                                         condition.column_name) != index.columns.end();
            if (index_covers) {
                matching_conditions.push_back(condition);
                combined_selectivity *= condition.selectivity;
            }
        }
        
        if (!matching_conditions.empty()) {
            QueryCondition best_condition = matching_conditions[0];
            for (const auto& cond : matching_conditions) {
                if (cond.selectivity < best_condition.selectivity) {
                    best_condition = cond;
                }
            }
            
            if (matching_conditions.size() > 1) {
                best_condition.selectivity = combined_selectivity;
            }
            
            double index_cost = calculate_index_seek_cost(table, index, best_condition);
            
            if (index_cost < best.total_cost) {
                best.total_cost = index_cost;
                best.access_type = index.is_unique ? "INDEX_SEEK" : "INDEX_RANGE";
                best.index_name = index.name;
                best.description = "Index Seek: " + index.name;
                best.estimated_rows = static_cast<size_t>(
                    table.row_count * best_condition.selectivity);
            }
        }
    }
    
    return best;
}

JoinOrderOptimizer::JoinOrderOptimizer(const CostModel& cost_model) 
    : cost_model_(cost_model), path_calculator_(cost_model) {}

double JoinOrderOptimizer::calculate_join_cost(const AccessPathCost& left_path,
                                                const AccessPathCost& right_path,
                                                const JoinCondition& join) const {
    double cost = left_path.total_cost;
    
    size_t join_result_rows = static_cast<size_t>(
        left_path.estimated_rows * right_path.estimated_rows * join.selectivity);
    
    if (join_result_rows > left_path.estimated_rows) {
        join_result_rows = left_path.estimated_rows;
    }
    if (join_result_rows > right_path.estimated_rows) {
        join_result_rows = right_path.estimated_rows;
    }
    if (join_result_rows == 0) join_result_rows = 1;
    
    if (right_path.estimated_rows > 0) {
        double right_lookup_cost = right_path.total_cost / 
                                   static_cast<double>(right_path.estimated_rows);
        if (right_lookup_cost < 0.001) right_lookup_cost = 0.001;
        
        cost += join_result_rows * right_lookup_cost;
        cost += join_result_rows * cost_model_.key_compare_cost();
    } else {
        cost += right_path.total_cost;
    }
    
    return cost;
}

JoinPlan JoinOrderOptimizer::optimize_join_order(
    const std::map<std::string, TableStats>& tables,
    const std::map<std::string, std::vector<IndexStats>>& table_indexes,
    const std::vector<QueryCondition>& conditions,
    const std::vector<JoinCondition>& joins) const {
    
    JoinPlan plan;
    plan.total_cost = 1e10;
    
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
    
    std::vector<std::string> remaining_tables;
    for (const auto& pair : tables) {
        remaining_tables.push_back(pair.first);
    }
    
    std::vector<std::string> selected_tables;
    std::vector<AccessPathCost> selected_paths;
    double current_cost = 0.0;
    
    std::string first_table;
    double min_first_cost = 1e10;
    size_t min_first_rows = SIZE_MAX;
    
    for (const std::string& table : remaining_tables) {
        std::vector<IndexStats> indexes;
        if (table_indexes.find(table) != table_indexes.end()) {
            indexes = table_indexes.at(table);
        }
        AccessPathCost path = path_calculator_.choose_best_access_path(
            tables.at(table), indexes, conditions);
        
        if (path.total_cost < min_first_cost || 
            (std::abs(path.total_cost - min_first_cost) < 0.01 && tables.at(table).row_count < min_first_rows)) {
            min_first_cost = path.total_cost;
            min_first_rows = tables.at(table).row_count;
            first_table = table;
        }
    }
    
    auto first_it = std::find(remaining_tables.begin(), remaining_tables.end(), first_table);
    
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
            
            std::vector<QueryCondition> table_conditions = conditions;
            for (const auto& join : joins) {
                QueryCondition join_cond;
                bool add_condition = false;
                
                if (join.left_table == table) {
                    join_cond.table_name = table;
                    join_cond.column_name = join.left_column;
                    join_cond.op = "=";
                    size_t left_table_rows = tables.at(join.right_table).row_count;
                    if (left_table_rows > 0) {
                        join_cond.selectivity = 1.0 / static_cast<double>(left_table_rows);
                    } else {
                        join_cond.selectivity = 0.01;
                    }
                    add_condition = true;
                } else if (join.right_table == table) {
                    join_cond.table_name = table;
                    join_cond.column_name = join.right_column;
                    join_cond.op = "=";
                    size_t right_table_rows = tables.at(join.left_table).row_count;
                    if (right_table_rows > 0) {
                        join_cond.selectivity = 1.0 / static_cast<double>(right_table_rows);
                    } else {
                        join_cond.selectivity = 0.01;
                    }
                    add_condition = true;
                }
                
                if (add_condition) {
                    bool exists = false;
                    for (const auto& cond : table_conditions) {
                        if (cond.table_name == join_cond.table_name && 
                            cond.column_name == join_cond.column_name) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        table_conditions.push_back(join_cond);
                    }
                }
            }
            
            AccessPathCost path = path_calculator_.choose_best_access_path(
                tables.at(table), table_idx, table_conditions);
            
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
                    AccessPathCost cumulative_left_path = selected_paths.back();
                    cumulative_left_path.total_cost = current_cost;
                    
                    double join_cost = calculate_join_cost(
                        cumulative_left_path, path, join);
                    
                    if (join_cost < min_cost) {
                        min_cost = join_cost;
                        best_table = table;
                        best_path = path;
                        best_join = join;
                        found_join = true;
                    }
                }
            }
            
            if (!found_join) {
                double cumulative_cost = current_cost + path.total_cost;
                if (cumulative_cost < min_cost) {
                    min_cost = cumulative_cost;
                    best_table = table;
                    best_path = path;
                }
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


void PerformanceCollector::record(const PerformanceMetrics& metrics) {
    metrics_.push_back(metrics);
}

void PerformanceCollector::print_summary() const {
    if (metrics_.empty()) return;
    
    std::cout << "\n";
    std::cout << Color::CYAN << Color::BOLD;
    std::cout << "================================================================================\n";
    std::cout << "                          性能统计摘要\n";
    std::cout << "================================================================================\n";
    std::cout << Color::RESET;
    std::cout << Color::YELLOW << "测试数量: " << Color::RESET << metrics_.size() << "\n\n";
    
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
    
    std::cout << Color::GREEN << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "成本统计" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n" << Color::RESET;
    
    std::vector<std::string> cost_headers = {"指标", "数值"};
    std::vector<int> cost_widths = {30, 45};
    
    std::cout << Color::YELLOW << "| " << std::left << std::setw(30) << "指标" 
              << " | " << std::right << std::setw(45) << "数值" << " |\n" << Color::RESET;
    std::cout << "|" << std::string(30, '-') << "-|" << std::string(45, '-') << "-|\n";
    
    std::stringstream ss_avg;
    std::stringstream ss_min;
    std::stringstream ss_max;
    std::stringstream ss_range;
    ss_avg << std::fixed << std::setprecision(2) << avg_cost;
    ss_min << std::fixed << std::setprecision(2) << min_cost;
    ss_max << std::fixed << std::setprecision(2) << max_cost;
    ss_range << std::fixed << std::setprecision(2) << (max_cost - min_cost);
    
    std::cout << "| " << std::left << std::setw(30) << "平均成本" 
              << " | " << std::right << std::setw(45) << Color::CYAN << ss_avg.str() << Color::RESET << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "最小成本" 
              << " | " << std::right << std::setw(45) << Color::GREEN << ss_min.str() << Color::RESET << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "最大成本" 
              << " | " << std::right << std::setw(45) << Color::RED << ss_max.str() << Color::RESET << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "成本范围" 
              << " | " << std::right << std::setw(45) << ss_range.str() << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n\n";
    
    std::cout << Color::GREEN << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "时间统计" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n" << Color::RESET;
    
    std::stringstream ss_avg_time;
    std::stringstream ss_min_time;
    std::stringstream ss_max_time;
    ss_avg_time << std::fixed << std::setprecision(3) << avg_time << " 毫秒";
    ss_min_time << std::fixed << std::setprecision(3) << min_time << " 毫秒";
    ss_max_time << std::fixed << std::setprecision(3) << max_time << " 毫秒";
    
    std::cout << Color::YELLOW << "| " << std::left << std::setw(30) << "指标" 
              << " | " << std::right << std::setw(45) << "数值" << " |\n" << Color::RESET;
    std::cout << "|" << std::string(30, '-') << "-|" << std::string(45, '-') << "-|\n";
    std::cout << "| " << std::left << std::setw(30) << "平均优化时间" 
              << " | " << std::right << std::setw(45) << Color::CYAN << ss_avg_time.str() << Color::RESET << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "最短时间" 
              << " | " << std::right << std::setw(45) << Color::GREEN << ss_min_time.str() << Color::RESET << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "最长时间" 
              << " | " << std::right << std::setw(45) << Color::RED << ss_max_time.str() << Color::RESET << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n\n";
    
    std::cout << Color::GREEN << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "数据统计" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n" << Color::RESET;
    
    std::stringstream ss_total_rows;
    std::stringstream ss_avg_rows;
    std::stringstream ss_avg_idx;
    ss_total_rows << std::fixed << std::setprecision(2) 
                  << (total_rows / 1000000.0) << "M (" 
                  << total_rows << " 总计)";
    ss_avg_rows << std::fixed << std::setprecision(0) 
                << (total_rows / static_cast<double>(metrics_.size()));
    ss_avg_idx << std::fixed << std::setprecision(2) 
               << (total_indexes / static_cast<double>(metrics_.size()));
    
    std::cout << Color::YELLOW << "| " << std::left << std::setw(30) << "指标" 
              << " | " << std::right << std::setw(45) << "数值" << " |\n" << Color::RESET;
    std::cout << "|" << std::string(30, '-') << "-|" << std::string(45, '-') << "-|\n";
    std::cout << "| " << std::left << std::setw(30) << "总估计行数" 
              << " | " << std::right << std::setw(45) << ss_total_rows.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "平均每测试行数" 
              << " | " << std::right << std::setw(45) << ss_avg_rows.str() << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "总使用索引数" 
              << " | " << std::right << std::setw(45) << total_indexes << " |\n";
    std::cout << "| " << std::left << std::setw(30) << "平均每测试索引数" 
              << " | " << std::right << std::setw(45) << ss_avg_idx.str() << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    std::cout << "\n";
    std::cout << Color::GREEN << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(76) << "各测试性能对比" << " |\n";
    std::cout << "+" << std::string(78, '-') << "+\n" << Color::RESET;
    
    std::cout << Color::YELLOW << "| " << std::left << std::setw(6) << "测试" 
              << " | " << std::right << std::setw(15) << "成本" 
              << " | " << std::right << std::setw(12) << "时间(毫秒)" 
              << " | " << std::right << std::setw(12) << "行数" 
              << " | " << std::right << std::setw(8) << "表数" 
              << " | " << std::right << std::setw(8) << "索引" << " |\n" << Color::RESET;
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
        std::cerr << Color::RED << "错误: 无法打开文件: " << filename << Color::RESET << "\n";
        return;
    }
    
    file << "优化时间(毫秒),总成本,估计行数,表数量,使用索引数\n";
    for (const auto& m : metrics_) {
        file << m.optimization_time_ms << ","
             << m.total_cost << ","
             << m.estimated_rows << ","
             << m.num_tables << ","
             << m.num_indexes_used << "\n";
    }
    
    file.close();
    std::cout << Color::GREEN << "性能数据已保存到: " << Color::CYAN << filename << Color::RESET << "\n";
}

