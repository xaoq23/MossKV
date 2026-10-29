#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mosskv {

// 非线程安全；多线程访问同一个实例时需要外部同步。
class DB {
public:
    explicit DB(std::filesystem::path path);

    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;
    DB(DB&&) = delete;
    DB& operator=(DB&&) = delete;

    // 打开数据库并回放 WAL。重复调用是幂等的。
    auto Open() -> bool;

    // 未调用 Open 时调用以下接口会返回 false 并设置 error()。
    // 写入失败时 memtable_ 保持不变；WAL 写入成功后才更新内存状态。
    auto Put(const std::string& key, const std::string& value) -> bool;
    auto Delete(const std::string& key) -> bool;

    // 成功返回值；键不存在返回 std::nullopt。
    // 该操作只读取内存，不修改 error()。
    auto Get(const std::string& key) const -> std::optional<std::string>;

    // 返回当前 memtable 的全量快照。
    auto List() const -> std::vector<std::pair<std::string, std::string>>;

    // 返回最近一次操作的错误信息。
    // 引用在下一次修改 error_ 的操作或对象销毁后失效，请勿长期保存。
    auto error() const -> const std::string&;

private:
    enum class RecordType : std::uint8_t {
        Put = 1,
        Delete = 2,
    };

    auto append_record(RecordType type, const std::string& key,
                       const std::string& value) -> bool;
    auto replay_wal() -> bool;

    std::filesystem::path path_;
    std::filesystem::path wal_path_;
    std::string error_;
    std::map<std::string, std::string> memtable_;
    bool opened_ = false;
};

}  // namespace mosskv
