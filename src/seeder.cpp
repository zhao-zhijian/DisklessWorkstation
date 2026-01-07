#include "seeder.hpp"
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
    
    while (size >= 1024.0 && unit_index < 4)
    {
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
    if (rate_bytes_per_sec == 0)
    {
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
    try
    {
        // 创建 session 配置
        lt::settings_pack settings;
        settings.set_int(lt::settings_pack::alert_mask, 
                         lt::alert::status_notification | 
                         lt::alert::error_notification |
                         lt::alert::peer_notification);
        
        // 设置监听接口（空字符串表示自动选择）
        settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:0");
        
        // 启用 DHT
        settings.set_bool(lt::settings_pack::enable_dht, true);
        
        // 启用本地服务发现
        settings.set_bool(lt::settings_pack::enable_lsd, true);
        
        // 启用 UPnP 和 NAT-PMP
        settings.set_bool(lt::settings_pack::enable_upnp, true);
        settings.set_bool(lt::settings_pack::enable_natpmp, true);
        
        // 创建 session
        session_ = std::make_unique<lt::session>(settings);
        
        std::cout << "Seeder 会话已初始化" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "初始化 Seeder 会话失败: " << e.what() << std::endl;
        throw;
    }
}

bool Seeder::validate_paths(const std::string& torrent_path, const std::string& save_path)
{
    namespace fs = std::filesystem;
    
    // 验证 torrent 文件是否存在
    if (!fs::exists(torrent_path))
    {
        std::cerr << "错误: Torrent 文件不存在: " << torrent_path << std::endl;
        return false;
    }
    
    // 验证保存路径是否存在
    if (!fs::exists(save_path))
    {
        std::cerr << "错误: 保存路径不存在: " << save_path << std::endl;
        std::cerr << "提示: 保存路径必须指向创建 torrent 时的原始文件或目录" << std::endl;
        return false;
    }
    
    return true;
}

bool Seeder::start_seeding(const std::string& torrent_path, const std::string& save_path)
{
    try
    {
        // 如果已经在做种，先停止
        if (is_seeding_)
        {
            stop_seeding();
        }
        
        // 验证路径
        if (!validate_paths(torrent_path, save_path))
        {
            return false;
        }
        
        // 加载 torrent 文件
        std::ifstream torrent_file(torrent_path, std::ios::binary);
        if (!torrent_file.is_open())
        {
            std::cerr << "错误: 无法打开 torrent 文件: " << torrent_path << std::endl;
            return false;
        }
        
        // 读取 torrent 文件内容
        std::vector<char> torrent_data((std::istreambuf_iterator<char>(torrent_file)),
                                       std::istreambuf_iterator<char>());
        torrent_file.close();
        
        // 解析 torrent 文件
        lt::error_code ec;
        lt::add_torrent_params params = lt::load_torrent_file(torrent_path);
        
        // 设置保存路径（必须指向原始文件/目录）
        params.save_path = save_path;
        
        // 设置做种模式（不下载，只上传）
        params.flags |= lt::torrent_flags::seed_mode;
        params.flags |= lt::torrent_flags::auto_managed;
        
        // 添加 torrent 到 session
        torrent_handle_ = session_->add_torrent(params, ec);
        
        if (ec)
        {
            std::cerr << "错误: 添加 torrent 失败: " << ec.message() << std::endl;
            return false;
        }
        
        // 设置优先级为最高（做种模式）
        torrent_handle_.set_upload_mode(false);
        
        is_seeding_ = true;
        
        std::cout << "开始做种..." << std::endl;
        std::cout << "Torrent 文件: " << torrent_path << std::endl;
        std::cout << "保存路径: " << save_path << std::endl;
        std::cout << std::endl;
        
        // 等待 torrent 状态更新
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "开始做种时出错: " << e.what() << std::endl;
        is_seeding_ = false;
        return false;
    }
}

void Seeder::stop_seeding()
{
    if (!is_seeding_ || !session_)
    {
        return;
    }
    
    try
    {
        if (torrent_handle_.is_valid())
        {
            // 从 session 中移除 torrent
            session_->remove_torrent(torrent_handle_, lt::session::delete_files);
            torrent_handle_ = lt::torrent_handle();
        }
        
        is_seeding_ = false;
        std::cout << "已停止做种" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "停止做种时出错: " << e.what() << std::endl;
    }
}

bool Seeder::is_seeding() const
{
    return is_seeding_ && torrent_handle_.is_valid();
}

void Seeder::print_status() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid())
    {
        std::cout << "当前未在做种" << std::endl;
        return;
    }
    
    try
    {
        lt::torrent_status status = torrent_handle_.status();
        
        std::cout << "=== 做种状态 ===" << std::endl;
        std::cout << "状态: ";
        
        if (status.state == lt::torrent_status::seeding)
        {
            std::cout << "做种中 (Seeding)" << std::endl;
        }
        else if (status.state == lt::torrent_status::finished)
        {
            std::cout << "已完成 (Finished)" << std::endl;
        }
        else if (status.state == lt::torrent_status::downloading)
        {
            std::cout << "下载中 (Downloading)" << std::endl;
        }
        else if (status.state == lt::torrent_status::checking_files)
        {
            std::cout << "检查文件中 (Checking Files)" << std::endl;
        }
        else
        {
            std::cout << "其他状态 (" << static_cast<int>(status.state) << ")" << std::endl;
        }
        
        std::cout << "连接的对等节点数: " << status.num_peers << std::endl;
        std::cout << "已上传: " << format_bytes(status.total_upload) << std::endl;
        std::cout << "已下载: " << format_bytes(status.total_download) << std::endl;
        std::cout << "上传速度: " << format_speed(status.upload_rate) << std::endl;
        std::cout << "下载速度: " << format_speed(status.download_rate) << std::endl;
        
        // 显示 tracker 状态
        std::vector<lt::announce_entry> trackers = torrent_handle_.trackers();
        if (!trackers.empty())
        {
            std::cout << "Tracker 状态:" << std::endl;
            for (const auto& tracker : trackers)
            {
                std::cout << "  - " << tracker.url;
                if (tracker.is_working())
                {
                    std::cout << " [工作正常]";
                }
                else
                {
                    std::cout << " [未连接]";
                }
                std::cout << std::endl;
            }
        }
        
        std::cout << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "获取状态时出错: " << e.what() << std::endl;
    }
}

