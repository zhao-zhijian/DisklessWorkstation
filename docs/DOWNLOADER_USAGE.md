# Downloader 类使用说明

## 概述

`Downloader` 类是一个用于实现 BitTorrent 下载功能的封装类。它基于 libtorrent 库，提供了简单易用的接口来启动和管理 torrent 下载任务。

## 主要功能

1. **自动下载**：从 torrent 文件启动下载，自动连接 tracker 和 peer
2. **进度监控**：实时显示下载进度、速度、连接数等信息
3. **暂停/恢复**：支持暂停和恢复下载任务
4. **状态查询**：提供丰富的状态查询接口
5. **事件处理**：自动处理下载过程中的各种事件和错误

## 类接口说明

### 构造函数和析构函数

```cpp
Downloader();           // 创建 Downloader 实例，自动初始化 libtorrent session
~Downloader();          // 析构函数，自动停止下载并清理资源
```

### 主要方法

#### `bool start_download(const std::string& torrent_path, const std::string& save_path)`

开始下载。

**参数：**
- `torrent_path`: torrent 文件的路径
- `save_path`: 下载文件的保存目录（如果不存在会自动创建）

**返回值：**
- `true`: 成功启动下载
- `false`: 启动失败（路径不存在、文件无法打开等）

**示例：**
```cpp
Downloader downloader;
if (downloader.start_download("example.torrent", "./downloads"))
{
    std::cout << "下载已启动" << std::endl;
}
```

#### `void stop_download()`

停止下载。

**说明：**
- 从 libtorrent session 中移除 torrent
- 保留已下载的文件
- 自动在析构函数中调用

#### `void pause()` / `void resume()`

暂停/恢复下载。

**示例：**
```cpp
downloader.pause();   // 暂停下载
downloader.resume();   // 恢复下载
```

#### `bool is_downloading() const`

检查是否正在下载。

**返回值：**
- `true`: 正在下载
- `false`: 未在下载

#### `bool is_finished() const`

检查是否已完成下载。

**返回值：**
- `true`: 下载已完成
- `false`: 未完成

#### `bool is_paused() const`

检查是否已暂停。

**返回值：**
- `true`: 已暂停
- `false`: 未暂停

#### `void print_status() const`

打印当前下载状态信息。

**显示内容：**
- 下载状态（下载中/已完成/检查文件中等）
- 下载进度（百分比）
- 已下载/总大小
- 连接的对等节点数
- 上传/下载速度
- Tracker 连接状态

**示例：**
```cpp
downloader.print_status();
```

**输出示例：**
```
=== 下载状态 ===
状态: 下载中 (Downloading)
进度: 45.32%
已下载: 453.20 MB / 1000.00 MB
连接的对等节点数: 8
已上传: 12.50 MB
已下载: 453.20 MB
上传速度: 125.00 KB/s
下载速度: 2.50 MB/s
Tracker 状态:
  - udp://tracker.openbittorrent.com:80/announce [工作正常]
  - http://172.16.1.63:6880/announce [工作正常]
```

#### `bool wait_and_process(int timeout_ms = 1000)`

等待并处理事件。

**参数：**
- `timeout_ms`: 等待时间（毫秒），默认 1000ms

**返回值：**
- `true`: 仍在下载，可以继续循环
- `false`: 应该退出循环

**说明：**
- 处理 libtorrent 的 alerts（如下载完成、错误等）
- 用于主循环中保持下载状态

### 统计信息方法

#### `double get_progress() const`

获取下载进度。

**返回值：**
- 0.0 - 1.0 之间的浮点数，表示下载进度

#### `std::int64_t get_downloaded_bytes() const`

获取已下载的字节数。

#### `std::int64_t get_total_size() const`

获取总文件大小（字节）。

#### `int get_download_rate() const`

获取下载速度（字节/秒）。

#### `int get_upload_rate() const`

获取上传速度（字节/秒）。

#### `int get_peer_count() const`

获取当前连接的 peer 数量。

## 使用示例

### 基本使用

```cpp
#include "downloader.hpp"
#include <iostream>

int main()
{
    // 创建 Downloader 实例
    Downloader downloader;
    
    // 开始下载
    if (downloader.start_download("example.torrent", "./downloads"))
    {
        std::cout << "下载已启动" << std::endl;
        
        // 主循环：保持下载状态
        int status_counter = 0;
        while (downloader.is_downloading() && !downloader.is_finished())
        {
            // 处理事件
            downloader.wait_and_process(1000);
            
            // 每 10 秒显示一次状态
            status_counter++;
            if (status_counter >= 10)
            {
                downloader.print_status();
                status_counter = 0;
            }
        }
        
        if (downloader.is_finished())
        {
            std::cout << "下载完成！" << std::endl;
        }
    }
    else
    {
        std::cerr << "启动下载失败" << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 带暂停/恢复功能

```cpp
#include "downloader.hpp"
#include <iostream>
#include <thread>

