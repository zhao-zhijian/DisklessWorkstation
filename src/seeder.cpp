#include "seeder.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <thread>
#include <chrono>
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

// 辅助函数：格式化速度
static std::string format_speed(int rate_bytes_per_sec)
{
    if (rate_bytes_per_sec == 0) {
        return "0 B/s";
    }
    
    return format_bytes(rate_bytes_per_sec) + "/s";
}

Seeder::Seeder()
    : session_(nullptr)
    , is_seeding_(false)
{
    configure_session();
}

Seeder::~Seeder()
{
    stop_seeding();
}

void Seeder::configure_session()
{
    try {
        // 创建 session 配置
        lt::settings_pack settings;
        settings.set_int(lt::settings_pack::alert_mask, 
                         lt::alert::status_notification | 
                         lt::alert::error_notification |
                         lt::alert::peer_notification |
                         lt::alert::storage_notification);
        
        // 设置监听接口（空字符串表示自动选择）
        settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:0");
        
        // 启用 DHT
        settings.set_bool(lt::settings_pack::enable_dht, true);
        
        // 启用本地服务发现
        settings.set_bool(lt::settings_pack::enable_lsd, true);
        
        // 启用 UPnP 和 NAT-PMP
        settings.set_bool(lt::settings_pack::enable_upnp, true);
        settings.set_bool(lt::settings_pack::enable_natpmp, true);
        
        // 设置上传/下载速度限制（0 表示无限制）
        settings.set_int(lt::settings_pack::download_rate_limit, 0);
        settings.set_int(lt::settings_pack::upload_rate_limit, 0);
        
        // 设置最大连接数（大文件需要更多连接）
        settings.set_int(lt::settings_pack::connections_limit, 200);
        
        // 设置磁盘缓存大小（大文件需要更大的缓存）
        // 默认是32MB，对于大文件可以增加到256MB
        settings.set_int(lt::settings_pack::cache_size, 256);
        
        // 设置磁盘缓存过期时间（毫秒）
        settings.set_int(lt::settings_pack::cache_expiry, 300);
        
        // 创建 session
        session_ = std::make_unique<lt::session>(settings);
        
        std::cout << "Seeder 会话已初始化" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "初始化 Seeder 会话失败: " << e.what() << std::endl;
        throw;
    }
}

bool Seeder::validate_paths(const std::string& torrent_path, const std::string& save_path)
{
    namespace fs = std::filesystem;
    
    // 验证 torrent 文件是否存在
    if (!fs::exists(torrent_path)) {
        std::cerr << "错误: Torrent 文件不存在: " << torrent_path << std::endl;
        return false;
    }
    
    // 验证保存路径是否存在
    if (!fs::exists(save_path)) {
        std::cerr << "错误: 保存路径不存在: " << save_path << std::endl;
        std::cerr << "提示: 保存路径必须指向创建 torrent 时的原始文件或目录" << std::endl;
        return false;
    }
    
    return true;
}

