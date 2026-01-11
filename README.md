# DisklessWorkstation

无盘工作站项目，使用 Conan 进行依赖管理。

## 项目简介

本项目使用 Conan 2.x 管理 C++ 依赖，主要依赖包括：
- **boost/1.81.0** - C++ 库集合
- **libtorrent/2.0.10** - BitTorrent 库

## 环境要求

- **Conan**: 2.x 版本
- **编译器**: MSVC (Visual Studio 2022)
- **CMake**: 3.16 或更高版本（由 Conan 自动管理）

## 快速开始

### 1. 安装依赖

使用 MSVC VS2022 编译器配置安装依赖：

```bash
conan install . -pr=conan_profiles/msvc-vs2022 --build=missing -of build
```

此命令会：
- 下载并安装 boost/1.81.0 和 libtorrent/2.0.10
- 将所有生成的文件（CMake 配置文件等）放在 `build` 文件夹中
- 如果本地没有预编译包，会自动从源码构建

### 2. 使用 CMake 构建项目

**重要：** 使用 CMake Presets 之前，必须先完成步骤 1 安装依赖，确保 `build/conan_toolchain.cmake` 文件存在。

#### 方式一：使用自动化脚本（最简单）

Windows 下可以使用提供的批处理脚本一键完成所有步骤：

```bash
# 完整构建流程（包括依赖安装）
build.bat

# 如果依赖已安装，可以跳过安装步骤
build.bat --skip-install
```

脚本会自动执行：
1. 检查构建工具（Conan、CMake）
2. 安装 Conan 依赖
3. 配置 CMake 项目
4. 构建项目（Release 配置）

#### 方式二：使用 CMake Presets（推荐）

CMake Presets 由 Conan 自动生成，位于 `build/CMakePresets.json`。

```bash
# 配置项目
cmake --preset conan-default

# 构建项目（Release 配置）
cmake --build --preset conan-release
```

#### 方式三：传统 CMake 命令

```bash
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . --config Release
```

## 项目结构

```
DisklessWorkstation/
├── build.bat                 # Windows 自动化构建脚本
├── conanfile.py              # Conan 配置文件
├── conan_profiles/           # Conan 编译器配置
│   └── msvc-vs2022          # MSVC VS2022 配置
├── src/                      # 源代码目录
│   ├── main.cpp             # 主程序
│   ├── torrent_builder.hpp  # TorrentBuilder 类头文件
│   ├── torrent_builder.cpp  # TorrentBuilder 类实现
│   ├── seeder.hpp           # Seeder 类头文件
│   ├── seeder.cpp           # Seeder 类实现
│   ├── downloader.hpp       # Downloader 类头文件
│   └── downloader.cpp       # Downloader 类实现
├── docs/                     # 文档目录
│   ├── TRACKER_EXPLANATION.md  # Tracker 说明文档
│   ├── SEEDER_USAGE.md         # Seeder 类使用说明
│   └── DOWNLOADER_USAGE.md     # Downloader 类使用说明
├── build/                    # 构建输出目录（由 Conan 生成）
└── README.md                 # 本文件
```

## 功能特性

### TorrentBuilder 类
- 创建 BitTorrent torrent 文件
- 支持添加多个 tracker
- 支持设置注释和创建者信息
- 自动计算文件哈希值
- **大文件优化**：自动为大文件（>4GB）设置合适的分片大小（16MB）

### Seeder 类
- 自动开始做种
- 自动向 tracker 报告
- 实时显示做种状态
- 支持 DHT、UPnP、NAT-PMP 等网络功能
- **大文件优化**：对于大文件（>50GB）使用快速模式，跳过文件验证以快速启动做种

### Downloader 类
- 从 torrent 文件自动下载
- 实时显示下载进度和速度
- 支持暂停/恢复下载
- 自动处理下载完成事件
- 支持 DHT、UPnP、NAT-PMP 等网络功能
- **大文件下载优化**：针对大文件（>50GB）自动优化配置，包括更大的磁盘缓存、更多连接数等

## 使用说明

### 基本用法

项目支持三种运行模式：

#### 1. 生成 Torrent 文件模式（默认）

```bash
# 创建 torrent 文件并开始做种
DisklessWorkstation.exe C:\MyFiles\example.txt example.torrent
```

程序会：
1. 创建 torrent 文件
2. 询问是否开始做种
3. 如果选择是，自动启动做种并向 tracker 报告

#### 2. 直接做种模式

```bash
# 使用已有的 torrent 文件开始做种
DisklessWorkstation.exe -s example.torrent C:\MyFiles
```

**参数说明：**
- `-s, --seed`: 直接做种模式（跳过生成 torrent 文件）
- `torrent文件路径`: 已有的 .torrent 文件路径
- `保存路径`: 原始文件/目录的保存路径（必须与创建 torrent 时的路径一致）

#### 3. 下载模式

```bash
# 从 torrent 文件下载
DisklessWorkstation.exe -d example.torrent C:\Downloads
```

**参数说明：**
- `-d, --download`: 下载模式
- `torrent文件路径`: 要下载的 .torrent 文件路径
- `保存路径`: 下载文件的保存目录（如果不存在会自动创建）

**大文件下载优化：**
- 自动检测大文件（>50GB）并应用优化配置
- 使用更大的磁盘缓存（512MB）提高性能
- 设置更多连接数以加快下载速度
- 自动监控并恢复被暂停的下载

### 使用示例

```bash
# 生成 torrent 文件
DisklessWorkstation.exe C:\MyFiles\example.txt example.torrent

# 直接做种（使用已有 torrent 文件）
DisklessWorkstation.exe -s example.torrent C:\MyFiles

# 下载文件
DisklessWorkstation.exe -d example.torrent C:\Downloads

# 大文件目录生成 torrent（自动优化）
DisklessWorkstation.exe D:\LargeDirectory\ D:\torrents\large.torrent

# 大文件目录下载（自动优化）
DisklessWorkstation.exe -d D:\torrents\large.torrent D:\Downloads
```

### 详细文档

- [Tracker 说明文档](docs/TRACKER_EXPLANATION.md) - 了解 tracker 的工作原理和使用方法
- [Seeder 类使用说明](docs/SEEDER_USAGE.md) - Seeder 类的详细 API 说明和使用示例
- [Downloader 类使用说明](docs/DOWNLOADER_USAGE.md) - Downloader 类的详细 API 说明和使用示例

## 依赖说明

### boost/1.81.0
- 配置为静态链接（`shared=False`）
- 提供 C++ 标准库扩展功能

### libtorrent/2.0.10
- 配置为静态链接（`shared=False`）
- BitTorrent 协议实现库
- 依赖 boost、openssl、zlib、bzip2

## 清理

清除所有本地 Conan 包：

```bash
conan remove "*" -c
```

清除构建文件：

```bash
Remove-Item -Path build -Recurse -Force
```

## 注意事项

- 所有 Conan 生成的文件都会放在 `build` 文件夹中，保持项目根目录整洁
- 首次安装可能需要较长时间，因为需要从源码构建 libtorrent
- 确保已安装 Visual Studio 2022 并配置好 MSVC 编译器

## 许可证

MIT License