int main()
{
    Downloader downloader;
    
    if (downloader.start_download("example.torrent", "./downloads"))
    {
        // 下载 30 秒后暂停
        std::this_thread::sleep_for(std::chrono::seconds(30));
        downloader.pause();
        std::cout << "下载已暂停" << std::endl;
        
        // 暂停 10 秒后恢复
        std::this_thread::sleep_for(std::chrono::seconds(10));
        downloader.resume();
        std::cout << "下载已恢复" << std::endl;
        
        // 继续下载直到完成
        while (downloader.is_downloading() && !downloader.is_finished())
        {
            downloader.wait_and_process(1000);
        }
    }
    
    return 0;
}
```

### 使用统计信息

```cpp
#include "downloader.hpp"
#include <iostream>
#include <iomanip>

int main()
{
    Downloader downloader;
    
    if (downloader.start_download("example.torrent", "./downloads"))
    {
        while (downloader.is_downloading() && !downloader.is_finished())
        {
            downloader.wait_and_process(1000);
            
            // 显示自定义进度信息
            double progress = downloader.get_progress();
            std::int64_t downloaded = downloader.get_downloaded_bytes();
            std::int64_t total = downloader.get_total_size();
            int rate = downloader.get_download_rate();
            
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "进度: " << (progress * 100.0) << "% | ";
            std::cout << "已下载: " << (downloaded / 1024.0 / 1024.0) << " MB / ";
            std::cout << (total / 1024.0 / 1024.0) << " MB | ";
            std::cout << "速度: " << (rate / 1024.0) << " KB/s" << std::endl;
        }
    }
    
    return 0;
}
```

## 重要注意事项

### 1. 保存路径

**save_path 是目录：**
- `save_path` 必须是一个目录路径，不是文件路径
- 如果目录不存在，Downloader 会自动创建
- 下载的文件会保存在该目录下，保持 torrent 中的目录结构

**示例：**
```cpp
// 正确：指定目录
downloader.start_download("example.torrent", "./downloads");

// 错误：指定文件路径
downloader.start_download("example.torrent", "./downloads/file.txt");  // 错误
```

### 2. 网络配置

Downloader 类自动配置了以下网络功能：
- **DHT（分布式哈希表）**：即使 tracker 不可用，也能通过 DHT 找到对等节点
- **UPnP/NAT-PMP**：自动配置路由器端口转发（如果支持）
- **本地服务发现（LSD）**：在本地网络中自动发现其他客户端

### 3. 下载速度

- 默认情况下，下载速度无限制
- 可以通过修改 `configure_session()` 中的设置来限制速度
- 实际下载速度取决于：
  - 可用的 peer 数量和上传速度
  - 网络带宽
  - Tracker 和 DHT 的连接情况

### 4. 文件完整性

- libtorrent 会自动验证下载的文件完整性
- 如果文件损坏，会自动重新下载损坏的部分
- 下载完成后会进行完整性检查

### 5. 资源管理

- Downloader 使用 RAII 模式，析构时自动清理资源
- 建议使用智能指针或栈对象，避免手动管理内存
- 停止下载时，已下载的文件会保留在保存目录中

## 常见问题

### Q: 为什么下载启动失败？

**可能原因：**
1. torrent 文件路径不正确或文件损坏
2. save_path 路径无效或无法创建目录
3. 文件权限问题
4. 磁盘空间不足

**解决方法：**
- 检查 torrent 文件是否存在且有效
- 检查保存路径的权限
- 确保有足够的磁盘空间

### Q: 为什么下载速度很慢？

**可能原因：**
1. 可用的 peer 数量少
2. peer 的上传速度慢
3. 网络带宽限制
4. Tracker 连接问题

**解决方法：**
- 等待更多 peer 连接
- 检查网络连接
- 尝试使用其他 tracker
- 检查防火墙设置

### Q: 如何暂停和恢复下载？

**方法：**
```cpp
downloader.pause();   // 暂停
downloader.resume();   // 恢复
```

**说明：**
- 暂停后，已下载的数据会保留
- 恢复后会继续从未完成的部分下载
- 可以随时暂停和恢复

### Q: 下载完成后文件在哪里？

**位置：**
- 文件保存在 `start_download()` 时指定的 `save_path` 目录下
- 保持 torrent 中定义的目录结构
- 例如：如果 torrent 包含 `folder/file.txt`，保存路径是 `./downloads`，则文件会在 `./downloads/folder/file.txt`

### Q: 如何检查下载是否完成？

**方法：**
```cpp
if (downloader.is_finished())
{
    std::cout << "下载完成！" << std::endl;
}
```

或者在主循环中：
```cpp
while (downloader.is_downloading() && !downloader.is_finished())
{
    // 继续下载
}
```

## 技术细节

### 内部实现

Downloader 类内部使用：
- `lt::session`：libtorrent 会话，管理所有 torrent 任务
- `lt::torrent_handle`：单个 torrent 任务的句柄
- `lt::add_torrent_params`：添加 torrent 时的参数

### Session 配置

默认配置包括：
- 启用 DHT、UPnP、NAT-PMP、LSD
- 自动选择监听端口
- 无下载/上传速度限制
- 最大连接数：200
- 启用状态、错误和存储通知

### 下载模式

- 使用自动管理模式（`auto_managed`）
- 自动进行文件完整性检查
- 支持断点续传（如果支持的话）

## 相关文档

- [Seeder 类使用说明](SEEDER_USAGE.md)：了解如何做种
- [Tracker 说明文档](TRACKER_EXPLANATION.md)：了解 tracker 的工作原理
- [LibTorrent 官方文档](https://libtorrent.org/)：libtorrent 库的详细文档
