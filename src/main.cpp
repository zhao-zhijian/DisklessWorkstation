#include <iostream>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <libtorrent/version.hpp>
#include "torrent_builder.hpp"
#include "seeder.hpp"

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // 设置控制台代码页为 UTF-8，解决中文乱码问题
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    try {
        std::cout << "=== LibTorrent Torrent 生成器 ===" << std::endl;
        std::cout << "LibTorrent Version: " << LIBTORRENT_VERSION << std::endl;
        std::cout << std::endl;

        // 示例用法
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
            // 如果没有提供参数，使用示例路径
            std::cout << "用法: " << argv[0] << " <文件或目录路径> [输出.torrent文件路径]" << std::endl;
            std::cout << std::endl;
            std::cout << "示例: " << argv[0] << " C:\\MyFiles\\example.txt example.torrent" << std::endl;
            std::cout << std::endl;
            
            // 可以在这里设置默认的测试路径
            // file_path = "test_file.txt";
            // output_path = "test_file.torrent";
            
            std::cout << "请提供文件或目录路径作为参数" << std::endl;
            return 1;
        }

        // 创建 TorrentBuilder 实例
        TorrentBuilder builder;
        
        // 配置 tracker 列表（可选）
        // 注意：添加 tracker URL 只是将其写入 torrent 文件
        // 真正的"上传"到 tracker 需要：
        // 1. 使用 BitTorrent 客户端打开 torrent 文件
        // 2. 开始做种（Seeding）
        // 3. 客户端会自动向 tracker 发送 announce 请求，tracker 会记录你的做种信息
        std::vector<std::string> trackers = {
            // 公共 tracker 示例（可以取消注释使用）:
            "udp://tracker.openbittorrent.com:80/announce",
            "udp://tracker.publicbt.com:80/announce",
            "udp://tracker.istole.it:80/announce",
            "http://tracker.bt-chat.com/announce",
            "http://172.16.1.63:6880/announce",
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
                
                // 确定保存路径（原始文件/目录的路径）
                std::filesystem::path file_path_obj(file_path);
                std::string save_path = file_path_obj.parent_path().string();
                if (save_path.empty()) {
                    save_path = ".";
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