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
├── build/                    # 构建输出目录（由 Conan 生成）
└── README.md                 # 本文件
```

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
