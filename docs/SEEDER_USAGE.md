# Seeder 类使用说明

## 概述

`Seeder` 类是一个用于实现 BitTorrent 做种功能的封装类。它基于 libtorrent 库，提供了简单易用的接口来启动和管理 torrent 做种任务。

## 主要功能

1. **自动做种**：从 torrent 文件启动做种，自动向 tracker 报告
2. **多torrent支持**：支持在同一个session中同时管理多个torrent的做种任务
3. **状态监控**：实时显示做种状态、连接数、上传/下载速度等信息
4. **Tracker 管理**：自动处理与 tracker 的通信，显示 tracker 连接状态
5. **事件处理**：处理做种过程中的各种事件和错误
6. **大文件优化**：针对大文件（>50GB）自动应用优化配置，包括快速模式启动、更大的磁盘缓存等

## 类接口说明

### 构造函数和析构函数

```cpp
Seeder();           // 创建 Seeder 实例，自动初始化 libtorrent session
~Seeder();          // 析构函数，自动停止做种并清理资源
```

### 主要方法

#### `bool start_seeding(const std::string& torrent_path, const std::string& save_path)`

开始做种。

**参数：**
- `torrent_path`: torrent 文件的路径
- `save_path`: 原始文件/目录的保存路径（必须与创建 torrent 时的路径一致）

**返回值：**
- `true`: 成功启动做种
- `false`: 启动失败（路径不存在、文件无法打开等）

**示例：**
```cpp
Seeder seeder;
if (seeder.start_seeding("example.torrent", "C:\\MyFiles"))
{
    std::cout << "做种已启动" << std::endl;
}
```

#### `void stop_seeding()`

停止做种。

**说明：**
- 从 libtorrent session 中移除 torrent
- 清理相关资源
- 自动在析构函数中调用

**示例：**
```cpp
seeder.stop_seeding();
```

#### `bool is_seeding() const`

检查是否正在做种。

**返回值：**
- `true`: 正在做种
- `false`: 未在做种

**示例：**
```cpp
if (seeder.is_seeding())
{
    std::cout << "正在做种中..." << std::endl;
}
```

#### `void print_status() const`

打印当前做种状态信息。

**显示内容：**
- 做种状态（做种中/已完成/下载中等）
- 连接的对等节点数
- 已上传/下载的字节数
- 上传/下载速度
- Tracker 连接状态

**示例：**
```cpp
seeder.print_status();
```

**输出示例（单个torrent）：**
```
=== 当前做种任务数: 1 ===

--- Torrent #1 状态 ---
状态: 做种中 (Seeding)
进度: 100.00%
已下载/需要: 1.00 GB / 1.00 GB
连接的对等节点数: 5
已上传: 125.50 MB
已下载: 0.00 B
上传速度: 1.25 MB/s
下载速度: 0.00 B/s
Tracker 状态:
  - udp://tracker.openbittorrent.com:80/announce [工作正常]
  - http://172.16.1.63:6880/announce [工作正常]
```

**输出示例（多个torrent）：**
```
=== 当前做种任务数: 3 ===

--- Torrent #1 状态 ---
状态: 做种中 (Seeding)
进度: 100.00%
已下载/需要: 1.00 GB / 1.00 GB
连接的对等节点数: 3
已上传: 125.50 MB
已下载: 0.00 B
上传速度: 1.25 MB/s
下载速度: 0.00 B/s
Tracker 状态:
  - http://172.16.1.63:6880/announce [工作正常]

--- Torrent #2 状态 ---
状态: 做种中 (Seeding)
进度: 100.00%
已下载/需要: 500.00 MB / 500.00 MB
连接的对等节点数: 2
已上传: 50.25 MB
已下载: 0.00 B
上传速度: 512.00 KB/s
下载速度: 0.00 B/s
Tracker 状态:
  - http://172.16.1.63:6880/announce [工作正常]

--- Torrent #3 状态 ---
状态: 检查文件中 (Checking Files)
进度: 45.32%
已下载/需要: 226.60 MB / 500.00 MB
连接的对等节点数: 0
已上传: 0.00 B
已下载: 0.00 B
上传速度: 0.00 B/s
下载速度: 0.00 B/s
Tracker 状态:
  - http://172.16.1.63:6880/announce [未连接]
```

