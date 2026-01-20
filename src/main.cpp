#include <iostream>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <libtorrent/version.hpp>
#include "torrent_builder.hpp"
#include "torrent_manager.hpp"
#include <cstdio>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>

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
        bool test_manager_mode = false;
        if (argc >= 2) {
            std::string first_arg = argv[1];
            if (first_arg == "-s" || first_arg == "--seed") {
                direct_seed_mode = true;
            } else if (first_arg == "-d" || first_arg == "--download") {
                download_mode = true;
            } else if (first_arg == "-m" || first_arg == "--multi-seed") {
                multi_seed_mode = true;
            } else if (first_arg == "-t" || first_arg == "--test-manager") {
                test_manager_mode = true;
            }
        }
        
        // TorrentManager 测试模式
        if (test_manager_mode) {
            std::cout << "=== TorrentManager 测试模式 ===" << std::endl;
            std::cout << std::endl;
            
            // 测试1: 单例模式测试
            std::cout << "[测试1] 单例模式测试..." << std::endl;
            TorrentManager& manager1 = TorrentManager::getInstance();
            TorrentManager& manager2 = TorrentManager::getInstance();
            if (&manager1 == &manager2) {
                std::cout << "✓ 单例模式测试通过：两个引用指向同一个实例" << std::endl;
            } else {
                std::cerr << "✗ 单例模式测试失败：两个引用指向不同实例" << std::endl;
            }
            std::cout << std::endl;
            
            // 检查参数
            if (argc < 3) {
                std::cout << "用法（TorrentManager测试）: " << argv[0] << " -t <测试模式>" << std::endl;
                std::cout << std::endl;
                std::cout << "测试模式:" << std::endl;
                std::cout << "  basic      - 基础功能测试（需要提供torrent文件和路径）" << std::endl;
                std::cout << "  concurrent - 并发测试（需要提供多个torrent文件和路径）" << std::endl;
                std::cout << std::endl;
                std::cout << "基础测试示例:" << std::endl;
                std::cout << "  " << argv[0] << " -t basic <torrent文件> <下载保存路径>" << std::endl;
                std::cout << "  " << argv[0] << " -t basic <torrent文件> <做种保存路径> --seed" << std::endl;
                std::cout << "  " << argv[0] << " -t basic <torrent文件> <下载保存路径> <做种保存路径>" << std::endl;
                std::cout << std::endl;
                std::cout << "并发测试示例:" << std::endl;
                std::cout << "  " << argv[0] << " -t concurrent <torrent1> <保存路径1> [torrent2] [保存路径2] ..." << std::endl;
                std::cout << std::endl;
                std::cout << "交互式测试示例:" << std::endl;
                std::cout << "  " << argv[0] << " -t interactive" << std::endl;
                return 1;
            }
            
            std::string test_mode = argv[2];
            
            // 基础功能测试
            if (test_mode == "basic") {
                // 检查参数：支持多种模式
                // 模式1: basic <torrent文件> <保存路径> - 只测试下载
                // 模式2: basic <torrent文件> <保存路径> --seed - 只测试做种
                // 模式3: basic <torrent文件> <下载保存路径> <做种保存路径> - 测试下载和做种
                // 模式4: basic <torrent文件> <保存路径> --peer <IP:端口> - 下载并手动添加peer
                if (argc < 5) {
                    std::cout << "用法1（仅下载）: " << argv[0] << " -t basic <torrent文件> <下载保存路径>" << std::endl;
                    std::cout << "用法2（仅做种）: " << argv[0] << " -t basic <torrent文件> <做种保存路径> --seed" << std::endl;
                    std::cout << "用法3（下载+做种）: " << argv[0] << " -t basic <torrent文件> <下载保存路径> <做种保存路径>" << std::endl;
                    std::cout << "用法4（下载+手动peer）: " << argv[0] << " -t basic <torrent文件> <下载保存路径> --peer <IP:端口>" << std::endl;
                    std::cout << std::endl;
                    std::cout << "说明:" << std::endl;
                    std::cout << "  如果只提供保存路径，则只测试下载功能" << std::endl;
                    std::cout << "  如果提供 --seed 标志，则只测试做种功能" << std::endl;
                    std::cout << "  如果提供两个路径，则同时测试下载和做种功能" << std::endl;
                    std::cout << "  如果提供 --peer 标志和 IP:端口，则手动添加做种端（用于 Tracker 不可用时）" << std::endl;
                    return 1;
                }
                
                std::string torrent_path = argv[3];
                std::string path1 = argv[4];
                std::string path2 = (argc >= 6) ? argv[5] : "";
                std::string peer_addr = "";  // 手动添加的 peer 地址
                
                // 检查是否有 --peer 参数
                for (int i = 5; i < argc - 1; i++) {
                    if (std::string(argv[i]) == "--peer") {
                        peer_addr = argv[i + 1];
                        break;
                    }
                }
                
                // 判断模式
                bool test_download = false;
                bool test_seeding = false;
                std::string download_save_path;
                std::string seeding_save_path;
                
                if (path2 == "--seed") {
                    // 模式2: 只测试做种
                    test_download = false;
                    test_seeding = true;
                    seeding_save_path = path1;
                } else if (path2 == "--peer") {
                    // 模式4: 下载并手动添加 peer
                    test_download = true;
                    test_seeding = false;
                    download_save_path = path1;
                } else if (!path2.empty()) {
                    // 模式3: 同时测试下载和做种
                    test_download = true;
                    test_seeding = true;
                    download_save_path = path1;
                    seeding_save_path = path2;
                } else {
                    // 模式1: 只测试下载
                    test_download = true;
                    test_seeding = false;
                    download_save_path = path1;
                }
                
                std::string download_hash;
                std::string seeding_hash;
                
                // 测试2: 启动下载（如果需要）
                if (test_download) {
                    std::cout << "[测试2] 启动下载测试..." << std::endl;
                    download_hash = manager1.start_download(torrent_path, download_save_path);
                    if (!download_hash.empty()) {
                        std::cout << "✓ 下载任务启动成功，info_hash: " << download_hash.substr(0, 16) << "..." << std::endl;
                        
                        // 如果指定了 peer 地址，手动添加
                        if (!peer_addr.empty()) {
                            std::cout << "正在手动添加 peer: " << peer_addr << std::endl;
                            
                            // 解析 IP:端口
                            std::string peer_ip;
                            int peer_port = 6881;  // 默认端口
                            size_t colon_pos = peer_addr.find(':');
                            if (colon_pos != std::string::npos) {
                                peer_ip = peer_addr.substr(0, colon_pos);
                                peer_port = std::stoi(peer_addr.substr(colon_pos + 1));
                            } else {
                                peer_ip = peer_addr;
                            }
                            
                            if (manager1.add_peer(download_hash, peer_ip, peer_port)) {
                                std::cout << "✓ 已手动添加 peer: " << peer_ip << ":" << peer_port << std::endl;
                            } else {
                                std::cerr << "✗ 添加 peer 失败" << std::endl;
                            }
                        }
                    } else {
                        std::cerr << "✗ 下载任务启动失败" << std::endl;
                        return 1;
                    }
                    std::cout << std::endl;
                }
                
                // 测试2b: 启动做种（如果需要）
                if (test_seeding) {
                    std::cout << "[测试2" << (test_download ? "b" : "") << "] 启动做种测试..." << std::endl;
                    seeding_hash = manager1.start_seeding(torrent_path, seeding_save_path);
                    if (!seeding_hash.empty()) {
                        std::cout << "✓ 做种任务启动成功，info_hash: " << seeding_hash.substr(0, 16) << "..." << std::endl;
                    } else {
                        std::cerr << "✗ 做种任务启动失败" << std::endl;
                        return 1;
                    }
                    std::cout << std::endl;
                }
                
                // 等待一段时间让任务开始
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
                // 测试3: 状态查询
                std::cout << "[测试3] 状态查询测试..." << std::endl;
                TorrentStatus status;
                
                if (test_download && !download_hash.empty()) {
                    status = manager1.get_torrent_status(download_hash);
                    if (status.is_valid) {
                        std::cout << "✓ 下载任务状态查询成功" << std::endl;
                        std::cout << "  类型: " << (status.type == TorrentType::Download ? "下载" : "做种") << std::endl;
                        std::cout << "  进度: " << (status.progress * 100.0) << "%" << std::endl;
                        std::cout << "  下载速度: " << format_bytes(status.download_rate) << "/s" << std::endl;
                    } else {
                        std::cerr << "✗ 下载任务状态查询失败" << std::endl;
                    }
                }
                
                if (test_seeding && !seeding_hash.empty()) {
                    status = manager1.get_torrent_status(seeding_hash);
                    if (status.is_valid) {
                        std::cout << "✓ 做种任务状态查询成功" << std::endl;
                        std::cout << "  类型: " << (status.type == TorrentType::Download ? "下载" : "做种") << std::endl;
                        std::cout << "  进度: " << (status.progress * 100.0) << "%" << std::endl;
                        std::cout << "  上传速度: " << format_bytes(status.upload_rate) << "/s" << std::endl;
                    } else {
                        std::cerr << "✗ 做种任务状态查询失败" << std::endl;
                    }
                }
                std::cout << std::endl;
                
                // 测试4: 暂停/恢复
                std::cout << "[测试4] 暂停/恢复测试..." << std::endl;
                
                if (test_download && !download_hash.empty()) {
                    if (manager1.pause_torrent(download_hash)) {
                        std::cout << "✓ 下载任务暂停成功" << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        status = manager1.get_torrent_status(download_hash);
                        if (status.is_paused) {
                            std::cout << "✓ 下载任务暂停状态确认" << std::endl;
                        }
                        
                        if (manager1.resume_torrent(download_hash)) {
                            std::cout << "✓ 下载任务恢复成功" << std::endl;
                        }
                    } else {
                        std::cerr << "✗ 下载任务暂停/恢复失败" << std::endl;
                    }
                }
                
                if (test_seeding && !seeding_hash.empty()) {
                    if (manager1.pause_torrent(seeding_hash)) {
                        std::cout << "✓ 做种任务暂停成功" << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        status = manager1.get_torrent_status(seeding_hash);
                        if (status.is_paused) {
                            std::cout << "✓ 做种任务暂停状态确认" << std::endl;
                        }
                        
                        if (manager1.resume_torrent(seeding_hash)) {
                            std::cout << "✓ 做种任务恢复成功" << std::endl;
                        }
                    } else {
                        std::cerr << "✗ 做种任务暂停/恢复失败" << std::endl;
                    }
                }
                std::cout << std::endl;
                
                // 测试5: 统计信息
                std::cout << "[测试5] 统计信息测试..." << std::endl;
                std::cout << "  总任务数: " << manager1.get_torrent_count() << std::endl;
                std::cout << "  下载任务数: " << manager1.get_download_count() << std::endl;
                std::cout << "  做种任务数: " << manager1.get_seeding_count() << std::endl;
                std::cout << "✓ 统计信息查询成功" << std::endl;
                std::cout << std::endl;
                
                // 测试6: 运行状态监控
                std::cout << "[测试6] 运行状态监控..." << std::endl;
                std::cout << "按 Ctrl+C 退出，每10秒显示详细状态" << std::endl;
                std::cout << std::endl;
                
                // 先显示网络状态诊断
                std::cout << "=== 初始网络状态诊断 ===" << std::endl;
                manager1.print_session_status();
                
                int counter = 0;
                while (true) {
                    manager1.wait_and_process(1000);
                    counter++;
                    
                    // 每10秒显示详细状态
                    if (counter % 10 == 0) {
                        std::cout << std::endl;
                        std::cout << "=== 当前状态（" << counter << "秒） ===" << std::endl;
                        
                        // 显示网络和 peer 状态
                        manager1.print_session_status();
                        
                        // 显示 torrent 状态
                        if (test_download && !download_hash.empty()) {
                            manager1.print_torrent_status(download_hash);
                        }
                        if (test_seeding && !seeding_hash.empty()) {
                            manager1.print_torrent_status(seeding_hash);
                        }
                    }
                    
                    // 每秒显示简要进度
                    if (test_download && !download_hash.empty()) {
                        TorrentStatus st = manager1.get_torrent_status(download_hash);
                        if (st.is_valid) {
                            std::cout << "\r下载进度: " << (st.progress * 100.0) << "% "
                                      << "速度: " << format_bytes(st.download_rate) << "/s "
                                      << "Peers: " << st.peer_count << "  " << std::flush;
                        }
                    }
                }
                std::cout << std::endl;
                
                // 测试7: 停止任务
                std::cout << "[测试7] 停止任务测试..." << std::endl;
                if (test_download && !download_hash.empty()) {
                    if (manager1.stop_torrent(download_hash)) {
                        std::cout << "✓ 下载任务停止成功" << std::endl;
                    } else {
                        std::cerr << "✗ 下载任务停止失败" << std::endl;
                    }
                }
                
                if (test_seeding && !seeding_hash.empty()) {
                    if (manager1.stop_torrent(seeding_hash)) {
                        std::cout << "✓ 做种任务停止成功" << std::endl;
                    } else {
                        std::cerr << "✗ 做种任务停止失败" << std::endl;
                    }
                }
                std::cout << std::endl;
                
                std::cout << "=== 基础功能测试完成 ===" << std::endl;
                return 0;
            }
            
            // 并发测试
            else if (test_mode == "concurrent") {
                if (argc < 5 || (argc - 3) % 2 != 0) {
                    std::cout << "用法: " << argv[0] << " -t concurrent <torrent1> <保存路径1> [torrent2] [保存路径2] ..." << std::endl;
                    std::cout << "参数必须是成对出现: <torrent文件路径> <保存路径>" << std::endl;
                    return 1;
                }
                
                std::cout << "[测试] 并发下载和做种测试..." << std::endl;
                std::cout << "将测试 " << (argc - 3) / 2 << " 个任务" << std::endl;
                std::cout << std::endl;
                
                std::vector<std::string> download_hashes;
                std::vector<std::string> seeding_hashes;
                
                // 解析参数对
                for (int i = 3; i < argc; i += 2) {
                    std::string torrent_path = argv[i];
                    std::string save_path = argv[i + 1];
                    int task_num = (i - 3) / 2 + 1;
                    
                    std::cout << "--- 任务 #" << task_num << " ---" << std::endl;
                    std::cout << "Torrent 文件: " << torrent_path << std::endl;
                    std::cout << "保存路径: " << save_path << std::endl;
                    
                    // 尝试启动下载
                    std::string hash = manager1.start_download(torrent_path, save_path);
                    if (!hash.empty()) {
                        download_hashes.push_back(hash);
                        std::cout << "✓ 下载任务启动成功" << std::endl;
                    } else {
                        // 如果下载失败，尝试做种
                        hash = manager1.start_seeding(torrent_path, save_path);
                        if (!hash.empty()) {
                            seeding_hashes.push_back(hash);
                            std::cout << "✓ 做种任务启动成功" << std::endl;
                        } else {
                            std::cerr << "✗ 启动失败" << std::endl;
                        }
                    }
                    std::cout << std::endl;
                }
                
                std::cout << "=== 任务启动完成 ===" << std::endl;
                std::cout << "下载任务: " << download_hashes.size() << " 个" << std::endl;
                std::cout << "做种任务: " << seeding_hashes.size() << " 个" << std::endl;
                std::cout << "总任务数: " << manager1.get_torrent_count() << std::endl;
                std::cout << std::endl;
                
                if (manager1.get_torrent_count() > 0) {
                    std::cout << "开始监控状态（30秒，每5秒更新一次）..." << std::endl;
                    std::cout << "按 Ctrl+C 提前停止" << std::endl;
                    std::cout << std::endl;
                    
                    int counter = 0;
                    while (counter < 30 && manager1.get_torrent_count() > 0) {
                        manager1.wait_and_process(1000);
                        counter++;
                        
                        if (counter % 5 == 0) {
                            std::cout << std::endl;
                            std::cout << "=== 状态更新（" << counter << "秒） ===" << std::endl;
                            manager1.print_all_status();
                            
                            // 显示统计信息
                            std::vector<TorrentStatus> all_status = manager1.get_all_torrent_status();
                            std::int64_t total_download = 0;
                            std::int64_t total_upload = 0;
                            int total_peers = 0;
                            
                            for (const auto& s : all_status) {
                                total_download += s.downloaded_bytes;
                                total_upload += s.uploaded_bytes;
                                total_peers += s.peer_count;
                            }
                            
                            std::cout << "总统计:" << std::endl;
                            std::cout << "  总下载: " << format_bytes(total_download) << std::endl;
                            std::cout << "  总上传: " << format_bytes(total_upload) << std::endl;
                            std::cout << "  总Peer数: " << total_peers << std::endl;
                            std::cout << std::endl;
                        }
                    }
                    
                    std::cout << std::endl;
                    std::cout << "=== 测试完成，停止所有任务 ===" << std::endl;
                    manager1.stop_all();
                } else {
                    std::cerr << "没有成功启动任何任务" << std::endl;
                    return 1;
                }
                
                return 0;
            }
            
            // 交互式测试
            else if (test_mode == "interactive") {
                std::cout << "=== 交互式测试模式 ===" << std::endl;
                std::cout << "输入命令来测试 TorrentManager" << std::endl;
                std::cout << "可用命令:" << std::endl;
                std::cout << "  download <torrent文件> <保存路径>  - 启动下载" << std::endl;
                std::cout << "  seed <torrent文件> <保存路径>       - 启动做种" << std::endl;
                std::cout << "  status                              - 显示所有状态" << std::endl;
                std::cout << "  status <info_hash>                  - 显示指定任务状态" << std::endl;
                std::cout << "  pause <info_hash>                   - 暂停任务" << std::endl;
                std::cout << "  resume <info_hash>                  - 恢复任务" << std::endl;
                std::cout << "  stop <info_hash>                    - 停止任务" << std::endl;
                std::cout << "  stop-all                             - 停止所有任务" << std::endl;
                std::cout << "  stats                                - 显示统计信息" << std::endl;
                std::cout << "  quit                                 - 退出" << std::endl;
                std::cout << std::endl;
                
                std::string command;
                while (true) {
                    std::cout << "> ";
                    std::getline(std::cin, command);
                    
                    if (command.empty()) continue;
                    
                    // 解析命令
                    std::istringstream iss(command);
                    std::string cmd;
                    iss >> cmd;
                    
                    if (cmd == "quit" || cmd == "exit" || cmd == "q") {
                        std::cout << "退出测试..." << std::endl;
                        manager1.stop_all();
                        break;
                    }
                    else if (cmd == "download") {
                        std::string torrent_path, save_path;
                        if (iss >> torrent_path >> save_path) {
                            std::string hash = manager1.start_download(torrent_path, save_path);
                            if (!hash.empty()) {
                                std::cout << "✓ 下载已启动，info_hash: " << hash << std::endl;
                            } else {
                                std::cerr << "✗ 启动下载失败" << std::endl;
                            }
                        } else {
                            std::cerr << "用法: download <torrent文件> <保存路径>" << std::endl;
                        }
                    }
                    else if (cmd == "seed") {
                        std::string torrent_path, save_path;
                        if (iss >> torrent_path >> save_path) {
                            std::string hash = manager1.start_seeding(torrent_path, save_path);
                            if (!hash.empty()) {
                                std::cout << "✓ 做种已启动，info_hash: " << hash << std::endl;
                            } else {
                                std::cerr << "✗ 启动做种失败" << std::endl;
                            }
                        } else {
                            std::cerr << "用法: seed <torrent文件> <保存路径>" << std::endl;
                        }
                    }
                    else if (cmd == "status") {
                        std::string hash;
                        if (iss >> hash) {
                            manager1.print_torrent_status(hash);
                        } else {
                            manager1.print_all_status();
                        }
                    }
                    else if (cmd == "pause") {
                        std::string hash;
                        if (iss >> hash) {
                            if (manager1.pause_torrent(hash)) {
                                std::cout << "✓ 已暂停" << std::endl;
                            } else {
                                std::cerr << "✗ 暂停失败" << std::endl;
                            }
                        } else {
                            std::cerr << "用法: pause <info_hash>" << std::endl;
                        }
                    }
                    else if (cmd == "resume") {
                        std::string hash;
                        if (iss >> hash) {
                            if (manager1.resume_torrent(hash)) {
                                std::cout << "✓ 已恢复" << std::endl;
                            } else {
                                std::cerr << "✗ 恢复失败" << std::endl;
                            }
                        } else {
                            std::cerr << "用法: resume <info_hash>" << std::endl;
                        }
                    }
                    else if (cmd == "stop") {
                        std::string hash;
                        if (iss >> hash) {
                            if (manager1.stop_torrent(hash)) {
                                std::cout << "✓ 已停止" << std::endl;
                            } else {
                                std::cerr << "✗ 停止失败" << std::endl;
                            }
                        } else {
                            std::cerr << "用法: stop <info_hash>" << std::endl;
                        }
                    }
                    else if (cmd == "stop-all") {
                        manager1.stop_all();
                        std::cout << "✓ 已停止所有任务" << std::endl;
                    }
                    else if (cmd == "stats") {
                        std::cout << "统计信息:" << std::endl;
                        std::cout << "  总任务数: " << manager1.get_torrent_count() << std::endl;
                        std::cout << "  下载任务数: " << manager1.get_download_count() << std::endl;
                        std::cout << "  做种任务数: " << manager1.get_seeding_count() << std::endl;
                        
                        std::vector<TorrentStatus> all_status = manager1.get_all_torrent_status();
                        for (const auto& s : all_status) {
                            std::cout << "  [" << s.info_hash.substr(0, 8) << "...] "
                                      << (s.type == TorrentType::Download ? "下载" : "做种") << " "
                                      << (s.progress * 100.0) << "%" << std::endl;
                        }
                    }
                    else {
                        std::cerr << "未知命令: " << cmd << std::endl;
                    }
                    
                    // 处理事件
                    manager1.wait_and_process(100);
                }
                
                return 0;
            }
            
            else {
                std::cerr << "未知的测试模式: " << test_mode << std::endl;
                std::cout << "可用模式: basic, concurrent, interactive" << std::endl;
                return 1;
            }
        }
        
        // 下载模式（使用 TorrentManager）
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
            
            std::cout << "=== 下载模式（使用 TorrentManager） ===" << std::endl;
            std::cout << "Torrent 文件: " << torrent_path << std::endl;
            std::cout << "保存路径: " << save_path << std::endl;
            std::cout << std::endl;
            
            TorrentManager& manager = TorrentManager::getInstance();
            std::string download_hash = manager.start_download(torrent_path, save_path);
            if (download_hash.empty()) {
                std::cerr << "启动下载失败" << std::endl;
                return 1;
            }
            
            std::cout << std::endl;
            std::cout << "下载已启动，按 Ctrl+C 停止下载" << std::endl;
            std::cout << "info_hash: " << download_hash << std::endl;
            std::cout << std::endl;
            
            // 主循环：保持下载状态并定期显示状态
            int status_counter = 0;
            while (true) {
                manager.wait_and_process(1000);
                
                TorrentStatus status = manager.get_torrent_status(download_hash);
                if (!status.is_valid) {
                    std::cout << "下载任务已结束或不存在" << std::endl;
                    break;
                }
                
                // 每 10 秒显示一次状态
                status_counter++;
                if (status_counter >= 10) {
                    manager.print_torrent_status(download_hash);
                    status_counter = 0;
                }
                
                if (status.is_finished) {
                    std::cout << std::endl;
                    std::cout << "=== 下载完成！===" << std::endl;
                    manager.print_torrent_status(download_hash);
                    break;
                }
            }
            
            return 0;
        }
        
        // 多torrent同时做种模式（使用 TorrentManager）
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
            
            std::cout << "=== 多Torrent同时做种模式（使用 TorrentManager） ===" << std::endl;
            std::cout << "将同时做种 " << (argc - 2) / 2 << " 个torrent" << std::endl;
            std::cout << std::endl;
            
            TorrentManager& manager = TorrentManager::getInstance();
            std::vector<std::string> seeding_hashes;
            int success_count = 0;
            int fail_count = 0;
            
            // 解析参数对：每两个参数为一对（torrent路径, 保存路径）
            for (int i = 2; i < argc; i += 2) {
                std::string torrent_path = argv[i];
                std::string save_path = argv[i + 1];
                
                std::cout << "--- 添加 Torrent #" << (i / 2) << " ---" << std::endl;
                std::cout << "Torrent 文件: " << torrent_path << std::endl;
                std::cout << "保存路径: " << save_path << std::endl;
                
                std::string hash = manager.start_seeding(torrent_path, save_path);
                if (!hash.empty()) {
                    success_count++;
                    seeding_hashes.push_back(hash);
                    std::cout << "✓ 成功添加，info_hash: " << hash.substr(0, 16) << "..." << std::endl;
                } else {
                    fail_count++;
                    std::cerr << "✗ 添加失败" << std::endl;
                }
                std::cout << std::endl;
            }
            
            std::cout << "=== 添加完成 ===" << std::endl;
            std::cout << "成功: " << success_count << " 个" << std::endl;
            std::cout << "失败: " << fail_count << " 个" << std::endl;
            std::cout << "当前做种数量: " << manager.get_seeding_count() << " 个" << std::endl;
            std::cout << std::endl;
            
            if (success_count > 0) {
                std::cout << "所有torrent已启动，按 Ctrl+C 停止做种" << std::endl;
                std::cout << std::endl;
                
                // 主循环：保持做种状态并定期显示状态
                int status_counter = 0;
                while (manager.get_seeding_count() > 0) {
                    // 处理事件
                    manager.wait_and_process(1000);
                    
                    // 每 10 秒显示一次状态
                    status_counter++;
                    if (status_counter >= 10) {
                        std::cout << std::endl;
                        std::cout << "=== 当前状态（每10秒更新） ===" << std::endl;
                        manager.print_all_status();
                        
                        // 统计做种相关信息
                        std::vector<TorrentStatus> seeding_status = manager.get_seeding_status();
                        std::int64_t total_upload = 0;
                        std::int64_t total_download = 0;
                        int total_peers = 0;
                        for (const auto& s : seeding_status) {
                            total_upload += s.uploaded_bytes;
                            total_download += s.downloaded_bytes;
                            total_peers += s.peer_count;
                        }
                        std::cout << "总Peer数: " << total_peers << std::endl;
                        std::cout << "总上传: " << format_bytes(total_upload) << std::endl;
                        std::cout << "总下载: " << format_bytes(total_download) << std::endl;
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
        
        // 直接做种模式（使用 TorrentManager）
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
            
            std::cout << "=== 直接做种模式（使用 TorrentManager） ===" << std::endl;
            std::cout << "Torrent 文件: " << torrent_path << std::endl;
            std::cout << "保存路径: " << save_path << std::endl;
            std::cout << std::endl;
            
            TorrentManager& manager = TorrentManager::getInstance();
            std::string seeding_hash = manager.start_seeding(torrent_path, save_path);
            if (seeding_hash.empty()) {
                std::cerr << "启动做种失败" << std::endl;
                return 1;
            }
            
            std::cout << std::endl;
            std::cout << "做种已启动，按 Ctrl+C 停止做种" << std::endl;
            std::cout << "info_hash: " << seeding_hash << std::endl;
            std::cout << std::endl;
            
            // 主循环：保持做种状态并定期显示状态
            int status_counter = 0;
            while (manager.get_seeding_count() > 0) {
                // 处理事件
                manager.wait_and_process(1000);
                
                // 每 10 秒显示一次状态
                status_counter++;
                if (status_counter >= 10) {
                    manager.print_torrent_status(seeding_hash);
                    status_counter = 0;
                }
            }
            
            std::cout << "做种已停止" << std::endl;
            return 0;
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
            std::cout << "用法（TorrentManager测试）: " << argv[0] << " -t <测试模式>" << std::endl;
            std::cout << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  生成 torrent: " << argv[0] << " C:\\MyFiles\\example.txt example.torrent" << std::endl;
            std::cout << "  直接做种    : " << argv[0] << " -s example.torrent C:\\MyFiles" << std::endl;
            std::cout << "  多torrent做种: " << argv[0] << " -m torrent1.torrent C:\\Files1 torrent2.torrent C:\\Files2" << std::endl;
            std::cout << "  下载        : " << argv[0] << " -d example.torrent C:\\Downloads" << std::endl;
            std::cout << "  测试Manager : " << argv[0] << " -t basic example.torrent C:\\Downloads" << std::endl;
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
                
                // 使用 TorrentManager 开始做种
                TorrentManager& manager = TorrentManager::getInstance();
                std::string seeding_hash = manager.start_seeding(output_path, save_path);
                if (seeding_hash.empty()) {
                    std::cerr << "启动做种失败" << std::endl;
                    return 1;
                }
                
                std::cout << std::endl;
                std::cout << "做种已启动，按 Ctrl+C 停止做种" << std::endl;
                std::cout << "info_hash: " << seeding_hash << std::endl;
                std::cout << std::endl;
                
                // 主循环：保持做种状态并定期显示状态
                int status_counter = 0;
                while (manager.get_seeding_count() > 0) {
                    // 处理事件
                    manager.wait_and_process(1000);
                    
                    // 每 10 秒显示一次状态
                    status_counter++;
                    if (status_counter >= 10) {
                        manager.print_torrent_status(seeding_hash);
                        status_counter = 0;
                    }
                }
                
                std::cout << "做种已停止" << std::endl;
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