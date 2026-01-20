# TorrentManager 类使用说明

## 概述

`TorrentManager` 类是一个统一的 Torrent 管理器，它合并了 `Downloader` 和 `Seeder` 类的功能，使用单例模式，通过一个共享的 libtorrent session 来管理所有的下载和做种任务。该类支持并发下载、并发做种，以及获取每个种子的详细状态。

## 主要特性

1. **单例模式**：全局唯一实例，确保所有操作共享同一个 session
2. **统一管理**：同时管理下载和做种任务
3. **并发支持**：支持多个下载任务和多个做种任务同时运行
4. **状态查询**：提供详细的状态查询接口，区分下载状态和做种状态
5. **线程安全**：使用互斥锁保护，支持多线程环境

## 类接口说明

### 获取单例实例

```cpp
TorrentManager& manager = TorrentManager::getInstance();
```

### 主要方法

#### `std::string start_download(const std::string& torrent_path, const std::string& save_path)`

开始下载任务。

**参数：**
- `torrent_path`: torrent 文件路径
- `save_path`: 下载文件的保存目录

**返回值：**
- 成功：返回 info_hash（用于后续操作）
- 失败：返回空字符串

**示例：**
```cpp
TorrentManager& manager = TorrentManager::getInstance();
std::string info_hash = manager.start_download("example.torrent", "./downloads");
if (!info_hash.empty()) {
    std::cout << "下载已启动，info_hash: " << info_hash << std::endl;
}
```

#### `std::string start_seeding(const std::string& torrent_path, const std::string& save_path)`

开始做种任务。

**参数：**
- `torrent_path`: torrent 文件路径
- `save_path`: 原始文件/目录的保存路径（必须与创建 torrent 时的路径一致）

**返回值：**
- 成功：返回 info_hash（用于后续操作）
- 失败：返回空字符串

**示例：**
```cpp
TorrentManager& manager = TorrentManager::getInstance();
std::string info_hash = manager.start_seeding("example.torrent", "C:\\MyFiles");
if (!info_hash.empty()) {
    std::cout << "做种已启动，info_hash: " << info_hash << std::endl;
}
```

#### `bool stop_torrent(const std::string& info_hash)`

停止指定的 torrent。

**参数：**
- `info_hash`: torrent 的 info_hash（由 start_download 或 start_seeding 返回）

**返回值：**
- `true`: 成功停止
- `false`: 失败（未找到指定的 torrent）

**示例：**
```cpp
manager.stop_torrent(info_hash);
```

#### `void stop_all()`

停止所有 torrent（包括下载和做种）。

#### `void stop_all_downloads()`

停止所有下载任务。

#### `void stop_all_seedings()`

停止所有做种任务。

#### `bool pause_torrent(const std::string& info_hash)`

暂停指定的 torrent。

#### `bool resume_torrent(const std::string& info_hash)`

恢复指定的 torrent。

#### `void pause_all()`

暂停所有 torrent。

#### `void resume_all()`

恢复所有 torrent。

### 状态查询方法

#### `TorrentStatus get_torrent_status(const std::string& info_hash) const`

获取指定 torrent 的状态。

**返回值：** `TorrentStatus` 结构体，包含以下信息：
- `info_hash`: info hash
- `type`: 类型（`TorrentType::Download` 或 `TorrentType::Seeding`）
- `torrent_path`: torrent 文件路径
- `save_path`: 保存路径
- `state`: 状态（`lt::torrent_status::state_t`）
- `progress`: 进度（0.0 - 1.0）
- `total_size`: 总大小（字节）
- `downloaded_bytes`: 已下载（字节）
- `uploaded_bytes`: 已上传（字节）
- `download_rate`: 下载速度（字节/秒）
- `upload_rate`: 上传速度（字节/秒）
- `peer_count`: 连接的 peer 数量
- `is_paused`: 是否暂停
- `is_finished`: 是否完成

**示例：**
```cpp
TorrentStatus status = manager.get_torrent_status(info_hash);
if (status.is_valid) {
    std::cout << "进度: " << (status.progress * 100.0) << "%" << std::endl;
    std::cout << "下载速度: " << status.download_rate << " B/s" << std::endl;
}
```

#### `std::vector<TorrentStatus> get_all_torrent_status() const`

获取所有 torrent 的状态。

#### `std::vector<TorrentStatus> get_download_status() const`

