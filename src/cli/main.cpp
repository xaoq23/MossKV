#include "../db/db.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

//使用匿名命名空间封装内部实现，避免符号冲突。
namespace {

// MossKV 版本号和默认数据库目录。
constexpr std::string_view kVersion = "0.1.1";
constexpr std::string_view kDefaultDatabasePath = "data";

// 将错误信息写入指定输出流，命令行错误统一写到 stderr。
auto print_error(std::ostream& output, const std::string& message) -> void {
    output << "Error: " << message << '\n';
}

// 打印支持的命令、选项及基本用法。
auto print_usage(std::ostream& output) -> void {
    output << "MossKV - A lightweight key-value store\n"
           << "Usage: mosskv [options] <command> [arguments]\n"
           << "\n"
           << "Commands:\n"
           << "  put <key> <value>  Store a key-value pair\n"
           << "  get <key>          Read a value\n"
           << "  delete <key>       Delete a key\n"
           << "  list               List all keys and values\n"
           << "\n"
           << "Options:\n"
           << "  -d, --db <path>    Database directory (default: data)\n"
           << "  -h, --help         Show this help message\n"
           << "  -V, --version      Show version information\n";
}

// 打印 MossKV 的名称和当前版本。
auto print_version(std::ostream& output) -> void {
    output << "mosskv " << kVersion << '\n';
}

// 从参数列表中提取 -d/--db，并从列表中移除该选项及其值。
// 返回 false 表示选项缺少路径等参数解析错误。
auto parse_database_option(
    std::vector<std::string>& arguments,
    std::string& database_path) -> bool {
    for (auto index = std::size_t{0}; index < arguments.size();) {
        const auto& argument = arguments[index];
        if (argument != "-d" && argument != "--db") {
            ++index;
            continue;
        }

        if (index + 1 >= arguments.size()) {
            print_error(std::cerr, "missing value for " + argument);
            return false;
        }

        database_path = arguments[index + 1];
        arguments.erase(arguments.begin() + static_cast<std::ptrdiff_t>(index),
                        arguments.begin() + static_cast<std::ptrdiff_t>(index + 2));
    }
    return true;
}

}  // namespace

// 命令行入口负责参数解析、打开数据库，并分派到对应的操作命令。
auto main(int argc, char* argv[]) -> int {
    // 将 argv 转换为字符串列表，方便后续查找选项和执行参数校验。
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (auto index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    // 没有参数时显示帮助，而不是尝试执行数据库命令。
    if (arguments.empty()) {
        print_usage(std::cout);
        return 0;
    }

    // 帮助和版本属于无数据库操作选项，不需要打开数据库目录。
    if (arguments.size() == 1 &&
        (arguments[0] == "-h" || arguments[0] == "--help")) {
        print_usage(std::cout);
        return 0;
    }
    if (arguments.size() == 1 &&
        (arguments[0] == "-V" || arguments[0] == "--version")) {
        print_version(std::cout);
        return 0;
    }

    // -d/--db 可放在命令前后；解析后剩余参数只包含待执行的命令。
    std::string database_path(kDefaultDatabasePath);
    if (!parse_database_option(arguments, database_path)) {
        return 1;
    }

    if (arguments.empty()) {
        print_error(std::cerr, "missing command");
        print_usage(std::cerr);
        return 1;
    }

    // 所有数据库命令执行前都先打开 WAL，并回放已有数据。
    mosskv::DB database(database_path);
    if (!database.Open()) {
        print_error(std::cerr, database.error());
        return 1;
    }

    const std::string& command = arguments[0];

    // put：严格要求键和值共两个参数，成功后写入 WAL 和内存表。
    if (command == "put") {
        if (arguments.size() != 3) {
            print_error(std::cerr, "usage: mosskv put <key> <value>");
            return 1;
        }
        if (!database.Put(arguments[1], arguments[2])) {
            print_error(std::cerr, database.error());
            return 1;
        }
        std::cout << "OK\n";
        return 0;
    }

    // get：键不存在时返回 NOT_FOUND 和退出码 2，与其他参数错误区分。
    if (command == "get") {
        if (arguments.size() != 2) {
            print_error(std::cerr, "usage: mosskv get <key>");
            return 1;
        }

        const auto value = database.Get(arguments[1]);
        if (!value.has_value()) {
            std::cout << "NOT_FOUND\n";
            return 2;
        }
        std::cout << *value << '\n';
        return 0;
    }

    // delete/del：通过追加删除记录实现持久化删除。
    if (command == "delete" || command == "del") {
        if (arguments.size() != 2) {
            print_error(std::cerr, "usage: mosskv delete <key>");
            return 1;
        }
        if (!database.Delete(arguments[1])) {
            print_error(std::cerr, database.error());
            return 1;
        }
        std::cout << "OK\n";
        return 0;
    }

    // list/ls：输出当前内存表中按键排序的全部键值对。
    if (command == "list" || command == "ls") {
        if (arguments.size() != 1) {
            print_error(std::cerr, "usage: mosskv list");
            return 1;
        }
        for (const auto& entry : database.List()) {
            std::cout << entry.first << '\t' << entry.second << '\n';
        }
        return 0;
    }

    // 未知命令不打开数据库语义之外的操作，而是打印错误和可用用法。
    print_error(std::cerr, "unknown command '" + command + "'");
    print_usage(std::cerr);
    return 1;
}
