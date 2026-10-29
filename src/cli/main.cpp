#include "../db/db.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kVersion = "0.1.0";
constexpr std::string_view kDefaultDatabasePath = "data";

void print_error(std::ostream& output, const std::string& message) {
    output << "Error: " << message << '\n';
}

void print_usage(std::ostream& output) {
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

void print_version(std::ostream& output) {
    output << "mosskv " << kVersion << '\n';
}

bool parse_database_option(
    std::vector<std::string>& arguments,
    std::string* database_path) {
    for (std::size_t index = 0; index < arguments.size();) {
        const std::string& argument = arguments[index];
        if (argument != "-d" && argument != "--db") {
            ++index;
            continue;
        }

        if (index + 1 >= arguments.size()) {
            print_error(std::cerr, "missing value for " + argument);
            return false;
        }

        *database_path = arguments[index + 1];
        arguments.erase(arguments.begin() + static_cast<std::ptrdiff_t>(index),
                        arguments.begin() + static_cast<std::ptrdiff_t>(index + 2));
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    if (arguments.empty()) {
        print_usage(std::cout);
        return 0;
    }

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

    std::string database_path(kDefaultDatabasePath);
    if (!parse_database_option(arguments, &database_path)) {
        return 1;
    }

    if (arguments.empty()) {
        print_error(std::cerr, "missing command");
        print_usage(std::cerr);
        return 1;
    }

    mosskv::DB database(database_path);
    if (!database.Open()) {
        print_error(std::cerr, database.error());
        return 1;
    }

    const std::string& command = arguments[0];
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

    if (command == "get") {
        if (arguments.size() != 2) {
            print_error(std::cerr, "usage: mosskv get <key>");
            return 1;
        }

        std::string value;
        if (!database.Get(arguments[1], &value)) {
            std::cout << "NOT_FOUND\n";
            return 2;
        }
        std::cout << value << '\n';
        return 0;
    }

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

    print_error(std::cerr, "unknown command '" + command + "'");
    print_usage(std::cerr);
    return 1;
}
