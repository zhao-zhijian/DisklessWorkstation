#include "downloader.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/alert_types.hpp>
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

// 辅助函数：格式化百分比
static std::string format_percent(double progress)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f%%", progress * 100.0);
    return std::string(buffer);
}

Downloader::Downloader()
    : session_(nullptr)
    , is_downloading_(false)
{
    configure_session();
}

Downloader::~Downloader()
{
    stop_download();
}

void Downloader::configure_session()
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
        
        // 设置下载限制（0 表示无限制）
        settings.set_int(lt::settings_pack::download_rate_limit, 0);
        settings.set_int(lt::settings_pack::upload_rate_limit, 0);
        
        // 设置最大连接数（大文件需要更多连接）
        settings.set_int(lt::settings_pack::connections_limit, 200);
        
        // 设置磁盘缓存大小（大文件需要更大的缓存）
        // 默认是32MB，对于大文件增加到512MB以提高性能
        settings.set_int(lt::settings_pack::cache_size, 512);
        
        // 设置磁盘缓存过期时间（毫秒），大文件需要更长的缓存时间
        settings.set_int(lt::settings_pack::cache_expiry, 300);
        
        // 设置磁盘写入队列大小（大文件需要更大的队列）
        settings.set_int(lt::settings_pack::max_queued_disk_bytes, 1024 * 1024 * 1024); // 1GB
        
        // 注意：max_connections_per_torrent 不是 settings_pack 的成员
        // 每个 torrent 的最大连接数需要通过 torrent_handle.set_max_connections() 设置
        // 这将在 start_download() 中完成
        
        // 创建 session
        session_ = std::make_unique<lt::session>(settings);
        
        std::cout << "Downloader 会话已初始化（已优化大文件下载配置）" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "初始化 Downloader 会话失败: " << e.what() << std::endl;
        throw;
    }
}

bool Downloader::validate_paths(const std::string& torrent_path, const std::string& save_path)
{
    namespace fs = std::filesystem;
    
    // 验证 torrent 文件是否存在
    if (!fs::exists(torrent_path)) {
        std::cerr << "错误: Torrent 文件不存在: " << torrent_path << std::endl;
        return false;
    }
    
    // 验证保存路径是否存在，如果不存在则创建
    if (!fs::exists(save_path)) {
        try {
            fs::create_directories(save_path);
            std::cout << "已创建保存目录: " << save_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "错误: 无法创建保存目录: " << save_path << std::endl;
            std::cerr << "原因: " << e.what() << std::endl;
            return false;
        }
    } else if (!fs::is_directory(save_path)) {
        std::cerr << "错误: 保存路径不是目录: " << save_path << std::endl;
        return false;
    }
    
    return true;
}