#### `bool wait_and_process(int timeout_ms = 1000)`

等待并处理事件。

**参数：**
- `timeout_ms`: 等待时间（毫秒），默认 1000ms

**返回值：**
- `true`: 仍在做种，可以继续循环
- `false`: 应该退出循环

**说明：**
- 处理 libtorrent 的 alerts（如 tracker 公告、错误等）
- 用于主循环中保持做种状态

**示例：**
```cpp
while (seeder.is_seeding())
{
    seeder.wait_and_process(1000);  // 等待 1 秒并处理事件
}
```

#### `int get_peer_count() const`

获取当前连接的 peer 数量。

**返回值：**
- 连接的 peer 数量，如果未在做种则返回 0

#### `std::int64_t get_uploaded_bytes() const`

获取已上传的字节数。

**返回值：**
- 已上传的字节数，如果未在做种则返回 0

#### `std::int64_t get_downloaded_bytes() const`

获取已下载的字节数（做种时通常为 0）。

**返回值：**
- 已下载的字节数，如果未在做种则返回 0

#### `size_t get_torrent_count() const`

获取当前管理的 torrent 数量。

**返回值：**
- 当前正在做种的 torrent 数量

**说明：**
- 支持同时管理多个torrent
- 每次调用 `start_seeding()` 都会添加一个新的torrent到管理列表
- 所有torrent共享同一个libtorrent session

## 使用示例

### 基本使用

```cpp
#include "seeder.hpp"
#include <iostream>

int main()
{
    // 创建 Seeder 实例
    Seeder seeder;
    
    // 开始做种
    // torrent_path: torrent 文件路径
    // save_path: 原始文件/目录的父目录路径
    if (seeder.start_seeding("example.torrent", "C:\\MyFiles"))
    {
        std::cout << "做种已启动" << std::endl;
        
        // 主循环：保持做种状态
        int status_counter = 0;
        while (seeder.is_seeding())
        {
            // 处理事件
            seeder.wait_and_process(1000);
            
            // 每 10 秒显示一次状态
            status_counter++;
            if (status_counter >= 10)
            {
                seeder.print_status();
                status_counter = 0;
            }
        }
        
        std::cout << "做种已停止" << std::endl;
    }
    else
    {
        std::cerr << "启动做种失败" << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 多Torrent同时做种

```cpp
#include "seeder.hpp"
#include <iostream>

int main()
{
    Seeder seeder;
    
    // 添加第一个torrent
    if (seeder.start_seeding("torrent1.torrent", "C:\\Files1"))
    {
        std::cout << "Torrent 1 已添加" << std::endl;
    }
    
    // 添加第二个torrent（共享同一个session）
    if (seeder.start_seeding("torrent2.torrent", "C:\\Files2"))
    {
        std::cout << "Torrent 2 已添加" << std::endl;
    }
    
    // 添加第三个torrent
    if (seeder.start_seeding("torrent3.torrent", "C:\\Files3"))
    {
        std::cout << "Torrent 3 已添加" << std::endl;
    }
    
    std::cout << "当前做种数量: " << seeder.get_torrent_count() << std::endl;
    
    // 主循环：保持做种状态
    int status_counter = 0;
    while (seeder.is_seeding())
    {
        seeder.wait_and_process(1000);
        
        // 每 10 秒显示一次状态
        status_counter++;
        if (status_counter >= 10)
        {
            std::cout << std::endl;
            std::cout << "=== 当前状态（每10秒更新） ===" << std::endl;
            seeder.print_status();
            std::cout << "总Peer数: " << seeder.get_peer_count() << std::endl;
            std::cout << "总上传: " << format_bytes(seeder.get_uploaded_bytes()) << std::endl;
            std::cout << "总下载: " << format_bytes(seeder.get_downloaded_bytes()) << std::endl;
            std::cout << std::endl;
            status_counter = 0;
        }
    }
    
    return 0;
}
```

**特点：**
- 所有torrent共享同一个libtorrent session，资源占用更高效
- 可以随时添加新的torrent（调用 `start_seeding()`）
- `print_status()` 会显示所有torrent的详细状态
- `get_peer_count()`、`get_uploaded_bytes()` 等方法返回所有torrent的累计值

### 与 TorrentBuilder 配合使用

```cpp
#include "torrent_builder.hpp"
#include "seeder.hpp"
#include <iostream>

