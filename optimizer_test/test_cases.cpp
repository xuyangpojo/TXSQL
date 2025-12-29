#include "optimizer.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <sstream>
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
    
    void init() {
        SetConsoleOutputCP(65001);
        SetConsoleCP(65001);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}

void print_line(int width = 80, char ch = '-') {
    std::cout << std::string(width, ch) << "\n";
}

void print_header(const std::string& title, int width = 80) {
    std::cout << "\n";
    std::cout << Color::CYAN << Color::BOLD;
    print_line(width, '=');
    std::cout << "  " << title << "\n";
    print_line(width, '=');
    std::cout << Color::RESET;
}

void print_table_header(const std::vector<std::string>& headers, const std::vector<int>& widths) {
    std::cout << Color::YELLOW << "|";
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << " " << std::left << std::setw(widths[i]) << headers[i] << " |";
    }
    std::cout << Color::RESET << "\n";
    
    std::cout << "|";
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << std::string(widths[i] + 2, '-') << "|";
    }
    std::cout << "\n";
}

void print_table_row(const std::vector<std::string>& values, const std::vector<int>& widths) {
    std::cout << "|";
    for (size_t i = 0; i < values.size(); ++i) {
        std::cout << " " << std::left << std::setw(widths[i]) << values[i] << " |";
    }
    std::cout << "\n";
}

std::string format_number(size_t num) {
    std::stringstream ss;
    ss << num;
    std::string s = ss.str();
    for (int i = s.length() - 3; i > 0; i -= 3) {
        s.insert(i, ",");
    }
    return s;
}

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

void print_table_stats(const std::map<std::string, TableStats>& tables) {
    print_header("表统计信息", 80);
    
    std::vector<std::string> headers = {"表名", "行数", "行大小", "内存占比", "基数"};
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
            cardinality_str = "无";
        }
        
        std::vector<std::string> row = {
            table.name,
            format_large_number(table.row_count),
            std::to_string(table.avg_row_size) + " 字节",
            std::to_string(static_cast<int>(table.pages_in_memory * 100)) + "%",
            cardinality_str
        };
        print_table_row(row, widths);
    }
    print_line(80);
}

