#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mosskv {

class DB {
public:
    explicit DB(std::string path);
    bool Open();
    bool Put(const std::string& key, const std::string& value);
    bool Delete(const std::string& key);
    bool Get(const std::string& key, std::string* value) const;
    std::vector<std::pair<std::string, std::string>> List() const;
    const std::string& error() const;

private:
    enum class RecordType : std::uint8_t {
        Put = 1,
        Delete = 2,
    };

    bool append_record(RecordType type, const std::string& key, const std::string& value);
    bool replay_wal();

    std::string path_;
    std::string wal_path_;
    std::string error_;
    std::map<std::string, std::string> memtable_;
    bool opened_ = false;
};

}  // namespace mosskv
