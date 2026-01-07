# BitTorrent Tracker 说明

## Tracker 的作用

Tracker 是 BitTorrent 协议中的协调服务器，它的主要作用是：
1. **记录做种者（Seeder）和下载者（Leecher）的信息**
2. **帮助客户端发现其他对等节点（Peers）**
3. **协调文件传输**

## 创建 Torrent 时添加 Tracker URL

### 1. 在创建种子时添加 Tracker URL

当你在创建 torrent 文件时添加 tracker URL（例如：`udp://tracker.openbittorrent.com:80/announce`），这个 URL 会被**写入到 torrent 文件中**。

**作用：**
- 告诉 BitTorrent 客户端（如 qBittorrent、uTorrent 等）应该向哪些 tracker 服务器报告
- 客户端在开始做种或下载时会自动向这些 tracker 发送 announce 请求

### 2. Tracker 的工作流程

```
1. 创建 Torrent 文件
   └─> 添加 Tracker URL 到 torrent 文件
   
2. 使用客户端打开 Torrent 文件并开始做种
   └─> 客户端向 Tracker 发送 announce 请求
       ├─> Tracker 记录这个 torrent 的 info_hash
       ├─> Tracker 记录做种者的 IP 和端口
       └─> Tracker 返回其他做种者和下载者的列表
   
3. 其他用户下载 Torrent 文件
   └─> 客户端向 Tracker 发送 announce 请求
       ├─> Tracker 返回做种者列表
       └─> 客户端连接到做种者开始下载
```

## 如何"上传"到 Tracker

### 方式一：自动注册（推荐）

大多数公共 tracker 支持**自动注册**：

1. **创建 torrent 文件时添加 tracker URL**
2. **使用 BitTorrent 客户端打开 torrent 文件并开始做种**
3. **客户端会自动向 tracker 发送 announce 请求**
4. **Tracker 会自动记录这个 torrent**

**优点：**
- 简单方便，无需手动操作
- 大多数公共 tracker 都支持

**示例 tracker（公共 tracker）：**
```
udp://tracker.openbittorrent.com:80/announce
udp://tracker.publicbt.com:80/announce
udp://tracker.istole.it:80/announce
http://tracker.bt-chat.com/announce
```

### 方式二：手动上传（私有 tracker）

某些**私有 tracker**（如 PT 站点）需要手动上传：

1. **创建 torrent 文件**
2. **登录 tracker 网站**
3. **通过网站的上传页面手动上传 torrent 文件**
4. **填写相关信息（标题、描述、分类等）**
5. **Tracker 验证后才会接受 announce 请求**

**特点：**
- 需要注册账号
- 通常有上传/下载比例要求
- 需要遵守站点规则

## 当前代码中的实现

### TorrentBuilder 类

在 `TorrentBuilder` 类中添加 tracker URL：

```cpp
// 添加 tracker URL 到 torrent 文件
std::vector<std::string> trackers = {
    "udp://tracker.openbittorrent.com:80/announce",
};
creator.set_trackers(trackers);
```

**这会将 tracker URL 写入 torrent 文件，但不会立即"上传"到 tracker。**

### Seeder 类

使用 `Seeder` 类可以自动开始做种并向 tracker 报告：

```cpp
Seeder seeder;
seeder.start_seeding("example.torrent", "C:\\MyFiles");
// 自动向 tracker 发送 announce 请求
```

**详细说明请参考：** [Seeder 类使用说明](SEEDER_USAGE.md)

## 完整的做种流程

### 方式一：使用程序内置的 Seeder 类（推荐）

1. **使用本程序创建 torrent 文件**
   ```bash
   DisklessWorkstation.exe C:\MyFiles\example.txt example.torrent
   ```

2. **程序会询问是否开始做种**
   - 输入 `y` 确认开始做种
   - 程序会自动使用 `Seeder` 类启动做种

3. **Seeder 自动向 tracker 报告**
   - 自动向所有配置的 tracker 发送 announce 请求
   - Tracker 会记录你的做种信息
   - 实时显示做种状态和 tracker 连接状态

4. **其他用户可以通过 tracker 找到你的做种**

**详细说明请参考：** [Seeder 类使用说明](SEEDER_USAGE.md)

### 方式二：使用外部 BitTorrent 客户端

1. **使用本程序创建 torrent 文件**
   ```bash
   DisklessWorkstation.exe C:\MyFiles\example.txt example.torrent
   ```

2. **使用 BitTorrent 客户端打开 torrent 文件**
   - 确保文件路径正确（指向原始文件）
   - 开始做种（Seeding）

3. **客户端自动向 tracker 报告**
   - 客户端会定期向 tracker 发送 announce 请求
   - Tracker 会记录你的做种信息

4. **其他用户可以通过 tracker 找到你的做种**

## 注意事项

1. **Tracker URL 格式：**
   - HTTP: `http://tracker.example.com:80/announce`
   - UDP: `udp://tracker.example.com:80/announce`
   - HTTPS: `https://tracker.example.com:443/announce`

2. **多个 Tracker：**
   - 可以添加多个 tracker URL
   - 客户端会尝试所有 tracker，使用第一个响应的

3. **Tracker 状态：**
   - 公共 tracker 可能不稳定或已关闭
   - 建议使用多个 tracker 提高可用性

4. **DHT（分布式哈希表）：**
   - 现代 BitTorrent 客户端支持 DHT
   - 即使 tracker 不可用，DHT 也能帮助找到对等节点

## 验证 Tracker 是否工作

1. **使用 BitTorrent 客户端打开 torrent 文件**
2. **查看 tracker 状态**
   - 应该显示 "Working" 或 "OK"
   - 如果显示 "Not contacted yet"，等待几秒钟
   - 如果显示 "Error" 或 "Offline"，该 tracker 可能不可用

3. **查看做种者数量**
   - 如果 tracker 正常工作，应该能看到做种者数量