bool Seeder::start_seeding(const std::string& torrent_path, const std::string& save_path)
{
    try {
        // 如果已经在做种，先停止
        if (is_seeding_) {
            stop_seeding();
        }
        
        // 验证路径
        if (!validate_paths(torrent_path, save_path)) {
            return false;
        }
        
        // 加载 torrent 文件
        std::ifstream torrent_file(torrent_path, std::ios::binary);
        if (!torrent_file.is_open()) {
            std::cerr << "错误: 无法打开 torrent 文件: " << torrent_path << std::endl;
            return false;
        }
        
        // 读取 torrent 文件内容
        std::vector<char> torrent_data((std::istreambuf_iterator<char>(torrent_file)),
                                       std::istreambuf_iterator<char>());
        torrent_file.close();
        
        // 解析 torrent 文件，先获取 torrent_info 以检查文件大小
        lt::error_code ec;
        lt::torrent_info ti(torrent_path, ec);
        if (ec) {
            std::cerr << "错误: 解析 torrent 文件失败: " << ec.message() << std::endl;
            return false;
        }
        
        // 获取 torrent 文件大小
        std::int64_t torrent_size = ti.total_size();
        
        // 验证文件是否存在（快速检查第一个文件）
        namespace fs = std::filesystem;
        bool files_exist = false;
        if (ti.num_files() > 0) {
            std::string first_file_path = ti.files().file_path(lt::file_index_t(0));
            // 构建完整路径：save_path + "/" + file_path_in_torrent
            fs::path save_path_obj(save_path);
            fs::path file_path_obj(first_file_path);
            fs::path full_path = save_path_obj / file_path_obj;
            full_path = full_path.lexically_normal();
            
            // 检查文件是否存在
            files_exist = fs::exists(full_path);
            if (files_exist) {
                std::cout << "验证: 第一个文件存在: " << full_path.string() << std::endl;
            } else {
                std::cout << "警告: 第一个文件不存在: " << full_path.string() << std::endl;
                std::cout << std::endl;
                std::cout << "路径信息:" << std::endl;
                std::cout << "  当前 save_path: " << save_path << std::endl;
                std::cout << "  torrent 中的文件路径: " << first_file_path << std::endl;
                std::cout << "  期望的完整路径: " << full_path.string() << std::endl;
                std::cout << std::endl;
                
                // 根据 torrent 中的文件路径推断正确的 save_path
                // 如果 torrent 中路径是 "Data\Base\Base.ini"，说明原始 save_path 应该是包含 "Data" 的父目录
                fs::path file_in_torrent(first_file_path);
                if (file_in_torrent.has_parent_path() && !file_in_torrent.parent_path().empty()) {
                    // 获取第一个目录名（例如 "Data\Base\Base.ini" -> "Data"）
                    fs::path parent = file_in_torrent.parent_path();
                    std::string first_dir_str = parent.begin()->string();
                    
                    // 提示用户正确的 save_path 应该包含什么
                    std::cout << "提示:" << std::endl;
                    std::cout << "  save_path 应该指向创建 torrent 时使用的根目录（父目录）" << std::endl;
                    std::cout << "  如果 torrent 中文件路径是 \"" << first_file_path << "\"" << std::endl;
                    std::cout << "  那么 save_path 应该是包含 \"" << first_dir_str << "\" 目录的父目录" << std::endl;
                    std::cout << std::endl;
                    std::cout << "  例如：" << std::endl;
                    std::cout << "    如果文件实际在: D:\\some\\path\\" << first_dir_str << "\\..." << std::endl;
                    std::cout << "    那么 save_path 应该是: D:\\some\\path" << std::endl;
                } else {
                    std::cout << "提示:" << std::endl;
                    std::cout << "  save_path 应该指向包含文件 \"" << first_file_path << "\" 的目录" << std::endl;
                }
                std::cout << std::endl;
                std::cout << "  如果文件已移动到其他位置，请使用文件实际所在位置的父目录作为 save_path" << std::endl;
                std::cout << std::endl;
                std::cout << "注意: 如果路径不正确，文件验证可能会失败或需要很长时间" << std::endl;
            }
        }
        
        // 创建 add_torrent_params
        lt::add_torrent_params params;
        params.ti = std::make_shared<lt::torrent_info>(ti);
        params.save_path = save_path;
        
        // 对于大文件（>50GB），如果文件存在，使用 seed_mode 跳过验证以快速启动做种
        // 对于小文件或文件不存在，让 libtorrent 自动验证
        const std::int64_t large_file_threshold = 50LL * 1024 * 1024 * 1024; // 50GB
        
        if (torrent_size > large_file_threshold && files_exist) {
            // 大文件且文件存在：使用 seed_mode 跳过验证，快速启动做种
            std::cout << "检测到大文件（总大小: " 
                      << format_bytes(torrent_size) 
                      << "），文件已存在，使用快速模式启动做种..." << std::endl;
            params.flags |= lt::torrent_flags::seed_mode;
            params.flags |= lt::torrent_flags::auto_managed;
        } else {
            // 小文件或文件不存在：让 libtorrent 自动验证
            if (torrent_size > large_file_threshold) {
                std::cout << "检测到大文件（总大小: " 
                          << format_bytes(torrent_size) 
                          << "），将进行文件验证（可能需要一些时间）..." << std::endl;
            }
            params.flags |= lt::torrent_flags::auto_managed;
        }
        
        // 添加 torrent 到 session
        torrent_handle_ = session_->add_torrent(params, ec);
        
        if (ec) {
            std::cerr << "错误: 添加 torrent 失败: " << ec.message() << std::endl;
            return false;
        }
        
        // 设置优先级为最高（做种模式）
        torrent_handle_.set_upload_mode(false);
        
        is_seeding_ = true;
        
        std::cout << "开始做种..." << std::endl;
        std::cout << "Torrent 文件: " << torrent_path << std::endl;
        std::cout << "保存路径: " << save_path << std::endl;
        std::cout << "Torrent 大小: " << format_bytes(torrent_size) << std::endl;
        std::cout << std::endl;
        
        // 等待 torrent 状态更新
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "开始做种时出错: " << e.what() << std::endl;
        is_seeding_ = false;
        return false;
    }
}

void Seeder::stop_seeding()
{
    if (!is_seeding_ || !session_) {
        return;
    }
    
    try {
        if (torrent_handle_.is_valid()) {
            // 从 session 中移除 torrent
            session_->remove_torrent(torrent_handle_, lt::session::delete_files);
            torrent_handle_ = lt::torrent_handle();
        }
        
        is_seeding_ = false;
        std::cout << "已停止做种" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "停止做种时出错: " << e.what() << std::endl;
    }
}

bool Seeder::is_seeding() const
{
    return is_seeding_ && torrent_handle_.is_valid();
}