bool Downloader::start_download(const std::string& torrent_path, const std::string& save_path)
{
    try {
        // 如果已经在下载，先停止
        if (is_downloading_) {
            stop_download();
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
        
        // 解析 torrent 文件，先获取 torrent_info 以检查文件大小
        lt::error_code ec;
        lt::torrent_info ti(torrent_path, ec);
        if (ec) {
            std::cerr << "错误: 解析 torrent 文件失败: " << ec.message() << std::endl;
            return false;
        }
        
        // 获取 torrent 文件大小
        std::int64_t torrent_size = ti.total_size();
        
        // 创建 add_torrent_params
        lt::add_torrent_params params;
        params.ti = std::make_shared<lt::torrent_info>(ti);
        params.save_path = save_path;
        
        // 针对大文件的优化设置
        const std::int64_t large_file_threshold = 50LL * 1024 * 1024 * 1024; // 50GB
        
        if (torrent_size > large_file_threshold) {
            std::cout << "检测到大文件（总大小: " 
                      << format_bytes(torrent_size) 
                      << "），应用大文件下载优化..." << std::endl;
            
            // 对于大文件，不使用 auto_managed，手动控制下载
            // 这样可以避免在检查文件时被自动暂停
            // 注意：不使用 auto_managed 意味着需要手动管理下载状态
            params.flags &= ~lt::torrent_flags::auto_managed;
            params.flags &= ~lt::torrent_flags::paused;  // 确保不是暂停状态
            
            std::cout << "使用手动下载模式（跳过自动管理）..." << std::endl;
        } else {
            // 小文件使用标准设置（自动管理）
            params.flags |= lt::torrent_flags::auto_managed;
        }
        
        // 添加 torrent 到 session
        torrent_handle_ = session_->add_torrent(params, ec);
        
        if (ec) {
            std::cerr << "错误: 添加 torrent 失败: " << ec.message() << std::endl;
            return false;
        }
        
        // 设置下载优先级（非上传模式）
        torrent_handle_.set_upload_mode(false);
        
        // 对于大文件，设置更高的下载优先级和更多连接
        if (torrent_size > large_file_threshold) {
            // 设置每个 torrent 的最大连接数（大文件需要更多连接）
            torrent_handle_.set_max_connections(200);
            
            // 设置所有文件为最高优先级
            std::vector<int> priorities;
            for (int i = 0; i < ti.num_files(); ++i) {
                priorities.push_back(7); // 最高优先级
            }
            torrent_handle_.prioritize_files(priorities);
            
            // 强制开始下载（大文件需要手动控制）
            // resume() 会自动取消暂停状态
            torrent_handle_.resume();
            
            std::cout << "已强制开始下载..." << std::endl;
        } else {
            // 小文件也设置合理的连接数
            torrent_handle_.set_max_connections(50);
        }
        
        // 确保下载已开始（无论文件大小）
        // 再次调用 resume 确保下载已启动
        torrent_handle_.resume();
        
        is_downloading_ = true;
        
        std::cout << "开始下载..." << std::endl;
        std::cout << "Torrent 文件: " << torrent_path << std::endl;
        std::cout << "保存路径: " << save_path << std::endl;
        std::cout << "文件大小: " << format_bytes(torrent_size) << std::endl;
        std::cout << std::endl;
        
        // 等待 torrent 状态更新
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "开始下载时出错: " << e.what() << std::endl;
        is_downloading_ = false;
        return false;
    }
}

void Downloader::stop_download()
{
    if (!is_downloading_ || !session_) {
        return;
    }
    
    try {
        if (torrent_handle_.is_valid()) {
            // 从 session 中移除 torrent（不删除文件，只删除部分文件）
            session_->remove_torrent(torrent_handle_, lt::session::delete_partfile);
            torrent_handle_ = lt::torrent_handle();
        }
        
        is_downloading_ = false;
        std::cout << "已停止下载" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "停止下载时出错: " << e.what() << std::endl;
    }
}

void Downloader::pause()
{
    if (torrent_handle_.is_valid()) {
        torrent_handle_.pause();
        std::cout << "下载已暂停" << std::endl;
    }
}

void Downloader::resume()
{
    if (torrent_handle_.is_valid()) {
        torrent_handle_.resume();
        std::cout << "下载已恢复" << std::endl;
    }
}

bool Downloader::is_downloading() const
{
    return is_downloading_ && torrent_handle_.is_valid();
}

bool Downloader::is_finished() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return false;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.state == lt::torrent_status::seeding || 
               status.state == lt::torrent_status::finished;
    } catch (const std::exception&) {
        return false;
    }
}

bool Downloader::is_paused() const
{
    if (!torrent_handle_.is_valid()) {
        return false;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.flags & lt::torrent_flags::paused;
    } catch (const std::exception&) {
        return false;
    }
}

void Downloader::print_status() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        std::cout << "当前未在下载" << std::endl;
        return;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        
        std::cout << "=== 下载状态 ===" << std::endl;
        std::cout << "状态: ";
        
        if (status.state == lt::torrent_status::seeding) {
            std::cout << "已完成 (Seeding)" << std::endl;
        } else if (status.state == lt::torrent_status::finished) {
            std::cout << "已完成 (Finished)" << std::endl;
        } else if (status.state == lt::torrent_status::downloading) {
            std::cout << "下载中 (Downloading)" << std::endl;
        } else if (status.state == lt::torrent_status::checking_files) {
            std::cout << "检查文件中 (Checking Files)" << std::endl;
        } else if (status.state == lt::torrent_status::checking_resume_data) {
            std::cout << "检查恢复数据中 (Checking Resume Data)" << std::endl;
        } else {
            std::cout << "其他状态 (" << static_cast<int>(status.state) << ")" << std::endl;
        }
        
        // 计算进度
        double progress = 0.0;
        if (status.total_wanted > 0) {
            progress = static_cast<double>(status.total_wanted_done) / 
                      static_cast<double>(status.total_wanted);
        }
        
        std::cout << "进度: " << format_percent(progress) << std::endl;
        std::cout << "已下载: " << format_bytes(status.total_wanted_done) 
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

