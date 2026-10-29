#include "db.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace mosskv {

// 构造数据库路径，并由 wal_path_ 保存 WAL 文件的位置。
DB::DB(std::filesystem::path path)
    : path_(std::move(path)),
      wal_path_(path_ / "wal.log") {}

auto DB::error() const -> const std::string& {
    return error_;
}

auto DB::Open() -> bool {
    error_.clear();

    // 已打开时直接返回成功，避免重复创建目录或重复回放 WAL。
    if (opened_) {
        return true;
    }

    // 创建数据库目录；路径不存在时由 std::error_code 报告错误。
    auto error = std::error_code{};
    std::filesystem::create_directories(path_, error);
    if (error) {
        error_ = "cannot create database directory '" + path_.string() + "': " +
                 error.message();
        return false;
    }

    // 启动时先从 WAL 重放数据，再允许执行后续写操作。
    if (!replay_wal()) {
        return false;
    }

    opened_ = true;
    return true;
}

auto DB::append_record(
    RecordType type,
    const std::string& key,
    const std::string& value) -> bool {
    // WAL 使用 uint32_t 记录长度，超出范围时不能继续写入。
    if (key.size() > std::numeric_limits<std::uint32_t>::max() ||
        value.size() > std::numeric_limits<std::uint32_t>::max()) {
        error_ = "key or value is too large";
        return false;
    }

    // 每次追加一条二进制记录：类型、键长度、值长度、键数据、可选值数据。
    std::ofstream wal(wal_path_, std::ios::binary | std::ios::app);
    if (!wal) {
        error_ = "cannot open WAL '" + wal_path_.string() + "'";
        return false;
    }

    const auto record_type = static_cast<std::uint8_t>(type);
    const auto key_size = static_cast<std::uint32_t>(key.size());
    const auto value_size = static_cast<std::uint32_t>(value.size());

    wal.write(reinterpret_cast<const char*>(&record_type), sizeof(record_type));
    wal.write(reinterpret_cast<const char*>(&key_size), sizeof(key_size));
    wal.write(reinterpret_cast<const char*>(&value_size), sizeof(value_size));
    wal.write(key.data(), static_cast<std::streamsize>(key.size()));
    if (type == RecordType::Put) {
        wal.write(value.data(), static_cast<std::streamsize>(value.size()));
    }

    // flush 失败视为写入失败，避免随后把同一条操作提交到内存。
    wal.flush();
    if (!wal) {
        error_ = "cannot write WAL '" + wal_path_.string() + "'";
        return false;
    }

    return true;
}

auto DB::Put(const std::string& key, const std::string& value) -> bool {
    if (!opened_) {
        error_ = "database is not open";
        return false;
    }
    if (key.empty()) {
        error_ = "key cannot be empty";
        return false;
    }
    if (!append_record(RecordType::Put, key, value)) {
        return false;
    }

    // WAL 写入成功后再更新 memtable，保证失败时内存状态不变。
    memtable_[key] = value;
    return true;
}

auto DB::Delete(const std::string& key) -> bool {
    if (!opened_) {
        error_ = "database is not open";
        return false;
    }
    if (key.empty()) {
        error_ = "key cannot be empty";
        return false;
    }
    if (!append_record(RecordType::Delete, key, "")) {
        return false;
    }

    // 删除记录持久化成功后，再从内存删除键。
    memtable_.erase(key);
    return true;
}

auto DB::Get(const std::string& key) const -> std::optional<std::string> {
    const auto found = memtable_.find(key);
    if (found == memtable_.end()) {
        return std::nullopt;
    }
    return found->second;
}

auto DB::List() const -> std::vector<std::pair<std::string, std::string>> {
    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(memtable_.size());
    // std::map 按键排序，因此 List() 返回按键升序排列的快照。
    for (const auto& [entry_key, entry_value] : memtable_) {
        entries.emplace_back(entry_key, entry_value);
    }
    return entries;
}

auto DB::replay_wal() -> bool {
    // WAL 是唯一持久状态来源，回放前先清空旧的内存快照。
    memtable_.clear();
    std::ifstream wal(wal_path_, std::ios::binary);
    if (!wal) {
        // WAL 不存在时表示这是一个新数据库，不属于错误。
        const auto status = std::filesystem::status(wal_path_);
        if (std::filesystem::exists(status)) {
            error_ = "cannot open WAL '" + wal_path_.string() + "'";
            return false;
        }
        return true;
    }

    while (wal.peek() != std::ifstream::traits_type::eof()) {
        std::uint8_t record_type = 0;
        std::uint32_t key_size = 0;
        std::uint32_t value_size = 0;

        // 逐条读取固定长度的头部，再读取对应的键和值。
        wal.read(reinterpret_cast<char*>(&record_type), sizeof(record_type));
        wal.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        wal.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
        if (!wal) {
            error_ = "WAL header is truncated in '" + wal_path_.string() + "'";
            return false;
        }

        if (record_type != static_cast<std::uint8_t>(RecordType::Put) &&
            record_type != static_cast<std::uint8_t>(RecordType::Delete)) {
            error_ = "WAL contains an unknown record type in '" + wal_path_.string() + "'";
            return false;
        }

        std::string key(key_size, '\0');
        if (key_size != 0) {
            wal.read(key.data(), static_cast<std::streamsize>(key_size));
            if (!wal) {
                error_ = "WAL key is truncated in '" + wal_path_.string() + "'";
                return false;
            }
        }

        std::string value;
        if (record_type == static_cast<std::uint8_t>(RecordType::Put)) {
            value.resize(value_size);
            if (value_size != 0) {
                wal.read(value.data(), static_cast<std::streamsize>(value_size));
                if (!wal) {
                    error_ = "WAL value is truncated in '" + wal_path_.string() + "'";
                    return false;
                }
            }
            memtable_[key] = value;
        } else {
            memtable_.erase(key);
        }
    }

    return true;
}

}  // namespace mosskv
