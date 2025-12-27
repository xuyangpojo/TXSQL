/**
 * SQL优化器测试用例
 * 
 * 包含各种测试场景：
 * 1. 单表查询（索引选择）
 * 2. 两表连接（连接顺序优化）
 * 3. 多表连接（复杂场景）
 */

#include "optimizer.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <sstream>

// ==================== Table Formatting Utilities ====================

// Print horizontal line
void print_line(int width = 80, char ch = '-') {
    std::cout << std::string(width, ch) << "\n";
}

// Print header with border
void print_header(const std::string& title, int width = 80) {
    std::cout << "\n";
    print_line(width, '=');
    std::cout << "  " << title << "\n";
    print_line(width, '=');
}

// Print table header
void print_table_header(const std::vector<std::string>& headers, const std::vector<int>& widths) {
    std::cout << "|";
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << " " << std::left << std::setw(widths[i]) << headers[i] << " |";
    }
    std::cout << "\n";
    
    std::cout << "|";
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << std::string(widths[i] + 2, '-') << "|";
    }
    std::cout << "\n";
}

// Print table row
void print_table_row(const std::vector<std::string>& values, const std::vector<int>& widths) {
    std::cout << "|";
    for (size_t i = 0; i < values.size(); ++i) {
        std::cout << " " << std::left << std::setw(widths[i]) << values[i] << " |";
    }
    std::cout << "\n";
}

// Format number with commas
std::string format_number(size_t num) {
    std::stringstream ss;
    ss << num;
    std::string s = ss.str();
    for (int i = s.length() - 3; i > 0; i -= 3) {
        s.insert(i, ",");
    }
    return s;
}

// Format large number with K/M suffix
std::string format_large_number(size_t num) {
    if (num >= 1000000) {
        double m = num / 1000000.0;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << m << "M";
        return ss.str();
    } else if (num >= 1000) {
        double k = num / 1000.0;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << k << "K";
        return ss.str();
    }
    return std::to_string(num);
}

// Print table statistics in table format
void print_table_stats(const std::map<std::string, TableStats>& tables) {
    print_header("Table Statistics", 80);
    
    std::vector<std::string> headers = {"Table", "Rows", "Row Size", "Memory %", "Cardinality"};
    std::vector<int> widths = {15, 12, 10, 12, 20};
    print_table_header(headers, widths);
    
    for (const auto& pair : tables) {
        const TableStats& table = pair.second;
        std::string cardinality_str = "";
        if (!table.column_cardinality.empty()) {
            for (const auto& col : table.column_cardinality) {
                if (!cardinality_str.empty()) cardinality_str += ", ";
                cardinality_str += col.first + ":" + format_number(col.second);
            }
        } else {
            cardinality_str = "N/A";
        }
        
        std::vector<std::string> row = {
            table.name,
            format_large_number(table.row_count),
            std::to_string(table.avg_row_size) + " B",
            std::to_string(static_cast<int>(table.pages_in_memory * 100)) + "%",
            cardinality_str
        };
        print_table_row(row, widths);
    }
    print_line(80);
}

// Print index information in table format
void print_index_info(const std::map<std::string, std::vector<IndexStats>>& indexes) {
    if (indexes.empty()) {
        print_header("Index Information: None", 80);
        return;
    }
    
    print_header("Index Information", 80);
    
    std::vector<std::string> headers = {"Table", "Index Name", "Type", "Columns", "Selectivity", "Memory %"};
    std::vector<int> widths = {12, 18, 10, 20, 12, 10};
    print_table_header(headers, widths);
    
    for (const auto& pair : indexes) {
        for (const auto& idx : pair.second) {
            std::string type = "NORMAL";
            if (idx.is_primary) type = "PRIMARY";
            else if (idx.is_unique) type = "UNIQUE";
            
            std::string columns_str = "";
            for (size_t i = 0; i < idx.columns.size(); ++i) {
                if (i > 0) columns_str += ",";
                columns_str += idx.columns[i];
            }
            
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << idx.selectivity;
            
            std::vector<std::string> row = {
                pair.first,
                idx.name,
                type,
                columns_str,
                ss.str(),
                std::to_string(static_cast<int>(idx.pages_in_memory * 100)) + "%"
            };
            print_table_row(row, widths);
        }
    }
    print_line(80);
}

