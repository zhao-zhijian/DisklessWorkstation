#include <iostream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif
#include <libtorrent/version.hpp>
#include "torrent_creator.hpp"

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // 设置控制台代码页为 UTF-8，解决中文乱码问题
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    try
    {
        std::cout << "=== LibTorrent Torrent 生成器 ===" << std::endl;
        std::cout << "LibTorrent Version: " << LIBTORRENT_VERSION << std::endl;
        std::cout << std::endl;

        // 示例用法
        std::string file_path;
        std::string output_path;
        
        if (argc >= 2)
        {
            file_path = argv[1];
            if (argc >= 3)
            {
                output_path = argv[2];
            }
            else
            {
                // 如果没有指定输出路径，使用默认名称
                std::filesystem::path p(file_path);
                output_path = p.filename().string() + ".torrent";
            }
        }
        else
        {
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

        // 创建 TorrentCreator 实例
        TorrentCreator creator;
        
        // 配置 tracker 列表（可选）
        std::vector<std::string> trackers = {
            // 可以添加 tracker URL，例如:
            // "udp://tracker.openbittorrent.com:80/announce",
            // "udp://tracker.publicbt.com:80/announce",
        };
        creator.set_trackers(trackers);
        
        // 设置注释
        creator.set_comment("由 DisklessWorkstation 创建");
        
        // 生成 torrent 文件
        std::cout << "输入路径: " << file_path << std::endl;
        std::cout << "输出路径: " << output_path << std::endl;
        std::cout << std::endl;

        if (creator.create_torrent(file_path, output_path))
        {
            std::cout << std::endl;
            std::cout << "=== Torrent 生成完成 ===" << std::endl;
            return 0;
        }
        else
        {
            std::cout << std::endl;
            std::cout << "=== Torrent 生成失败 ===" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}

