# 🍃 MossKV

**A Lightweight, Edge-Optimized Embedded Key-Value Engine**  

面向边缘设备与本地 AI 场景的高性能轻量化嵌入式 LSM-Tree 存储引擎，支持工作负载自适应 Compaction 与零拷贝读取。

*license*:Apache License Version 2.0, January 2004 http://www.apache.org/licenses/

## 项目目录

```
src/
├── cli/         # main.cpp，命令行入口，不含引擎逻辑
├── db/          # DB 门面：读写路径编排、WriteBatch、Snapshot
├── wal/         # 预写日志 writer/reader
├── memtable/    # 跳表、arena 内存池
├── sstable/     # SST 构建/读取、block、bloom filter
├── compaction/  # 工作负载自适应 compaction 策略
├── version/     # manifest、版本与层级元数据
└── util/        # status、varint 编码、crc32、file/env 抽象
```

## 构建命令



```powershell
安装cizon(未发布)
```

Debug 构建：

```powershell
cizon build
```

Release 构建：

```powershell
cizon build --release
```

运行程序：

```powershell
cizon run
cizon run -- --help
cizon run -- --version
```