int main()
{
    // 1. 创建 torrent 文件
    TorrentBuilder builder;
    builder.set_trackers({
        "udp://tracker.openbittorrent.com:80/announce",
        "http://172.16.1.63:6880/announce"
    });
    
    std::string file_path = "C:\\MyFiles\\example.txt";
    std::string torrent_path = "example.torrent";
    
    if (builder.create_torrent(file_path, torrent_path))
    {
        std::cout << "Torrent 文件创建成功" << std::endl;
        
        // 2. 开始做种
        Seeder seeder;
        
        // 注意：save_path 必须是原始文件的父目录
        std::filesystem::path p(file_path);
        std::string save_path = p.parent_path().string();
        if (save_path.empty())
        {
            save_path = ".";
        }
        
        if (seeder.start_seeding(torrent_path, save_path))
        {
            std::cout << "开始做种..." << std::endl;
            
            // 保持做种状态
            while (seeder.is_seeding())
            {
                seeder.wait_and_process(1000);
            }
        }
    }
    
    return 0;
}
```

## 重要注意事项

### 1. 路径要求

**save_path 必须正确：**
- `save_path` 必须指向创建 torrent 时使用的**父目录**
- 如果创建 torrent 时文件路径是 `C:\MyFiles\example.txt`，则 `save_path` 应该是 `C:\MyFiles`
- 如果创建 torrent 时目录路径是 `C:\MyFiles\Folder`，则 `save_path` 应该是 `C:\MyFiles`

**示例：**
```cpp
// 创建 torrent 时
TorrentBuilder builder;
builder.create_torrent("C:\\MyFiles\\example.txt", "example.torrent");
// 此时 root_path 是 "C:\\MyFiles"

// 做种时
Seeder seeder;
seeder.start_seeding("example.torrent", "C:\\MyFiles");  // 必须匹配
```

### 2. 文件完整性

- 确保原始文件/目录完整且未被修改
- 如果文件被修改或删除，做种可能会失败

### 3. 网络配置

Seeder 类自动配置了以下网络功能：
- **DHT（分布式哈希表）**：即使 tracker 不可用，也能通过 DHT 找到对等节点
- **UPnP/NAT-PMP**：自动配置路由器端口转发（如果支持）
- **本地服务发现（LSD）**：在本地网络中自动发现其他客户端

### 4. Tracker 连接

- Seeder 会自动向 torrent 文件中配置的所有 tracker 发送 announce 请求
- Tracker 连接状态会在 `print_status()` 中显示
- 如果 tracker 不可用，程序仍会继续运行，等待 tracker 恢复

### 5. 资源管理

- Seeder 使用 RAII 模式，析构时自动清理资源
- 建议使用智能指针或栈对象，避免手动管理内存

### 6. 多Torrent管理

Seeder 类支持在同一个session中同时管理多个torrent：

**特点：**
- 所有torrent共享同一个libtorrent session，资源占用更高效
- 可以随时添加新的torrent（多次调用 `start_seeding()`）
- `print_status()` 会显示所有torrent的详细状态
- `get_peer_count()`、`get_uploaded_bytes()` 等方法返回所有torrent的累计值
- `get_torrent_count()` 返回当前管理的torrent数量

**使用场景：**
- 需要同时做多个torrent时
- 需要统一管理多个做种任务时
- 需要节省资源（共享session）时

### 7. 大文件做种优化

Seeder 类针对大文件（>50GB）自动应用以下优化：

**自动检测和优化：**
- 自动检测文件大小，对于超过 50GB 的文件自动应用优化配置
- 如果文件已存在，使用 `seed_mode` 快速模式，跳过文件验证以快速启动做种
- 如果文件不存在，会进行文件验证（可能需要较长时间）

**性能优化配置：**
- **磁盘缓存**：设置为 256MB，提高大文件做种性能
- **连接数**：最大连接数设置为 200
- **快速启动**：对于已存在的大文件，跳过验证直接开始做种

**使用示例：**
```cpp
Seeder seeder;

