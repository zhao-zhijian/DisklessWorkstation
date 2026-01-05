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

安装依赖后，可以使用 CMake 构建项目：

```bash
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . --config Release
```

或者使用 CMake Presets：

```bash
cmake --preset conan-default
cmake --build --preset conan-default
```

## 项目结构

```
DisklessWorkstation/
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