获取所有下载任务的状态。

#### `std::vector<TorrentStatus> get_seeding_status() const`

获取所有做种任务的状态。

### 其他方法

#### `bool has_torrent(const std::string& info_hash) const`

检查指定 torrent 是否存在。

#### `size_t get_torrent_count() const`

获取 torrent 总数。

#### `size_t get_download_count() const`

获取下载任务数量。

#### `size_t get_seeding_count() const`

获取做种任务数量。

#### `bool wait_and_process(int timeout_ms = 1000)`

等待并处理事件（用于保持运行状态）。

**参数：**
- `timeout_ms`: 等待超时时间（毫秒）

**返回值：**
- `true`: 继续运行
- `false`: 应该退出

#### `void print_all_status() const`

打印所有 torrent 的状态。

#### `void print_torrent_status(const std::string& info_hash) const`

打印指定 torrent 的状态。

## 完整使用示例

```cpp
#include "torrent_manager.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    // 获取单例实例
    TorrentManager& manager = TorrentManager::getInstance();
    
    // 启动下载任务
    std::string download_hash = manager.start_download("file1.torrent", "./downloads");
    
    // 启动做种任务
    std::string seeding_hash = manager.start_seeding("file2.torrent", "C:\\MyFiles");
    
    // 主循环：处理事件并显示状态
    while (true) {
        // 处理事件
        if (!manager.wait_and_process(1000)) {
            break;
        }
        
        // 每5秒打印一次状态
        static int counter = 0;
        if (++counter >= 5) {
            counter = 0;
            manager.print_all_status();
        }
        
        // 检查下载是否完成
        if (!download_hash.empty()) {
            TorrentStatus status = manager.get_torrent_status(download_hash);
            if (status.is_finished) {
                std::cout << "下载完成！" << std::endl;
                download_hash.clear(); // 不再检查
            }
        }
    }
    
    // 停止所有任务
    manager.stop_all();
    
    return 0;
}
```

## 并发使用示例

```cpp
#include "torrent_manager.hpp"
#include <vector>
#include <thread>

int main()
{
    TorrentManager& manager = TorrentManager::getInstance();
    
    // 启动多个下载任务
    std::vector<std::string> download_hashes;
    download_hashes.push_back(manager.start_download("file1.torrent", "./downloads"));
    download_hashes.push_back(manager.start_download("file2.torrent", "./downloads"));
    download_hashes.push_back(manager.start_download("file3.torrent", "./downloads"));
    
    // 启动多个做种任务
    std::vector<std::string> seeding_hashes;
    seeding_hashes.push_back(manager.start_seeding("seed1.torrent", "C:\\Seeds"));
    seeding_hashes.push_back(manager.start_seeding("seed2.torrent", "C:\\Seeds"));
    
    // 主循环
    while (manager.get_torrent_count() > 0) {
        manager.wait_and_process(1000);
        
        // 显示统计信息
        std::cout << "总任务数: " << manager.get_torrent_count() 
                  << " (下载: " << manager.get_download_count()
                  << ", 做种: " << manager.get_seeding_count() << ")" << std::endl;
    }
    
    return 0;
}
```

## 注意事项

1. **单例模式**：`TorrentManager` 使用单例模式，所有操作都通过 `getInstance()` 获取实例
2. **info_hash**：每个 torrent 都有一个唯一的 info_hash，用于标识和操作该 torrent
3. **线程安全**：所有公共方法都是线程安全的，可以在多线程环境中使用
4. **资源管理**：析构函数会自动停止所有任务并清理资源
5. **路径验证**：
   - 下载时：如果保存路径不存在，会自动创建
   - 做种时：保存路径必须存在，且必须指向创建 torrent 时的原始文件或目录
6. **大文件优化**：对于大于 50GB 的文件，会自动应用优化配置

## 与 Downloader/Seeder 的区别

1. **统一管理**：`TorrentManager` 使用一个共享的 session，而 `Downloader` 和 `Seeder` 各自创建独立的 session
2. **并发支持**：`TorrentManager` 原生支持多个下载和做种任务，而 `Downloader` 只支持单个下载任务
3. **状态查询**：`TorrentManager` 提供了更详细的状态查询接口，可以区分下载和做种状态
4. **单例模式**：`TorrentManager` 使用单例模式，确保全局唯一实例