// 大文件会自动应用优化
if (seeder.start_seeding("large_file.torrent", "C:\\MyFiles"))
{
    // 大文件做种会自动使用优化配置
    // 如果文件已存在，会快速启动（跳过验证）
    // 如果文件不存在，会进行验证（可能需要时间）
    while (seeder.is_seeding())
    {
        seeder.wait_and_process(1000);
    }
}
```

**注意事项：**
- 大文件做种时，如果文件已存在，会快速启动（跳过验证）
- 如果文件不存在或需要验证，可能需要较长时间
- 确保文件路径正确，与创建 torrent 时的路径一致

## 常见问题

### Q: 为什么做种启动失败？

**可能原因：**
1. torrent 文件路径不正确
2. save_path 路径不正确（必须与创建 torrent 时的父目录一致）
3. 原始文件/目录不存在或被移动
4. 文件权限问题

**解决方法：**
- 检查路径是否正确
- 确保原始文件/目录存在且可访问
- 查看错误消息获取详细信息

### Q: 为什么 tracker 显示"未连接"？

**可能原因：**
1. Tracker 服务器不可用或已关闭
2. 网络连接问题
3. 防火墙阻止了连接
4. Tracker URL 格式错误

**解决方法：**
- 检查网络连接
- 尝试使用其他 tracker
- 检查防火墙设置
- 等待一段时间，tracker 可能需要时间响应

### Q: 如何停止做种？

**方法：**
1. 调用 `stop_seeding()` 方法
2. 让 Seeder 对象离开作用域（自动调用析构函数）
3. 在主循环中检查条件并退出循环

### Q: 做种时占用多少资源？

**资源占用：**
- CPU：通常很低（< 5%），主要在传输数据时增加
- 内存：取决于 torrent 大小和连接数，通常几十到几百 MB
- 网络：取决于上传速度设置和连接数

## 技术细节

### 内部实现

Seeder 类内部使用：
- `lt::session`：libtorrent 会话，管理所有 torrent 任务（所有torrent共享同一个session）
- `std::vector<lt::torrent_handle>`：管理所有torrent任务的句柄列表
- `lt::add_torrent_params`：添加 torrent 时的参数

**多torrent管理：**
- 使用 `std::vector<lt::torrent_handle>` 存储所有torrent句柄
- 每次调用 `start_seeding()` 都会添加一个新的torrent句柄到列表
- `is_seeding()` 检查所有torrent句柄，只要有一个有效即返回true
- `wait_and_process()` 会自动清理无效的torrent句柄

### Session 配置

默认配置包括：
- 启用 DHT、UPnP、NAT-PMP、LSD
- 自动选择监听端口
- 启用状态和错误通知

### 做种模式

使用 `seed_mode` 标志：
- 跳过文件完整性检查（假设文件已完整）
- 直接进入做种状态
- 不下载，只上传

## 相关文档

- [Tracker 说明文档](TRACKER_EXPLANATION.md)：了解 tracker 的工作原理
- [LibTorrent 官方文档](https://libtorrent.org/)：libtorrent 库的详细文档

