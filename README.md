\# 🍃 MossKV



> \*\*A Lightweight, Edge-Optimized Embedded Key-Value Engine\*\*  

> 面向边缘设备与本地 AI 场景的高性能轻量化嵌入式 LSM-Tree 存储引擎，支持工作负载自适应 Compaction 与零拷贝读取。



\[!\[License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

\[!\[C++ Standard](https://img.shields.io/badge/c%2B%2B-17-blue.svg)]()

\[!\[Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)]()



\---



\## 🌟 核心特性 (Key Features)



\- \*\*LSM-Tree 核心架构\*\*：基于 Concurrent SkipList (MemTable) 与分层磁盘存储 (SSTable)，高并发顺序写吞吐大幅提升。

\- \*\*工作负载自适应 Compaction\*\*：基于运行时 Read/Write Ratio 动态切换 Size-Tiered 与 Leveled 策略，平衡写放大与读延迟。

\- \*\*边缘内存友好设计\*\*：支持 Bit-per-Key 可配置的动态 Bloom Filter，协同 LRU Block Cache 进行冷热数据内存淘汰。

\- \*\*零拷贝读路径 (Zero-Copy Read Path)\*\*：SSTable 数据读取深度集成 `mmap` 与 Arena 内存分配器，压低 P99 随机读延迟。

\- \*\*崩溃恢复与数据一致性\*\*：WAL 预写日志结合 CRC32 校验，支持 `sync` / `async` 刷盘模式与重启自动故障恢复 (Crash Recovery)。



\---



\## 🏗️ 系统架构 (Architecture)