void print_index_info(const std::map<std::string, std::vector<IndexStats>>& indexes) {
    if (indexes.empty()) {
        print_header("索引信息: 无", 80);
        return;
    }
    
    print_header("索引信息", 80);
    
    std::vector<std::string> headers = {"表名", "索引名", "类型", "列名", "选择性", "内存占比"};
    std::vector<int> widths = {12, 18, 10, 20, 12, 10};
    print_table_header(headers, widths);
    
    for (const auto& pair : indexes) {
        for (const auto& idx : pair.second) {
            std::string type = "普通";
            if (idx.is_primary) type = "主键";
            else if (idx.is_unique) type = "唯一";
            
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

void print_query_conditions(const std::vector<QueryCondition>& conditions) {
    if (conditions.empty()) {
        print_header("查询条件: 无", 80);
        return;
    }
    
    print_header("查询条件", 80);
    
    std::vector<std::string> headers = {"表名", "列名", "操作符", "选择性"};
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

void print_join_conditions(const std::vector<JoinCondition>& joins) {
    if (joins.empty()) {
        print_header("连接条件: 无", 80);
        return;
    }
    
    print_header("连接条件", 80);
    
    std::vector<std::string> headers = {"左表.列", "右表.列", "选择性"};
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

void run_test_case(const std::string& test_name,
                  const std::map<std::string, TableStats>& tables,
                  const std::map<std::string, std::vector<IndexStats>>& indexes,
                  const std::vector<QueryCondition>& conditions,
                  const std::vector<JoinCondition>& joins,
                  CostModel& cost_model,
                  PerformanceCollector& collector,
                  bool verbose = true) {
    
    std::cout << "\n" << std::string(80, '-') << "\n";
    std::cout << Color::CYAN << Color::BOLD << test_name << Color::RESET << "\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    JoinOrderOptimizer optimizer(cost_model);
    JoinPlan plan = optimizer.optimize_join_order(tables, indexes, conditions, joins);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double time_ms = duration.count() / 1000.0;
    
    size_t total_rows = 0;
    size_t indexes_used = 0;
    
    std::cout << "执行计划: ";
    for (size_t i = 0; i < plan.table_order.size(); ++i) {
        if (i > 0) std::cout << " -> ";
        if (i < plan.access_paths.size()) {
            const auto& path = plan.access_paths[i];
            total_rows += path.estimated_rows;
            if (path.access_type != "TABLE_SCAN") indexes_used++;
            
            std::string access_desc = path.description;
            if (path.access_type == "TABLE_SCAN") {
                std::cout << Color::RED << plan.table_order[i] << "(全表扫描)" << Color::RESET;
            } else {
                std::cout << Color::GREEN << plan.table_order[i] << "(" << access_desc << ")" << Color::RESET;
            }
        } else {
            std::cout << plan.table_order[i];
        }
    }
    std::cout << "\n";
    
    std::cout << "总成本: " << Color::YELLOW << std::fixed << std::setprecision(2) << plan.total_cost 
              << Color::RESET << " | "
              << "估计行数: " << format_large_number(total_rows) << " | "
              << "索引使用: " << indexes_used << "/" << plan.access_paths.size() << " | "
              << "耗时: " << std::fixed << std::setprecision(3) << time_ms << "ms\n";
    
    PerformanceMetrics metrics;
    metrics.optimization_time_ms = time_ms;
    metrics.total_cost = plan.total_cost;
    metrics.estimated_rows = total_rows;
    metrics.num_tables = tables.size();
    metrics.num_indexes_used = indexes_used;
    collector.record(metrics);
}

void test_single_table_query(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT * FROM orders WHERE customer_id = 12345\n";
    
    std::map<std::string, TableStats> tables;
    TableStats orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.3;
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
    
    run_test_case("测试 1: 单表查询 (索引 vs 全表扫描)",
                 tables, indexes, conditions, {}, cost_model, collector, true);
}

void test_two_table_join(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT o.order_id, c.customer_name\n"
              << "     FROM orders o JOIN customers c ON o.customer_id = c.customer_id\n"
              << "     WHERE o.order_date > '2023-01-01'\n";
    
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
    customers.pages_in_memory = 0.8;
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
    
    run_test_case("测试 2: 两表连接 (测试连接顺序)",
                 tables, indexes, conditions, joins, cost_model, collector, true);
}

void test_multi_table_join(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT o.order_id, c.customer_name, p.product_name\n"
              << "     FROM orders o\n"
              << "     JOIN customers c ON o.customer_id = c.customer_id\n"
              << "     JOIN order_items oi ON o.order_id = oi.order_id\n"
              << "     JOIN products p ON oi.product_id = p.product_id\n";
    
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
    customers.pages_in_memory = 0.8;
    tables["customers"] = customers;
    
    TableStats order_items;
    order_items.name = "order_items";
    order_items.row_count = 5000000;
    order_items.avg_row_size = 50;
    order_items.pages_in_memory = 0.2;
    tables["order_items"] = order_items;
    
    TableStats products;
    products.name = "products";
    products.row_count = 10000;
    products.avg_row_size = 200;
    products.pages_in_memory = 0.9;
    tables["products"] = products;
    
    std::map<std::string, std::vector<IndexStats>> indexes;
    
    std::vector<JoinCondition> joins;
    JoinCondition j1;
    j1.left_table = "orders";
    j1.left_column = "customer_id";
    j1.right_table = "customers";
    j1.right_column = "customer_id";
    j1.selectivity = 1.0;
    joins.push_back(j1);
    
    JoinCondition j2;
    j2.left_table = "orders";
    j2.left_column = "order_id";
    j2.right_table = "order_items";
    j2.right_column = "order_id";
    j2.selectivity = 1.0;
    joins.push_back(j2);
    
    JoinCondition j3;
    j3.left_table = "order_items";
    j3.left_column = "product_id";
    j3.right_table = "products";
    j3.right_column = "product_id";
    j3.selectivity = 1.0;
    joins.push_back(j3);
    
    run_test_case("测试 3: 多表连接 (4个表)",
                 tables, indexes, {}, joins, cost_model, collector, true);
}

void test_range_query(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT * FROM orders WHERE order_date BETWEEN '2023-01-01' AND '2023-12-31'\n";
    
    std::map<std::string, TableStats> tables;
    TableStats orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.3;
    orders.column_cardinality["order_date"] = 365;
    tables["orders"] = orders;
    
    std::map<std::string, std::vector<IndexStats>> indexes;
    IndexStats idx_date;
    idx_date.name = "idx_order_date";
    idx_date.table_name = "orders";
    idx_date.columns = {"order_date"};
    idx_date.is_unique = false;
    idx_date.pages_in_memory = 0.4;
    idx_date.selectivity = 1.0 / 365.0;
    indexes["orders"] = {idx_date};
    
    std::vector<QueryCondition> conditions;
    QueryCondition cond;
    cond.table_name = "orders";
    cond.column_name = "order_date";
    cond.op = "BETWEEN";
    cond.selectivity = 365.0 / 1000000.0;
    conditions.push_back(cond);
    
    run_test_case("测试 4: 范围查询 (日期范围)",
                 tables, indexes, conditions, {}, cost_model, collector, true);
}

void test_multiple_indexes(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT * FROM orders WHERE customer_id = 12345 AND status = 'shipped'\n";
    
    std::map<std::string, TableStats> tables;
    TableStats orders;
    orders.name = "orders";
    orders.row_count = 1000000;
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.3;
    orders.column_cardinality["customer_id"] = 50000;
    orders.column_cardinality["status"] = 4;
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
    
    run_test_case("测试 5: 多索引选择 (最佳索引选择)",
                 tables, indexes, conditions, {}, cost_model, collector, true);
}

void test_large_table_join(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT * FROM large_table l JOIN small_table s ON l.id = s.id\n";
    
    std::map<std::string, TableStats> tables;
    
    TableStats large_table;
    large_table.name = "large_table";
    large_table.row_count = 10000000;
    large_table.avg_row_size = 500;
    large_table.pages_in_memory = 0.1;
    tables["large_table"] = large_table;
    
    TableStats small_table;
    small_table.name = "small_table";
    small_table.row_count = 1000;
    small_table.avg_row_size = 100;
    small_table.pages_in_memory = 0.95;
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
    
    run_test_case("测试 6: 大小表连接 (表大小差异)",
                 tables, indexes, {}, joins, cost_model, collector, true);
}

void test_complex_query(CostModel& cost_model, PerformanceCollector& collector) {
    std::cout << Color::BLUE << "SQL: " << Color::RESET 
              << "SELECT c.name, COUNT(o.id), SUM(o.amount)\n"
              << "     FROM customers c\n"
              << "     JOIN orders o ON c.id = o.customer_id\n"
              << "     WHERE o.date > '2023-01-01' AND c.country = 'US'\n"
              << "     GROUP BY c.id\n";
    
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
    orders.row_count = 5000000;
    orders.avg_row_size = 200;
    orders.pages_in_memory = 0.2;
    orders.column_cardinality["customer_id"] = 100000;
    orders.column_cardinality["date"] = 1825;
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
    cond1.selectivity = 0.4;
    conditions.push_back(cond1);
    
    QueryCondition cond2;
    cond2.table_name = "customers";
    cond2.column_name = "country";
    cond2.op = "=";
    cond2.selectivity = 1.0 / 50.0;
    conditions.push_back(cond2);
    
    std::vector<JoinCondition> joins;
    JoinCondition join;
    join.left_table = "customers";
    join.left_column = "id";
    join.right_table = "orders";
    join.right_column = "customer_id";
    join.selectivity = 1.0;
    joins.push_back(join);
    
    run_test_case("测试 7: 复杂查询 (多条件 + 连接)",
                 tables, indexes, conditions, joins, cost_model, collector, true);
}

int main() {
    Color::init();
    
    std::cout << "\n";
    std::cout << Color::CYAN << Color::BOLD;
    std::cout << "================================================================================\n";
    std::cout << "                    SQL 优化器测试程序\n";
    std::cout << "              基于 TXSQL 优化器核心算法\n";
    std::cout << "================================================================================\n";
    std::cout << Color::RESET;
    
    CostModel cost_model;
    PerformanceCollector collector;
    
    test_single_table_query(cost_model, collector);
    test_two_table_join(cost_model, collector);
    test_multi_table_join(cost_model, collector);
    test_range_query(cost_model, collector);
    test_multiple_indexes(cost_model, collector);
    test_large_table_join(cost_model, collector);
    test_complex_query(cost_model, collector);
    
    collector.print_summary();
    collector.save_to_file("optimizer_performance.csv");
    
    std::cout << "\n" << Color::GREEN << Color::BOLD;
    std::cout << "========== 测试完成 ==========\n";
    std::cout << Color::RESET;
    std::cout << Color::YELLOW << "说明:\n";
    std::cout << "1. 成本值越低越好\n";
    std::cout << "2. 优化器选择成本最低的执行计划\n";
    std::cout << "3. 索引访问通常比全表扫描更便宜\n";
    std::cout << "4. 连接顺序影响总成本\n" << Color::RESET;
    
    return 0;
}

