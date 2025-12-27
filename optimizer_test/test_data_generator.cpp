/**
 * 测试数据生成器
 * 生成用于测试SQL优化器的模拟数据
 */

#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>

class TestDataGenerator {
private:
    std::mt19937 rng_;
    
    // 生成随机字符串
    std::string random_string(size_t length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::uniform_int_distribution<> dist(0, chars.size() - 1);
        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += chars[dist(rng_)];
        }
        return result;
    }
    
    // 生成随机日期
    std::string random_date() {
        std::uniform_int_distribution<> year_dist(2020, 2024);
        std::uniform_int_distribution<> month_dist(1, 12);
        std::uniform_int_distribution<> day_dist(1, 28);
        
        int year = year_dist(rng_);
        int month = month_dist(rng_);
        int day = day_dist(rng_);
        
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(4) << year << "-"
            << std::setw(2) << month << "-" << std::setw(2) << day;
        return oss.str();
    }
    
public:
    TestDataGenerator(unsigned int seed = std::time(nullptr)) : rng_(seed) {}
    
    // 生成customers表数据
    void generate_customers(size_t count, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        std::uniform_int_distribution<> name_len_dist(5, 20);
        std::uniform_int_distribution<> email_len_dist(10, 30);
        
        file << "customer_id,customer_name,email,created_at\n";
        
        for (size_t i = 1; i <= count; ++i) {
            file << i << ","
                 << "Customer" << i << ","
                 << "customer" << i << "@example.com,"
                 << random_date() << "\n";
        }
        
        file.close();
        std::cout << "生成 " << count << " 条customers数据到 " << filename << "\n";
    }
    
    // 生成orders表数据
    void generate_orders(size_t count, size_t customer_count, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        std::uniform_int_distribution<> customer_dist(1, customer_count);
        std::uniform_real_distribution<> amount_dist(10.0, 10000.0);
        std::uniform_int_distribution<> status_dist(0, 3);
        const std::vector<std::string> statuses = {"pending", "processing", "shipped", "delivered"};
        
        file << "order_id,customer_id,order_date,amount,status\n";
        
        for (size_t i = 1; i <= count; ++i) {
            file << i << ","
                 << customer_dist(rng_) << ","
                 << random_date() << ","
                 << std::fixed << std::setprecision(2) << amount_dist(rng_) << ","
                 << statuses[status_dist(rng_)] << "\n";
        }
        
        file.close();
        std::cout << "生成 " << count << " 条orders数据到 " << filename << "\n";
    }
    
    // 生成products表数据
    void generate_products(size_t count, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        std::uniform_int_distribution<> category_dist(1, 20);
        std::uniform_real_distribution<> price_dist(1.0, 1000.0);
        
        file << "product_id,product_name,category_id,price\n";
        
        for (size_t i = 1; i <= count; ++i) {
            file << i << ","
                 << "Product" << i << ","
                 << category_dist(rng_) << ","
                 << std::fixed << std::setprecision(2) << price_dist(rng_) << "\n";
        }
        
        file.close();
        std::cout << "生成 " << count << " 条products数据到 " << filename << "\n";
    }
    
    // 生成order_items表数据
    void generate_order_items(size_t count, size_t order_count, size_t product_count, 
                             const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        std::uniform_int_distribution<> order_dist(1, order_count);
        std::uniform_int_distribution<> product_dist(1, product_count);
        std::uniform_int_distribution<> quantity_dist(1, 10);
        std::uniform_real_distribution<> price_dist(1.0, 500.0);
        
        file << "order_item_id,order_id,product_id,quantity,price\n";
        
        for (size_t i = 1; i <= count; ++i) {
            file << i << ","
                 << order_dist(rng_) << ","
                 << product_dist(rng_) << ","
                 << quantity_dist(rng_) << ","
                 << std::fixed << std::setprecision(2) << price_dist(rng_) << "\n";
        }
        
        file.close();
        std::cout << "生成 " << count << " 条order_items数据到 " << filename << "\n";
    }
    
    // 生成统计信息文件（用于优化器）
    void generate_statistics(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        file << "# 表统计信息\n";
        file << "# 格式: 表名,行数,平均行大小(字节),内存页比例\n\n";
        
        file << "customers,50000,150,0.8\n";
        file << "orders,1000000,200,0.3\n";
        file << "products,10000,100,0.9\n";
        file << "order_items,5000000,50,0.2\n";
        
        file << "\n# 索引统计信息\n";
        file << "# 格式: 索引名,表名,列名,选择性,内存页比例\n\n";
        
        file << "PRIMARY,customers,customer_id,1.0,0.9\n";
        file << "idx_email,customers,email,0.02,0.5\n";
        file << "PRIMARY,orders,order_id,1.0,0.3\n";
        file << "idx_customer_id,orders,customer_id,0.02,0.5\n";
        file << "idx_order_date,orders,order_date,0.1,0.4\n";
        file << "PRIMARY,products,product_id,1.0,0.9\n";
        file << "idx_category,products,category_id,0.05,0.6\n";
        file << "PRIMARY,order_items,order_item_id,1.0,0.2\n";
        file << "idx_order_id,order_items,order_id,0.2,0.3\n";
        file << "idx_product_id,order_items,product_id,0.1,0.4\n";
        
        file.close();
        std::cout << "生成统计信息到 " << filename << "\n";
    }
    
    // 生成测试查询SQL
    void generate_test_queries(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << "\n";
            return;
        }
        
        file << "-- SQL优化器测试查询集\n\n";
        
        file << "-- 测试1: 单表查询 - 索引选择\n";
        file << "SELECT * FROM orders WHERE customer_id = 12345;\n\n";
        
        file << "-- 测试2: 单表查询 - 范围查询\n";
        file << "SELECT * FROM orders WHERE order_date > '2023-01-01';\n\n";
        
        file << "-- 测试3: 两表连接\n";
        file << "SELECT o.order_id, c.customer_name\n";
        file << "FROM orders o JOIN customers c ON o.customer_id = c.customer_id\n";
        file << "WHERE o.order_date > '2023-01-01';\n\n";
        
        file << "-- 测试4: 三表连接\n";
        file << "SELECT o.order_id, c.customer_name, p.product_name\n";
        file << "FROM orders o\n";
        file << "JOIN customers c ON o.customer_id = c.customer_id\n";
        file << "JOIN order_items oi ON o.order_id = oi.order_id\n";
        file << "JOIN products p ON oi.product_id = p.product_id\n";
        file << "WHERE o.order_date > '2023-01-01';\n\n";
        
        file << "-- 测试5: 聚合查询\n";
        file << "SELECT customer_id, COUNT(*), SUM(amount)\n";
        file << "FROM orders\n";
        file << "WHERE order_date > '2023-01-01'\n";
        file << "GROUP BY customer_id\n";
        file << "HAVING COUNT(*) > 10;\n\n";
        
        file << "-- 测试6: 子查询\n";
        file << "SELECT * FROM customers\n";
        file << "WHERE customer_id IN (\n";
        file << "    SELECT customer_id FROM orders WHERE amount > 1000\n";
        file << ");\n\n";
        
        file << "-- 测试7: 复杂连接\n";
        file << "SELECT c.customer_name, COUNT(DISTINCT o.order_id), SUM(oi.quantity * oi.price)\n";
        file << "FROM customers c\n";
        file << "JOIN orders o ON c.customer_id = o.customer_id\n";
        file << "JOIN order_items oi ON o.order_id = oi.order_id\n";
        file << "JOIN products p ON oi.product_id = p.product_id\n";
        file << "WHERE o.order_date BETWEEN '2023-01-01' AND '2023-12-31'\n";
        file << "GROUP BY c.customer_id, c.customer_name\n";
        file << "HAVING SUM(oi.quantity * oi.price) > 5000\n";
        file << "ORDER BY SUM(oi.quantity * oi.price) DESC\n";
        file << "LIMIT 100;\n";
        
        file.close();
        std::cout << "生成测试查询到 " << filename << "\n";
    }
};

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "   测试数据生成器\n";
    std::cout << "========================================\n\n";
    
    TestDataGenerator generator;
    
    // 生成数据
    size_t customer_count = 50000;
    size_t order_count = 1000000;
    size_t product_count = 10000;
    size_t order_item_count = 5000000;
    
    if (argc > 1) {
        // 可以指定数据规模
        customer_count = std::stoul(argv[1]);
        order_count = std::stoul(argv[2]);
        product_count = std::stoul(argv[3]);
        order_item_count = std::stoul(argv[4]);
    }
    
    std::cout << "数据规模:\n";
    std::cout << "  customers: " << customer_count << "\n";
    std::cout << "  orders: " << order_count << "\n";
    std::cout << "  products: " << product_count << "\n";
    std::cout << "  order_items: " << order_item_count << "\n\n";
    
    generator.generate_customers(customer_count, "test_data/customers.csv");
    generator.generate_orders(order_count, customer_count, "test_data/orders.csv");
    generator.generate_products(product_count, "test_data/products.csv");
    generator.generate_order_items(order_item_count, order_count, product_count, 
                                  "test_data/order_items.csv");
    generator.generate_statistics("test_data/statistics.txt");
    generator.generate_test_queries("test_data/test_queries.sql");
    
    std::cout << "\n数据生成完成！\n";
    std::cout << "文件保存在 test_data/ 目录\n";
    
    return 0;
}