bool Downloader::wait_and_process(int timeout_ms)
{
    if (!session_) {
        return false;
    }
    
    try {
        // 处理 alerts
        std::vector<lt::alert*> alerts;
        session_->pop_alerts(&alerts);
        
        for (lt::alert* alert : alerts) {
            if (lt::alert_cast<lt::torrent_finished_alert>(alert)) {
                // 下载完成
                std::cout << std::endl;
                std::cout << "=== 下载完成！===" << std::endl;
                std::cout << std::endl;
            } else if (lt::alert_cast<lt::tracker_announce_alert>(alert)) {
                // 处理 tracker 相关警报
                auto* ta = lt::alert_cast<lt::tracker_announce_alert>(alert);
                if (ta) {
                    // 可以在这里记录 tracker 公告信息
                }
            } else if (lt::alert_cast<lt::torrent_error_alert>(alert)) {
                // 处理错误警报
                auto* tea = lt::alert_cast<lt::torrent_error_alert>(alert);
                if (tea) {
                    std::cerr << "Torrent 错误: " << tea->error.message() << std::endl;
                }
            } else if (lt::alert_cast<lt::file_error_alert>(alert)) {
                // 处理文件错误
                auto* fea = lt::alert_cast<lt::file_error_alert>(alert);
                if (fea) {
                    std::cerr << "文件错误: " << fea->error.message() << std::endl;
                }
            } else if (lt::alert_cast<lt::state_changed_alert>(alert)) {
                // 处理状态变化警报
                auto* sca = lt::alert_cast<lt::state_changed_alert>(alert);
                if (sca && torrent_handle_.is_valid()) {
                    // 如果从 checking_files 状态转换到其他状态，确保下载已开始
                    if (sca->state == lt::torrent_status::downloading || 
                        sca->state == lt::torrent_status::finished) {
                        // 确保下载没有被暂停（resume 会自动取消暂停）
                        torrent_handle_.resume();
                    }
                }
            }
        }
        
        // 对于大文件，定期检查并确保下载没有被暂停
        if (torrent_handle_.is_valid()) {
            try {
                lt::torrent_status status = torrent_handle_.status();
                
                // 如果处于 checking_files 状态，等待检查完成
                // 如果检查完成但处于暂停状态，强制恢复
                if (status.state != lt::torrent_status::checking_files &&
                    status.state != lt::torrent_status::checking_resume_data &&
                    (status.flags & lt::torrent_flags::paused)) {
                    // 如果下载被暂停了，强制恢复（resume 会自动取消暂停状态）
                    torrent_handle_.resume();
                }
            } catch (...) {
                // 忽略状态检查错误
            }
        }
        
        // 等待指定时间
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        
        return is_downloading_;
    } catch (const std::exception& e) {
        std::cerr << "处理事件时出错: " << e.what() << std::endl;
        return false;
    }
}

int Downloader::get_peer_count() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.num_peers;
    } catch (const std::exception&) {
        return 0;
    }
}

std::int64_t Downloader::get_downloaded_bytes() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_wanted_done;
    } catch (const std::exception&) {
        return 0;
    }
}

std::int64_t Downloader::get_uploaded_bytes() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_upload;
    } catch (const std::exception&) {
        return 0;
    }
}

int Downloader::get_download_rate() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.download_rate;
    } catch (const std::exception&) {
        return 0;
    }
}

int Downloader::get_upload_rate() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.upload_rate;
    } catch (const std::exception&) {
        return 0;
    }
}

double Downloader::get_progress() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0.0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        if (status.total_wanted > 0) {
            return static_cast<double>(status.total_wanted_done) / 
                   static_cast<double>(status.total_wanted);
        }
        return 0.0;
    } catch (const std::exception&) {
        return 0.0;
    }
}

std::int64_t Downloader::get_total_size() const
{
    if (!is_downloading_ || !torrent_handle_.is_valid()) {
        return 0;
    }
    
    try {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_wanted;
    } catch (const std::exception&) {
        return 0;
    }
}