// Print query conditions in table format
void print_query_conditions(const std::vector<QueryCondition>& conditions) {
    if (conditions.empty()) {
        print_header("Query Conditions: None", 80);
        return;
    }
    
    print_header("Query Conditions", 80);
    
    std::vector<std::string> headers = {"Table", "Column", "Operator", "Selectivity"};
    std::vector<int> widths = {15, 20, 12, 15};
    print_table_header(headers, widths);
    
    for (const auto& cond : conditions) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(4) << cond.selectivity;
        
        std::vector<std::string> row = {
            cond.table_name,
            cond.column_name,
            cond.op,
            ss.str()
        };
        print_table_row(row, widths);
    }
    print_line(80);
}

// Print join conditions in table format
void print_join_conditions(const std::vector<JoinCondition>& joins) {
    if (joins.empty()) {
        print_header("Join Conditions: None", 80);
        return;
    }
    
    print_header("Join Conditions", 80);
    
    std::vector<std::string> headers = {"Left Table.Column", "Right Table.Column", "Selectivity"};
    std::vector<int> widths = {25, 25, 15};
    print_table_header(headers, widths);
    
    for (const auto& join : joins) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(4) << join.selectivity;
        
        std::vector<std::string> row = {
            join.left_table + "." + join.left_column,
            join.right_table + "." + join.right_column,
            ss.str()
        };
        print_table_row(row, widths);
    }
    print_line(80);
}

