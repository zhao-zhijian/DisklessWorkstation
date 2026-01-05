cmake_minimum_required(VERSION 3.16)

# 包含 Conan 生成的工具链文件（必须在 project() 之前）
# 优先查找 build 目录（使用 -of build 时）
# 然后查找当前构建目录
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/conan_toolchain.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/build/conan_toolchain.cmake")
    message(STATUS "使用工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
elseif(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
    message(STATUS "使用工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conan_toolchain.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/conan_toolchain.cmake")
    message(STATUS "使用工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
endif()

project(DisklessWorkstation VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置 CMake 包搜索路径（查找 Conan 生成的包配置文件）
# 优先查找 build 目录（使用 -of build 时）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build")
    message(STATUS "添加包搜索路径: ${CMAKE_CURRENT_SOURCE_DIR}/build")
endif()

# 查找依赖包（由 Conan CMakeDeps 生成）
find_package(LibtorrentRasterbar REQUIRED)
find_package(Boost REQUIRED)

# 添加可执行文件
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/torrent_creator.cpp
)

# 添加 Windows 定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# 设置 MSVC 编译器使用 UTF-8 编码，解决 C4819 警告
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    LibtorrentRasterbar::torrent-rasterbar
    boost::boost
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 打印配置信息
message(STATUS "=== DisklessWorkstation 配置信息 ===")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "LibTorrent 版本: ${LibtorrentRasterbar_VERSION_STRING}")
message(STATUS "=====================================")