void Seeder::print_status() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid()) {
        std::cout << "当前未在做种" << std::endl;
        return;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        
        std::cout << "=== 做种状态 ===" << std::endl;
        std::cout << "状态: ";
        
        if (status.state == lt::torrent_status::seeding) {
            std::cout << "做种中 (Seeding)" << std::endl;
        } else if (status.state == lt::torrent_status::finished) {
            std::cout << "已完成 (Finished)" << std::endl;
        } else if (status.state == lt::torrent_status::downloading) {
            std::cout << "下载中 (Downloading)" << std::endl;
        } else if (status.state == lt::torrent_status::checking_files) {
            std::cout << "检查文件中 (Checking Files)" << std::endl;
        } else {
            std::cout << "其他状态 (" << static_cast<int>(status.state) << ")" << std::endl;
        }
        
        // 计算进度（用于文件验证和下载）
        double progress = 0.0;
        if (status.total_wanted > 0) {
            progress = static_cast<double>(status.total_wanted_done) / 
                      static_cast<double>(status.total_wanted);
        }
        
        std::cout << "进度: " << (progress * 100.0) << "%" << std::endl;
        std::cout << "已下载/需要: " << format_bytes(status.total_wanted_done) 
                  << " / " << format_bytes(status.total_wanted) << std::endl;
        std::cout << "连接的对等节点数: " << status.num_peers << std::endl;
        std::cout << "已上传: " << format_bytes(status.total_upload) << std::endl;
        std::cout << "已下载: " << format_bytes(status.total_download) << std::endl;
        std::cout << "上传速度: " << format_speed(status.upload_rate) << std::endl;
        std::cout << "下载速度: " << format_speed(status.download_rate) << std::endl;
        
        // 显示 tracker 状态
        std::vector<lt::announce_entry> trackers = torrent_handle_.trackers();
        if (!trackers.empty()) {
            std::cout << "Tracker 状态:" << std::endl;
            for (const auto& tracker : trackers) {
                std::cout << "  - " << tracker.url;
                if (tracker.is_working()) {
                    std::cout << " [工作正常]";
                } else {
                    std::cout << " [未连接]";
                }
                std::cout << std::endl;
            }
        }
        
        std::cout << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "获取状态时出错: " << e.what() << std::endl;
    }
}

bool Seeder::wait_and_process(int timeout_ms)
{
    if (!session_) {
        return false;
    }
    
    try {
        // 处理 alerts
        std::vector<lt::alert*> alerts;
        session_->pop_alerts(&alerts);
        
        for (lt::alert* alert : alerts) {
            if (lt::alert_cast<lt::tracker_announce_alert>(alert)) {
                // 处理 tracker 相关警报
                auto* ta = lt::alert_cast<lt::tracker_announce_alert>(alert);
                if (ta) {
                    // 可以在这里记录 tracker 公告信息（可选）
                    // std::cout << "Tracker 公告: " << ta->tracker_url() << std::endl;
                }
            } else if (lt::alert_cast<lt::torrent_error_alert>(alert)) {
                // 处理错误警报
                auto* tea = lt::alert_cast<lt::torrent_error_alert>(alert);
                if (tea) {
                    std::cerr << "Torrent 错误: " << tea->error.message() << std::endl;
                    std::cerr << "  错误类型: " << tea->error.category().name() << std::endl;
                }
            } else if (lt::alert_cast<lt::file_error_alert>(alert)) {
                // 处理文件错误
                auto* fea = lt::alert_cast<lt::file_error_alert>(alert);
                if (fea) {
                    std::cerr << "文件错误: " << fea->error.message() << std::endl;
                    std::cerr << "  文件路径: " << fea->filename() << std::endl;
                }
            } else if (lt::alert_cast<lt::torrent_finished_alert>(alert)) {
                // 文件验证/下载完成
                std::cout << std::endl;
                std::cout << "=== 文件验证完成，进入做种状态 ===" << std::endl;
                std::cout << std::endl;
            } else if (lt::alert_cast<lt::state_changed_alert>(alert)) {
                // 状态改变
                auto* sca = lt::alert_cast<lt::state_changed_alert>(alert);
                if (sca) {
                    const char* state_name = "未知状态";
                    switch (sca->state) {
                        case lt::torrent_status::checking_files:
                            state_name = "检查文件中";
                            break;
                        case lt::torrent_status::downloading_metadata:
                            state_name = "下载元数据";
                            break;
                        case lt::torrent_status::downloading:
                            state_name = "下载中";
                            break;
                        case lt::torrent_status::finished:
                            state_name = "已完成";
                            break;
                        case lt::torrent_status::seeding:
                            state_name = "做种中";
                            break;
                        case lt::torrent_status::allocating:
                            state_name = "分配空间中";
                            break;
                        default:
                            state_name = "其他状态";
                            break;
                    }
                    std::cout << "状态改变: " << state_name << " (状态值: " << sca->state << ")" << std::endl;
                }
            }
        }
        
        // 等待指定时间
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        
        return is_seeding_;
    } catch (const std::exception& e) {
        std::cerr << "处理事件时出错: " << e.what() << std::endl;
        return false;
    }
}

int Seeder::get_peer_count() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.num_peers;
    } catch (const std::exception&) {
        return 0;
    }
}

std::int64_t Seeder::get_uploaded_bytes() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_upload;
    } catch (const std::exception&) {
        return 0;
    }
}

std::int64_t Seeder::get_downloaded_bytes() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_download;
    } catch (const std::exception&) {
        return 0;
    }
}