bool Seeder::wait_and_process(int timeout_ms)
{
    if (!session_)
    {
        return false;
    }
    
    try
    {
        // 处理 alerts
        std::vector<lt::alert*> alerts;
        session_->pop_alerts(&alerts);
        
        for (lt::alert* alert : alerts)
        {
            // 处理 tracker 相关警报
            if (lt::alert_cast<lt::tracker_announce_alert>(alert))
            {
                auto* ta = lt::alert_cast<lt::tracker_announce_alert>(alert);
                if (ta)
                {
                    std::cout << "Tracker 公告: " << ta->tracker_url() << std::endl;
                }
            }
            // 处理错误警报
            else if (lt::alert_cast<lt::torrent_error_alert>(alert))
            {
                auto* tea = lt::alert_cast<lt::torrent_error_alert>(alert);
                if (tea)
                {
                    std::cerr << "Torrent 错误: " << tea->error.message() << std::endl;
                }
            }
        }
        
        // 等待指定时间
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        
        return is_seeding_;
    }
    catch (const std::exception& e)
    {
        std::cerr << "处理事件时出错: " << e.what() << std::endl;
        return false;
    }
}

int Seeder::get_peer_count() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid())
    {
        return 0;
    }
    
    try
    {
        lt::torrent_status status = torrent_handle_.status();
        return status.num_peers;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

std::int64_t Seeder::get_uploaded_bytes() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid())
    {
        return 0;
    }
    
    try
    {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_upload;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

std::int64_t Seeder::get_downloaded_bytes() const
{
    if (!is_seeding_ || !torrent_handle_.is_valid())
    {
        return 0;
    }
    
    try
    {
        lt::torrent_status status = torrent_handle_.status();
        return status.total_download;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

