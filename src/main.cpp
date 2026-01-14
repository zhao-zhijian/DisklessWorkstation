#include <iostream>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <libtorrent/version.hpp>
#include "torrent_builder.hpp"
#include "seeder.hpp"
#include "downloader.hpp"
#include <cstdio>

// 辅助函数：格式化字节数
static std::string format_bytes(std::int64_t bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
    return std::string(buffer);
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // 设置控制台代码页为 UTF-8，解决中文乱码问题
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    try {
        std::cout << "=== LibTorrent Torrent 工具 ===" << std::endl;
        std::cout << "LibTorrent Version: " << LIBTORRENT_VERSION << std::endl;
        std::cout << std::endl;

        // 检查运行模式
        bool direct_seed_mode = false;
        bool download_mode = false;
        bool multi_seed_mode = false;
        if (argc >= 2) {
            std::string first_arg = argv[1];
            if (first_arg == "-s" || first_arg == "--seed") {
                direct_seed_mode = true;
            } else if (first_arg == "-d" || first_arg == "--download") {
                download_mode = true;
            } else if (first_arg == "-m" || first_arg == "--multi-seed") {
                multi_seed_mode = true;
            }
        }
        
        // 下载模式
        if (download_mode) {
            // 下载模式：从 torrent 文件下载
            if (argc < 4) {
                std::cout << "用法（下载）: " << argv[0] << " -d <torrent文件路径> <保存路径>" << std::endl;
                std::cout << std::endl;
                std::cout << "示例: " << argv[0] << " -d example.torrent C:\\Downloads" << std::endl;
                std::cout << std::endl;
                std::cout << "说明: " << std::endl;
                std::cout << "  -d, --download : 下载模式" << std::endl;
                std::cout << "  torrent文件路径: 要下载的 .torrent 文件路径" << std::endl;
                std::cout << "  保存路径        : 下载文件的保存目录" << std::endl;
                return 1;
            }
            
            std::string torrent_path = argv[2];
            std::string save_path = argv[3];
            
            std::cout << "=== 下载模式 ===" << std::endl;
            std::cout << "Torrent 文件: " << torrent_path << std::endl;
            std::cout << "保存路径: " << save_path << std::endl;
            std::cout << std::endl;
            
            // 创建 Downloader 实例并开始下载
            Downloader downloader;
            if (downloader.start_download(torrent_path, save_path)) {
                std::cout << std::endl;
                std::cout << "下载已启动，按 Ctrl+C 停止下载" << std::endl;
                std::cout << std::endl;
                
                // 主循环：保持下载状态并定期显示状态
                int status_counter = 0;
                while (downloader.is_downloading() && !downloader.is_finished()) {
                    // 处理事件
                    downloader.wait_and_process(1000);
                    
                    // 每 10 秒显示一次状态
                    status_counter++;
                    if (status_counter >= 10) {
                        downloader.print_status();
                        status_counter = 0;
                    }
                }
                
                // 检查是否完成
                if (downloader.is_finished()) {
                    std::cout << std::endl;
                    std::cout << "=== 下载完成！===" << std::endl;
                    downloader.print_status();
                } else {
                    std::cout << "下载已停止" << std::endl;
                }
                return 0;
            } else {
                std::cerr << "启动下载失败" << std::endl;
                return 1;
            }
        }
        
        // 多torrent同时做种模式
        if (multi_seed_mode) {
            // 多torrent做种模式：同时做多个torrent
            if (argc < 4 || (argc - 2) % 2 != 0) {
                std::cout << "用法（多torrent做种）: " << argv[0] << " -m <torrent1> <保存路径1> [torrent2] [保存路径2] ..." << std::endl;
                std::cout << std::endl;
                std::cout << "示例: " << argv[0] << " -m torrent1.torrent C:\\Files1 torrent2.torrent C:\\Files2 torrent3.torrent C:\\Files3" << std::endl;
                std::cout << std::endl;
                std::cout << "说明: " << std::endl;
                std::cout << "  -m, --multi-seed : 多torrent同时做种模式" << std::endl;
                std::cout << "  参数必须是成对出现: <torrent文件路径> <保存路径>" << std::endl;
                std::cout << "  可以同时做多个torrent，所有torrent在同一个session中并发做种" << std::endl;
                return 1;
            }
            
            std::cout << "=== 多Torrent同时做种模式 ===" << std::endl;
            std::cout << "将同时做种 " << (argc - 2) / 2 << " 个torrent" << std::endl;
            std::cout << std::endl;
            
            // 创建 Seeder 实例（所有torrent共享同一个session）
            Seeder seeder;
            int success_count = 0;
            int fail_count = 0;
            
            // 解析参数对：每两个参数为一对（torrent路径, 保存路径）
            for (int i = 2; i < argc; i += 2) {
                std::string torrent_path = argv[i];
                std::string save_path = argv[i + 1];
                
                std::cout << "--- 添加 Torrent #" << (i / 2) << " ---" << std::endl;
                std::cout << "Torrent 文件: " << torrent_path << std::endl;
                std::cout << "保存路径: " << save_path << std::endl;
                
                if (seeder.start_seeding(torrent_path, save_path)) {
                    success_count++;
                    std::cout << "✓ 成功添加" << std::endl;
                } else {
                    fail_count++;
                    std::cerr << "✗ 添加失败" << std::endl;
                }
                std::cout << std::endl;
            }
            
            std::cout << "=== 添加完成 ===" << std::endl;
            std::cout << "成功: " << success_count << " 个" << std::endl;
            std::cout << "失败: " << fail_count << " 个" << std::endl;
            std::cout << "当前做种数量: " << seeder.get_torrent_count() << " 个" << std::endl;
            std::cout << std::endl;
            
            if (success_count > 0) {
                std::cout << "所有torrent已启动，按 Ctrl+C 停止做种" << std::endl;
                std::cout << std::endl;
                
                // 主循环：保持做种状态并定期显示状态
                int status_counter = 0;
                while (seeder.is_seeding()) {
                    // 处理事件
                    seeder.wait_and_process(1000);
                    
                    // 每 10 秒显示一次状态
                    status_counter++;
                    if (status_counter >= 10) {
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
                
                std::cout << "所有做种已停止" << std::endl;
                return 0;
            } else {
                std::cerr << "没有成功启动任何做种任务" << std::endl;
                return 1;
            }
        }
        
        // 直接做种模式
        if (direct_seed_mode) {
            // 直接做种模式：使用已有的 torrent 文件
            if (argc < 4) {
                std::cout << "用法（直接做种）: " << argv[0] << " -s <torrent文件路径> <保存路径>" << std::endl;
                std::cout << std::endl;
                std::cout << "示例: " << argv[0] << " -s example.torrent C:\\MyFiles" << std::endl;
                std::cout << std::endl;
                std::cout << "说明: " << std::endl;
                std::cout << "  -s, --seed    : 直接做种模式（跳过生成 torrent 文件）" << std::endl;
                std::cout << "  torrent文件路径: 已有的 .torrent 文件路径" << std::endl;
                std::cout << "  保存路径        : 原始文件/目录的保存路径（必须与创建 torrent 时的路径一致）" << std::endl;
                return 1;
            }
            
            std::string torrent_path = argv[2];
            std::string save_path = argv[3];
            
            std::cout << "=== 直接做种模式 ===" << std::endl;
            std::cout << "Torrent 文件: " << torrent_path << std::endl;
            std::cout << "保存路径: " << save_path << std::endl;
            std::cout << std::endl;
            
            // 创建 Seeder 实例并开始做种
            Seeder seeder;
            if (seeder.start_seeding(torrent_path, save_path)) {
                std::cout << std::endl;
                std::cout << "做种已启动，按 Ctrl+C 停止做种" << std::endl;
                std::cout << std::endl;
                
                // 主循环：保持做种状态并定期显示状态
                int status_counter = 0;
                while (seeder.is_seeding()) {
                    // 处理事件
                    seeder.wait_and_process(1000);
                    
                    // 每 10 秒显示一次状态
                    status_counter++;
                    if (status_counter >= 10) {
                        seeder.print_status();
                        status_counter = 0;
                    }
                }
                
                std::cout << "做种已停止" << std::endl;
                return 0;
            } else {
                std::cerr << "启动做种失败" << std::endl;
                return 1;
            }
        }
        
        // 原有模式：生成 torrent 文件
        std::string file_path;
        std::string output_path;
        
        if (argc >= 2) {
            file_path = argv[1];
            if (argc >= 3) {
                output_path = argv[2];
            } else {
                // 如果没有指定输出路径，使用默认名称
                std::filesystem::path p(file_path);
                output_path = p.filename().string() + ".torrent";
            }
        } else {
            // 如果没有提供参数，显示用法
            std::cout << "用法（生成 torrent）: " << argv[0] << " <文件或目录路径> [输出.torrent文件路径]" << std::endl;
            std::cout << std::endl;
            std::cout << "用法（直接做种）: " << argv[0] << " -s <torrent文件路径> <保存路径>" << std::endl;
            std::cout << std::endl;
            std::cout << "用法（多torrent做种）: " << argv[0] << " -m <torrent1> <保存路径1> [torrent2] [保存路径2] ..." << std::endl;
            std::cout << std::endl;
            std::cout << "用法（下载）    : " << argv[0] << " -d <torrent文件路径> <保存路径>" << std::endl;
            std::cout << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  生成 torrent: " << argv[0] << " C:\\MyFiles\\example.txt example.torrent" << std::endl;
            std::cout << "  直接做种    : " << argv[0] << " -s example.torrent C:\\MyFiles" << std::endl;
            std::cout << "  多torrent做种: " << argv[0] << " -m torrent1.torrent C:\\Files1 torrent2.torrent C:\\Files2" << std::endl;
            std::cout << "  下载        : " << argv[0] << " -d example.torrent C:\\Downloads" << std::endl;
            std::cout << std::endl;
            
            std::cout << "请提供文件或目录路径作为参数" << std::endl;
            return 1;
        }

        // 创建 TorrentBuilder 实例（生成 torrent 模式）
        std::cout << "=== Torrent 生成模式 ===" << std::endl;
        TorrentBuilder builder;
        
        // 配置 tracker 列表（可选）
        // 注意：添加 tracker URL 只是将其写入 torrent 文件
        // 真正的"上传"到 tracker 需要：
        // 1. 使用 BitTorrent 客户端打开 torrent 文件
        // 2. 开始做种（Seeding）
        // 3. 客户端会自动向 tracker 发送 announce 请求，tracker 会记录你的做种信息
        std::vector<std::string> trackers = {
            // 公共 tracker 示例（可以取消注释使用）:
            // "udp://tracker.openbittorrent.com:80/announce",
            // "udp://tracker.publicbt.com:80/announce",
            // "udp://tracker.istole.it:80/announce",
            // "http://tracker.bt-chat.com/announce",
            "http://172.16.1.63:6880/announce",
            "http://124.71.64.241:6969/announce",
            "http://124.71.64.241:6880/announce",
        };
        builder.set_trackers(trackers);
        
        // 设置注释
        builder.set_comment("由 DisklessWorkstation 创建");
        
        // 生成 torrent 文件
        std::cout << "输入路径: " << file_path << std::endl;
        std::cout << "输出路径: " << output_path << std::endl;
        std::cout << std::endl;

        if (builder.create_torrent(file_path, output_path)) {
            std::cout << std::endl;
            std::cout << "=== Torrent 生成完成 ===" << std::endl;
            std::cout << std::endl;
            
            // 询问是否开始做种
            std::cout << "是否开始做种？(y/n): ";
            std::string answer;
            std::getline(std::cin, answer);
            
            if (answer == "y" || answer == "Y" || answer == "yes" || answer == "YES") {
                std::cout << std::endl;
                std::cout << "=== 开始做种 ===" << std::endl;
                
                // 确定保存路径（原始文件/目录的根路径）
                // 这个路径应该与创建 torrent 时使用的根路径一致
                std::filesystem::path file_path_obj(file_path);
                std::string save_path;
                
                if (std::filesystem::is_directory(file_path)) {
                    // 如果是目录，使用目录的父目录作为 save_path
                    save_path = file_path_obj.parent_path().string();
                    if (save_path.empty()) {
                        save_path = ".";
                    }
                } else {
                    // 如果是文件，使用文件的父目录作为 save_path
                    save_path = file_path_obj.parent_path().string();
                    if (save_path.empty()) {
                        save_path = ".";
                    }
                }
                
                // 创建 Seeder 实例并开始做种
                Seeder seeder;
                if (seeder.start_seeding(output_path, save_path)) {
                    std::cout << std::endl;
                    std::cout << "做种已启动，按 Ctrl+C 停止做种" << std::endl;
                    std::cout << std::endl;
                    
                    // 主循环：保持做种状态并定期显示状态
                    int status_counter = 0;
                    while (seeder.is_seeding()) {
                        // 处理事件
                        seeder.wait_and_process(1000);
                        
                        // 每 10 秒显示一次状态
                        status_counter++;
                        if (status_counter >= 10) {
                            seeder.print_status();
                            status_counter = 0;
                        }
                    }
                    
                    std::cout << "做种已停止" << std::endl;
                } else {
                    std::cerr << "启动做种失败" << std::endl;
                    return 1;
                }
            } else {
                std::cout << "跳过做种步骤" << std::endl;
                std::cout << "提示: 你可以稍后使用 BitTorrent 客户端打开 torrent 文件开始做种" << std::endl;
            }
            
            return 0;
        } else {
            std::cout << std::endl;
            std::cout << "=== Torrent 生成失败 ===" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}