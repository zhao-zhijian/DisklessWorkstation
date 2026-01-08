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
        
        // 设置最大连接数
        settings.set_int(lt::settings_pack::connections_limit, 200);
        
        // 创建 session
        session_ = std::make_unique<lt::session>(settings);
        
        std::cout << "Downloader 会话已初始化" << std::endl;
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
        
        // 解析 torrent 文件
        lt::error_code ec;
        lt::add_torrent_params params = lt::load_torrent_file(torrent_path);
        
        // 设置保存路径
        params.save_path = save_path;
        
        // 设置下载模式（自动管理）
        params.flags |= lt::torrent_flags::auto_managed;
        
        // 添加 torrent 到 session
        torrent_handle_ = session_->add_torrent(params, ec);
        
        if (ec) {
            std::cerr << "错误: 添加 torrent 失败: " << ec.message() << std::endl;
            return false;
        }
        
        // 设置下载优先级
        torrent_handle_.set_upload_mode(false);
        
        is_downloading_ = true;
        
        std::cout << "开始下载..." << std::endl;
        std::cout << "Torrent 文件: " << torrent_path << std::endl;
        std::cout << "保存路径: " << save_path << std::endl;
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

