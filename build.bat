@echo off
setlocal enabledelayedexpansion

echo.
echo ========================================
echo    DisklessWorkstation 自动化构建脚本
echo ========================================
echo.

:: 检查必需的工具
echo [1/4] 检查构建工具...
where conan >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 Conan，请确保已安装并添加到 PATH
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 CMake，请确保已安装并添加到 PATH
    exit /b 1
)

echo [成功] Conan 和 CMake 已找到
echo.

:: 检查是否跳过依赖安装
set "SKIP_INSTALL=0"
if "%1"=="--skip-install" (
    set "SKIP_INSTALL=1"
    echo [信息] 跳过依赖安装步骤
    echo.
)

:: 步骤 1: 安装依赖
if "%SKIP_INSTALL%"=="0" (
    echo [2/4] 安装 Conan 依赖...
    echo 执行: conan install . -pr=conan_profiles/msvc-vs2022 --build=missing -of build
    echo.
    
    conan install . -pr=conan_profiles/msvc-vs2022 --build=missing -of build
    if errorlevel 1 (
        echo.
        echo [错误] 依赖安装失败
        exit /b 1
    )
    
    echo.
    echo [成功] 依赖安装成功
    echo.
) else (
    echo [2/4] 跳过依赖安装
    echo.
)

:: 检查工具链文件是否存在
if not exist "build\conan_toolchain.cmake" (
    echo [错误] 未找到 build\conan_toolchain.cmake 文件
    echo 请先运行依赖安装步骤
    exit /b 1
)

:: 步骤 2: 配置项目
echo [3/4] 配置 CMake 项目...
echo 执行: cmake --preset conan-default
echo.

cmake --preset conan-default
if errorlevel 1 (
    echo.
    echo [错误] CMake 配置失败
    exit /b 1
)

echo.
echo [成功] CMake 配置成功
echo.

:: 步骤 3: 构建项目
echo [4/4] 构建项目 (Release 配置)...
echo 执行: cmake --build --preset conan-release
echo.

cmake --build --preset conan-release
if errorlevel 1 (
    echo.
    echo [错误] 项目构建失败
    exit /b 1
)

echo.
echo ========================================
echo [成功] 构建成功完成！
echo ========================================
echo.
echo 可执行文件位置: build\bin\Release\DisklessWorkstation.exe
echo.

endlocal
exit /b 0