// 运行单个测试用例
void run_test_case(const std::string& test_name,
                  const std::map<std::string, TableStats>& tables,
                  const std::map<std::string, std::vector<IndexStats>>& indexes,
                  const std::vector<QueryCondition>& conditions,
                  const std::vector<JoinCondition>& joins,
                  CostModel& cost_model,
                  PerformanceCollector& collector,
                  bool verbose = true) {
    
    print_header(test_name, 80);
    
    if (verbose) {
        print_table_stats(tables);
        print_index_info(indexes);
        print_query_conditions(conditions);
        print_join_conditions(joins);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    JoinOrderOptimizer optimizer(cost_model);
    JoinPlan plan = optimizer.optimize_join_order(tables, indexes, conditions, joins);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double time_ms = duration.count() / 1000.0;
    
    // Print execution plan in table format
    print_header("Execution Plan", 80);
    
    // Summary box
    std::cout << "\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    std::cout << "| " << std::left << std::setw(20) << "Total Cost:" 
              << std::right << std::setw(55) << std::fixed << std::setprecision(2) 
              << plan.total_cost << " |\n";
    std::cout << "| " << std::left << std::setw(20) << "Join Type:" 
              << std::right << std::setw(55) << plan.join_type << " |\n";
    std::cout << "| " << std::left << std::setw(20) << "Optimization Time:" 
              << std::right << std::setw(55) << std::fixed << std::setprecision(3) 
              << time_ms << " ms |\n";
    std::cout << "+" << std::string(78, '-') << "+\n";
    
    // Execution steps table
    std::cout << "\n";
    std::vector<std::string> headers = {"Step", "Table", "Access Method", "Rows", "Step Cost", "Cumulative Cost"};
    std::vector<int> widths = {6, 15, 25, 12, 12, 15};
    print_table_header(headers, widths);
    
    size_t total_rows = 0;
    size_t indexes_used = 0;
    double cumulative_cost = 0.0;
    for (size_t i = 0; i < plan.table_order.size(); ++i) {
        if (i < plan.access_paths.size()) {
            const auto& path = plan.access_paths[i];
            cumulative_cost += path.total_cost;
            
            std::stringstream ss_rows, ss_step_cost, ss_cum_cost;
            ss_rows << format_large_number(path.estimated_rows);
            ss_step_cost << std::fixed << std::setprecision(2) << path.total_cost;
            ss_cum_cost << std::fixed << std::setprecision(2) << cumulative_cost;
            
            std::vector<std::string> row = {
                std::to_string(i + 1),
                plan.table_order[i],
                path.description,
                ss_rows.str(),
                ss_step_cost.str(),
                ss_cum_cost.str()
            };
            print_table_row(row, widths);
            
            total_rows += path.estimated_rows;
            if (path.access_type != "TABLE_SCAN") indexes_used++;
        }
    }
    print_line(80);
    
    // Summary table
    std::cout << "\n";
    std::vector<std::string> summary_headers = {"Metric", "Value"};
    std::vector<int> summary_widths = {25, 50};
    print_table_header(summary_headers, summary_widths);
    
    std::stringstream ss_total_rows, ss_cost_per_row;
    ss_total_rows << format_large_number(total_rows) 
                  << " (" << std::fixed << std::setprecision(2) 
                  << (total_rows / 1000000.0) << "M)";
    ss_cost_per_row << std::fixed << std::setprecision(6) 
                    << (plan.total_cost / total_rows);
    
    std::vector<std::string> summary_row1 = {"Total Estimated Rows", ss_total_rows.str()};
    std::vector<std::string> summary_row2 = {
        "Indexes Used", 
        std::to_string(indexes_used) + " / " + std::to_string(plan.access_paths.size())
    };
    std::vector<std::string> summary_row3 = {"Cost per Row", ss_cost_per_row.str()};
    
    print_table_row(summary_row1, summary_widths);
    print_table_row(summary_row2, summary_widths);
    print_table_row(summary_row3, summary_widths);
    print_line(80);
    
    // 记录性能指标
    PerformanceMetrics metrics;
    metrics.optimization_time_ms = time_ms;
    metrics.total_cost = plan.total_cost;
    metrics.estimated_rows = total_rows;
    metrics.num_tables = tables.size();
    metrics.num_indexes_used = indexes_used;
    collector.record(metrics);
}

// ==================== 测试用例定义 ====================

void test_single_table_query(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT * FROM orders WHERE customer_id = 12345\n";
    
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
    
    run_test_case("Test 1: Single Table Query (Index vs Table Scan)",
                 tables, indexes, conditions, {}, cost_model, collector, true);
}

void test_two_table_join(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT o.order_id, c.customer_name\n";
    std::cout << "     FROM orders o JOIN customers c ON o.customer_id = c.customer_id\n";
    std::cout << "     WHERE o.order_date > '2023-01-01'\n";
    
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
    
    run_test_case("Test 2: Two Table Join (Testing Join Order)",
                 tables, indexes, conditions, joins, cost_model, collector, true);
}

void test_multi_table_join(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT o.order_id, c.customer_name, p.product_name\n";
    std::cout << "     FROM orders o\n";
    std::cout << "     JOIN customers c ON o.customer_id = c.customer_id\n";
    std::cout << "     JOIN order_items oi ON o.order_id = oi.order_id\n";
    std::cout << "     JOIN products p ON oi.product_id = p.product_id\n";
    
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
    
    run_test_case("Test 3: Multi-Table Join (4 Tables)",
                 tables, indexes, {}, joins, cost_model, collector, true);
}

void test_range_query(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT * FROM orders WHERE order_date BETWEEN '2023-01-01' AND '2023-12-31'\n";
    
    std::map<std::string, TableStats> tables;
    TableStats orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.3;
    orders.column_cardinality["order_date"] = 365;  // Days in a year
    tables["orders"] = orders;
    
    std::map<std::string, std::vector<IndexStats>> indexes;
    IndexStats idx_date;
    idx_date.name = "idx_order_date";
    idx_date.table_name = "orders";
    idx_date.columns = {"order_date"};
    idx_date.is_unique = false;
    idx_date.pages_in_memory = 0.4;
    idx_date.selectivity = 1.0 / 365.0;  // One day selectivity
    indexes["orders"] = {idx_date};
    
    std::vector<QueryCondition> conditions;
    QueryCondition cond;
    cond.table_name = "orders";
    cond.column_name = "order_date";
    cond.op = "BETWEEN";
    cond.selectivity = 365.0 / 1000000.0;  // Full year range
    conditions.push_back(cond);
    
    run_test_case("Test 4: Range Query (Date Range)",
                 tables, indexes, conditions, {}, cost_model, collector, true);
}

void test_multiple_indexes(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT * FROM orders WHERE customer_id = 12345 AND status = 'shipped'\n";
    
    std::map<std::string, TableStats> tables;
    TableStats orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.3;
    orders.column_cardinality["customer_id"] = 50000;
    orders.column_cardinality["status"] = 4;  // 4 status values
    tables["orders"] = orders;
    
    std::map<std::string, std::vector<IndexStats>> indexes;
    
    IndexStats idx_customer;
    idx_customer.name = "idx_customer_id";
    idx_customer.table_name = "orders";
    idx_customer.columns = {"customer_id"};
    idx_customer.is_unique = false;
    idx_customer.pages_in_memory = 0.5;
    idx_customer.selectivity = 1.0 / 50000.0;
    
    IndexStats idx_status;
    idx_status.name = "idx_status";
    idx_status.table_name = "orders";
    idx_status.columns = {"status"};
    idx_status.is_unique = false;
    idx_status.pages_in_memory = 0.3;
    idx_status.selectivity = 1.0 / 4.0;
    
    indexes["orders"] = {idx_customer, idx_status};
    
    std::vector<QueryCondition> conditions;
    QueryCondition cond1;
    cond1.table_name = "orders";
    cond1.column_name = "customer_id";
    cond1.op = "=";
    cond1.selectivity = 1.0 / 50000.0;
    conditions.push_back(cond1);
    
    QueryCondition cond2;
    cond2.table_name = "orders";
    cond2.column_name = "status";
    cond2.op = "=";
    cond2.selectivity = 1.0 / 4.0;
    conditions.push_back(cond2);
    
    run_test_case("Test 5: Multiple Indexes (Best Index Selection)",
                 tables, indexes, conditions, {}, cost_model, collector, true);
}

void test_large_table_join(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT * FROM large_table l JOIN small_table s ON l.id = s.id\n";
    
    std::map<std::string, TableStats> tables;
    
    TableStats large_table;
    large_table.name = "large_table";
    large_table.row_count = 10000000;  // 10M rows
    large_table.avg_row_size = 500;
    large_table.pages_in_memory = 0.1;  // Mostly on disk
    tables["large_table"] = large_table;
    
    TableStats small_table;
    small_table.name = "small_table";
    small_table.row_count = 1000;  // 1K rows
    small_table.avg_row_size = 100;
    small_table.pages_in_memory = 0.95;  // Mostly in memory
    tables["small_table"] = small_table;
    
    std::map<std::string, std::vector<IndexStats>> indexes;
    
    IndexStats idx_large;
    idx_large.name = "PRIMARY";
    idx_large.table_name = "large_table";
    idx_large.columns = {"id"};
    idx_large.is_unique = true;
    idx_large.is_primary = true;
    idx_large.pages_in_memory = 0.2;
    indexes["large_table"] = {idx_large};
    
    IndexStats idx_small;
    idx_small.name = "PRIMARY";
    idx_small.table_name = "small_table";
    idx_small.columns = {"id"};
    idx_small.is_unique = true;
    idx_small.is_primary = true;
    idx_small.pages_in_memory = 0.98;
    indexes["small_table"] = {idx_small};
    
    std::vector<JoinCondition> joins;
    JoinCondition join;
    join.left_table = "large_table";
    join.left_column = "id";
    join.right_table = "small_table";
    join.right_column = "id";
    join.selectivity = 1.0;
    joins.push_back(join);
    
    run_test_case("Test 6: Large-Small Table Join (Size Difference)",
                 tables, indexes, {}, joins, cost_model, collector, true);
}

void test_complex_query(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << "\nSQL: SELECT c.name, COUNT(o.id), SUM(o.amount)\n";
    std::cout << "     FROM customers c\n";
    std::cout << "     JOIN orders o ON c.id = o.customer_id\n";
    std::cout << "     WHERE o.date > '2023-01-01' AND c.country = 'US'\n";
    std::cout << "     GROUP BY c.id\n";
    
    std::map<std::string, TableStats> tables;
    
    TableStats customers;
    customers.name = "customers";
    customers.row_count = 100000;
    customers.avg_row_size = 150;
    customers.pages_in_memory = 0.6;
    customers.column_cardinality["country"] = 50;
    tables["customers"] = customers;
    
    TableStats orders;
    orders.name = "orders";
    orders.row_count = 5000000;  // 5M orders
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.2;
    orders.column_cardinality["customer_id"] = 100000;
    orders.column_cardinality["date"] = 1825;  // 5 years
    tables["orders"] = orders;
    
    std::map<std::string, std::vector<IndexStats>> indexes;
    
    IndexStats idx_customer_country;
    idx_customer_country.name = "idx_country";
    idx_customer_country.table_name = "customers";
    idx_customer_country.columns = {"country"};
    idx_customer_country.is_unique = false;
    idx_customer_country.pages_in_memory = 0.5;
    idx_customer_country.selectivity = 1.0 / 50.0;
    indexes["customers"] = {idx_customer_country};
    
    IndexStats idx_order_customer;
    idx_order_customer.name = "idx_customer_id";
    idx_order_customer.table_name = "orders";
    idx_order_customer.columns = {"customer_id"};
    idx_order_customer.is_unique = false;
    idx_order_customer.pages_in_memory = 0.3;
    idx_order_customer.selectivity = 1.0 / 100000.0;
    
    IndexStats idx_order_date;
    idx_order_date.name = "idx_order_date";
    idx_order_date.table_name = "orders";
    idx_order_date.columns = {"date"};
    idx_order_date.is_unique = false;
    idx_order_date.pages_in_memory = 0.4;
    idx_order_date.selectivity = 1.0 / 1825.0;
    
    indexes["orders"] = {idx_order_customer, idx_order_date};
    
    std::vector<QueryCondition> conditions;
    QueryCondition cond1;
    cond1.table_name = "orders";
    cond1.column_name = "date";
    cond1.op = ">";
    cond1.selectivity = 0.4;  // 40% of orders after 2023-01-01
    conditions.push_back(cond1);
    
    QueryCondition cond2;
    cond2.table_name = "customers";
    cond2.column_name = "country";
    cond2.op = "=";
    cond2.selectivity = 1.0 / 50.0;  // US customers
    conditions.push_back(cond2);
    
    std::vector<JoinCondition> joins;
    JoinCondition join;
    join.left_table = "customers";
    join.left_column = "id";
    join.right_table = "orders";
    join.right_column = "customer_id";
    join.selectivity = 1.0;
    joins.push_back(join);
    
    run_test_case("Test 7: Complex Query (Multiple Conditions + Join)",
                 tables, indexes, conditions, joins, cost_model, collector, true);
}

// ==================== 主函数 ====================

int main() {
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "                    SQL OPTIMIZER TEST PROGRAM\n";
    std::cout << "              Based on TXSQL Optimizer Core Algorithm\n";
    std::cout << "================================================================================\n";
    
    // 创建成本模型
    CostModel cost_model;
    
    // 性能收集器
    PerformanceCollector collector;
    
    // Run test cases
    test_single_table_query(cost_model, collector);
    test_two_table_join(cost_model, collector);
    test_multi_table_join(cost_model, collector);
    test_range_query(cost_model, collector);
    test_multiple_indexes(cost_model, collector);
    test_large_table_join(cost_model, collector);
    test_complex_query(cost_model, collector);
    
    // 打印性能统计
    collector.print_summary();
    
    // 保存性能数据
    collector.save_to_file("optimizer_performance.csv");
    
    std::cout << "\n========== Test Completed ==========\n";
    std::cout << "Notes:\n";
    std::cout << "1. Lower cost values are better\n";
    std::cout << "2. Optimizer selects the execution plan with lowest cost\n";
    std::cout << "3. Index access is usually cheaper than table scan\n";
    std::cout << "4. Join order affects total cost\n";
    
    return 0;
}

