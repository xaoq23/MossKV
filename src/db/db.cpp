#include "db.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace mosskv {

DB::DB(std::string path) : path_(std::move(path)) {
    wal_path_ = (std::filesystem::path(path_) / "wal.log").string();
}

const std::string& DB::error() const {
    return error_;
}

bool DB::Open() {
    error_.clear();

    std::error_code error;
    std::filesystem::create_directories(path_, error);
    if (error) {
        error_ = "cannot create database directory '" + path_ + "': " + error.message();
        return false;
    }

    if (!replay_wal()) {
        return false;
    }

    opened_ = true;
    return true;
}

bool DB::append_record(RecordType type, const std::string& key, const std::string& value) {
    if (key.size() > std::numeric_limits<std::uint32_t>::max() ||
        value.size() > std::numeric_limits<std::uint32_t>::max()) {
        error_ = "key or value is too large";
        return false;
    }

    std::ofstream wal(wal_path_, std::ios::binary | std::ios::app);
    if (!wal) {
        error_ = "cannot open WAL '" + wal_path_ + "'";
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

    wal.flush();
    if (!wal) {
        error_ = "cannot write WAL '" + wal_path_ + "'";
        return false;
    }

    return true;
}

bool DB::Put(const std::string& key, const std::string& value) {
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

    memtable_[key] = value;
    return true;
}

bool DB::Delete(const std::string& key) {
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

    memtable_.erase(key);
    return true;
}

bool DB::Get(const std::string& key, std::string* value) const {
    const auto found = memtable_.find(key);
    if (found == memtable_.end()) {
        return false;
    }

    if (value != nullptr) {
        *value = found->second;
    }
    return true;
}

std::vector<std::pair<std::string, std::string>> DB::List() const {
    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(memtable_.size());
    for (const auto& entry : memtable_) {
        entries.emplace_back(entry);
    }
    return entries;
}

bool DB::replay_wal() {
    memtable_.clear();
    std::ifstream wal(wal_path_, std::ios::binary);
    if (!wal) {
        const auto status = std::filesystem::status(wal_path_);
        if (std::filesystem::exists(status)) {
            error_ = "cannot open WAL '" + wal_path_ + "'";
            return false;
        }
        return true;
    }

    while (wal.peek() != std::ifstream::traits_type::eof()) {
        std::uint8_t record_type = 0;
        std::uint32_t key_size = 0;
        std::uint32_t value_size = 0;
        wal.read(reinterpret_cast<char*>(&record_type), sizeof(record_type));
        wal.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        wal.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
        if (!wal) {
            error_ = "WAL header is truncated in '" + wal_path_ + "'";
            return false;
        }

        if (record_type != static_cast<std::uint8_t>(RecordType::Put) &&
            record_type != static_cast<std::uint8_t>(RecordType::Delete)) {
            error_ = "WAL contains an unknown record type in '" + wal_path_ + "'";
            return false;
        }

        std::string key(key_size, '\0');
        if (key_size != 0) {
            wal.read(key.data(), static_cast<std::streamsize>(key_size));
            if (!wal) {
                error_ = "WAL key is truncated in '" + wal_path_ + "'";
                return false;
            }
        }

        std::string value;
        if (record_type == static_cast<std::uint8_t>(RecordType::Put)) {
            value.resize(value_size);
            if (value_size != 0) {
                wal.read(value.data(), static_cast<std::streamsize>(value_size));
                if (!wal) {
                    error_ = "WAL value is truncated in '" + wal_path_ + "'";
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
