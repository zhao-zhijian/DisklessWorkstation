#include "torrent_manager.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <algorithm>
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

// 单例实例获取
TorrentManager& TorrentManager::getInstance()
{
    static TorrentManager instance;
    return instance;
}

// 私有构造函数
TorrentManager::TorrentManager()
    : session_(nullptr)
{
    configure_session();
}

// 析构函数
TorrentManager::~TorrentManager()
{
    stop_all();
}

// 初始化 session 设置
void TorrentManager::configure_session()
{
    try {
        // 创建 session 配置（合并 Downloader 和 Seeder 的最佳配置）
        lt::settings_pack settings;
        settings.set_int(lt::settings_pack::alert_mask, 
                         lt::alert::status_notification | 
                         lt::alert::error_notification |
                         lt::alert::peer_notification |
                         lt::alert::storage_notification);
        
        // 设置监听接口（空字符串表示自动选择）
        settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:0");
        
        // 启用 DHT（下载需要，做种可选）
        settings.set_bool(lt::settings_pack::enable_dht, true);
        
        // 启用本地服务发现
        settings.set_bool(lt::settings_pack::enable_lsd, true);
        
        // 启用 UPnP 和 NAT-PMP
        settings.set_bool(lt::settings_pack::enable_upnp, true);
        settings.set_bool(lt::settings_pack::enable_natpmp, true);
        
        // 设置上传/下载速度限制（0 表示无限制）
        settings.set_int(lt::settings_pack::download_rate_limit, 0);
        settings.set_int(lt::settings_pack::upload_rate_limit, 0);
        
        // 设置最大连接数（支持并发下载和做种）
        settings.set_int(lt::settings_pack::connections_limit, 200);
        
        // 设置磁盘缓存大小（大文件需要更大的缓存）
        settings.set_int(lt::settings_pack::cache_size, 512);
        
        // 设置磁盘缓存过期时间（毫秒）
        settings.set_int(lt::settings_pack::cache_expiry, 300);
        
        // 设置磁盘写入队列大小（大文件需要更大的队列）
        settings.set_int(lt::settings_pack::max_queued_disk_bytes, 1024 * 1024 * 1024); // 1GB
        
        // 创建 session
        session_ = std::make_unique<lt::session>(settings);
        
        std::cout << "TorrentManager 会话已初始化（支持并发下载和做种）" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "初始化 TorrentManager 会话失败: " << e.what() << std::endl;
        throw;
    }
}

// 验证路径
bool TorrentManager::validate_paths(const std::string& torrent_path, const std::string& save_path, bool create_save_path)
{
    namespace fs = std::filesystem;
    
    // 验证 torrent 文件是否存在
    if (!fs::exists(torrent_path)) {
        std::cerr << "错误: Torrent 文件不存在: " << torrent_path << std::endl;
        return false;
    }
    
    // 验证保存路径
    if (!fs::exists(save_path)) {
        if (create_save_path) {
            // 下载时：如果路径不存在则创建
            try {
                fs::create_directories(save_path);
                std::cout << "已创建保存目录: " << save_path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "错误: 无法创建保存目录: " << save_path << std::endl;
                std::cerr << "原因: " << e.what() << std::endl;
                return false;
            }
        } else {
            // 做种时：路径必须存在
            std::cerr << "错误: 保存路径不存在: " << save_path << std::endl;
            std::cerr << "提示: 保存路径必须指向创建 torrent 时的原始文件或目录" << std::endl;
            return false;
        }
    } else if (!fs::is_directory(save_path)) {
        std::cerr << "错误: 保存路径不是目录: " << save_path << std::endl;
        return false;
    }
    
    return true;
}

// 从 torrent_info 获取 info_hash 字符串
std::string TorrentManager::get_info_hash_string(const lt::torrent_info& ti) const
{
    lt::sha1_hash hash = ti.info_hash();
    // 将 sha1_hash 转换为十六进制字符串
    std::ostringstream oss;
    oss << hash;
    return oss.str();
}

// 开始下载
std::string TorrentManager::start_download(const std::string& torrent_path, const std::string& save_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 验证路径
        if (!validate_paths(torrent_path, save_path, true)) {
            return "";
        }
        
        // 加载 torrent 文件
        std::ifstream torrent_file(torrent_path, std::ios::binary);
        if (!torrent_file.is_open()) {
            std::cerr << "错误: 无法打开 torrent 文件: " << torrent_path << std::endl;
            return "";
        }
        
        // 解析 torrent 文件
        lt::error_code ec;
        lt::torrent_info ti(torrent_path, ec);
        if (ec) {
            std::cerr << "错误: 解析 torrent 文件失败: " << ec.message() << std::endl;
            return "";
        }
        
        // 获取 info_hash
        std::string info_hash = get_info_hash_string(ti);
        
        // 检查是否已存在
        if (torrents_.find(info_hash) != torrents_.end()) {
            std::cerr << "错误: 该 torrent 已存在（info_hash: " << info_hash << "）" << std::endl;
            return "";
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
            
            params.flags &= ~lt::torrent_flags::auto_managed;
            params.flags &= ~lt::torrent_flags::paused;
            
            std::cout << "使用手动下载模式（跳过自动管理）..." << std::endl;
        } else {
            params.flags |= lt::torrent_flags::auto_managed;
        }
        
        // 添加 torrent 到 session
        lt::torrent_handle th = session_->add_torrent(params, ec);
        
        if (ec) {
            std::cerr << "错误: 添加 torrent 失败: " << ec.message() << std::endl;
            return "";
        }
        
        // 设置下载优先级（非上传模式）
        th.set_upload_mode(false);
        
        // 对于大文件，设置更高的下载优先级和更多连接
        if (torrent_size > large_file_threshold) {
            th.set_max_connections(200);
            
            std::vector<int> priorities;
            for (int i = 0; i < ti.num_files(); ++i) {
                priorities.push_back(7); // 最高优先级
            }
            th.prioritize_files(priorities);
            
            th.resume();
            
            std::cout << "已强制开始下载..." << std::endl;
        } else {
            th.set_max_connections(50);
        }
        
        // 确保下载已开始
        th.resume();
        
        // 保存 torrent 信息
        TorrentInfo info;
        info.handle = th;
        info.type = TorrentType::Download;
        info.torrent_path = torrent_path;
        info.save_path = save_path;
        info.info_hash = info_hash;
        info.is_valid = true;
        
        torrents_[info_hash] = info;
        
        std::cout << "开始下载 [info_hash: " << info_hash.substr(0, 8) << "...]" << std::endl;
        std::cout << "Torrent 文件: " << torrent_path << std::endl;
        std::cout << "保存路径: " << save_path << std::endl;
        std::cout << "文件大小: " << format_bytes(torrent_size) << std::endl;
        // 此处已持有 mutex_，使用无锁版本避免死锁
        std::cout << "当前下载任务数: " << get_download_count_unsafe() << std::endl;
        std::cout << std::endl;
        
        // 等待 torrent 状态更新
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return info_hash;
    } catch (const std::exception& e) {
        std::cerr << "开始下载时出错: " << e.what() << std::endl;
        return "";
    }
}

// 开始做种
std::string TorrentManager::start_seeding(const std::string& torrent_path, const std::string& save_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 验证路径
        if (!validate_paths(torrent_path, save_path, false)) {
            return "";
        }
        
        // 加载 torrent 文件
        std::ifstream torrent_file(torrent_path, std::ios::binary);
        if (!torrent_file.is_open()) {
            std::cerr << "错误: 无法打开 torrent 文件: " << torrent_path << std::endl;
            return "";
        }
        
        // 解析 torrent 文件
        lt::error_code ec;
        lt::torrent_info ti(torrent_path, ec);
        if (ec) {
            std::cerr << "错误: 解析 torrent 文件失败: " << ec.message() << std::endl;
            return "";
        }
        
        // 获取 info_hash
        std::string info_hash = get_info_hash_string(ti);
        
        // 检查是否已存在
        if (torrents_.find(info_hash) != torrents_.end()) {
            std::cerr << "错误: 该 torrent 已存在（info_hash: " << info_hash << "）" << std::endl;
            return "";
        }
        
        // 获取 torrent 文件大小
        std::int64_t torrent_size = ti.total_size();
        
        // 验证文件是否存在（快速检查第一个文件）
        namespace fs = std::filesystem;
        bool files_exist = false;
        if (ti.num_files() > 0) {
            std::string first_file_path = ti.files().file_path(lt::file_index_t(0));
            fs::path save_path_obj(save_path);
            fs::path file_path_obj(first_file_path);
            fs::path full_path = save_path_obj / file_path_obj;
            full_path = full_path.lexically_normal();
            
            files_exist = fs::exists(full_path);
            if (files_exist) {
                std::cout << "验证: 第一个文件存在: " << full_path.string() << std::endl;
            } else {
                std::cout << "警告: 第一个文件不存在: " << full_path.string() << std::endl;
            }
        }
        
        // 创建 add_torrent_params
        lt::add_torrent_params params;
        params.ti = std::make_shared<lt::torrent_info>(ti);
        params.save_path = save_path;
        
        // 对于大文件（>50GB），如果文件存在，使用 seed_mode 跳过验证以快速启动做种
        const std::int64_t large_file_threshold = 50LL * 1024 * 1024 * 1024; // 50GB
        
        if (torrent_size > large_file_threshold && files_exist) {
            std::cout << "检测到大文件（总大小: " 
                      << format_bytes(torrent_size) 
                      << "），文件已存在，使用快速模式启动做种..." << std::endl;
            params.flags |= lt::torrent_flags::seed_mode;
            params.flags |= lt::torrent_flags::auto_managed;
        } else {
            if (torrent_size > large_file_threshold) {
                std::cout << "检测到大文件（总大小: " 
                          << format_bytes(torrent_size) 
                          << "），将进行文件验证（可能需要一些时间）..." << std::endl;
            }
            params.flags |= lt::torrent_flags::auto_managed;
        }
        
        // 确保做种时不被暂停
        params.flags &= ~lt::torrent_flags::paused;
        
        // 添加 torrent 到 session
        lt::torrent_handle th = session_->add_torrent(params, ec);
        
        if (ec) {
            std::cerr << "错误: 添加 torrent 失败: " << ec.message() << std::endl;
            return "";
        }
        
        // 保存 torrent 信息
        TorrentInfo info;
        info.handle = th;
        info.type = TorrentType::Seeding;
        info.torrent_path = torrent_path;
        info.save_path = save_path;
        info.info_hash = info_hash;
        info.is_valid = true;
        
        torrents_[info_hash] = info;
        
        std::cout << "开始做种 [info_hash: " << info_hash.substr(0, 8) << "...]" << std::endl;
        std::cout << "Torrent 文件: " << torrent_path << std::endl;
        std::cout << "保存路径: " << save_path << std::endl;
        std::cout << "Torrent 大小: " << format_bytes(torrent_size) << std::endl;
        // 此处已持有 mutex_，使用无锁版本避免死锁
        std::cout << "当前做种任务数: " << get_seeding_count_unsafe() << std::endl;
        std::cout << std::endl;
        
        // 等待 torrent 状态更新
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return info_hash;
    } catch (const std::exception& e) {
        std::cerr << "开始做种时出错: " << e.what() << std::endl;
        return "";
    }
}

// 停止指定的 torrent
bool TorrentManager::stop_torrent(const std::string& info_hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = torrents_.find(info_hash);
    if (it == torrents_.end()) {
        std::cerr << "错误: 未找到指定的 torrent (info_hash: " << info_hash << ")" << std::endl;
        return false;
    }
    
    try {
        TorrentInfo& info = it->second;
        if (info.handle.is_valid()) {
            // 根据类型决定是否删除文件
            // 下载时只删除部分文件，做种时不删除文件
            if (info.type == TorrentType::Seeding) {
                session_->remove_torrent(info.handle, lt::session::delete_files);
            } else {
                session_->remove_torrent(info.handle, lt::session::delete_partfile);
            }
        }
        
        torrents_.erase(it);
        
        std::cout << "已停止 torrent (info_hash: " << info_hash.substr(0, 8) << "...)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "停止 torrent 时出错: " << e.what() << std::endl;
        torrents_.erase(it);
        return false;
    }
}

// 停止所有 torrent
void TorrentManager::stop_all()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!session_) {
        return;
    }
    
    try {
        for (auto& pair : torrents_) {
            TorrentInfo& info = pair.second;
            if (info.handle.is_valid()) {
                // 根据类型决定是否删除文件
                if (info.type == TorrentType::Seeding) {
                    session_->remove_torrent(info.handle, lt::session::delete_files);
                } else {
                    session_->remove_torrent(info.handle, lt::session::delete_partfile);
                }
            }
        }
        
        torrents_.clear();
        std::cout << "已停止所有 torrent" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "停止所有 torrent 时出错: " << e.what() << std::endl;
        torrents_.clear();
    }
}

// 停止所有下载
void TorrentManager::stop_all_downloads()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> to_remove;
    
    for (const auto& pair : torrents_) {
        if (pair.second.type == TorrentType::Download) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& info_hash : to_remove) {
        auto it = torrents_.find(info_hash);
        if (it != torrents_.end() && it->second.handle.is_valid()) {
            session_->remove_torrent(it->second.handle, lt::session::delete_partfile);
        }
    }
    
    for (const auto& info_hash : to_remove) {
        torrents_.erase(info_hash);
    }
    
    if (!to_remove.empty()) {
        std::cout << "已停止所有下载任务" << std::endl;
    }
}

// 停止所有做种
void TorrentManager::stop_all_seedings()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> to_remove;
    
    for (const auto& pair : torrents_) {
        if (pair.second.type == TorrentType::Seeding) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& info_hash : to_remove) {
        auto it = torrents_.find(info_hash);
        if (it != torrents_.end() && it->second.handle.is_valid()) {
            session_->remove_torrent(it->second.handle, lt::session::delete_files);
        }
    }
    
    for (const auto& info_hash : to_remove) {
        torrents_.erase(info_hash);
    }
    
    if (!to_remove.empty()) {
        std::cout << "已停止所有做种任务" << std::endl;
    }
}

// 暂停指定的 torrent
bool TorrentManager::pause_torrent(const std::string& info_hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = torrents_.find(info_hash);
    if (it == torrents_.end() || !it->second.handle.is_valid()) {
        std::cerr << "错误: 未找到指定的 torrent (info_hash: " << info_hash << ")" << std::endl;
        return false;
    }
    
    try {
        it->second.handle.pause();
        std::cout << "已暂停 torrent (info_hash: " << info_hash.substr(0, 8) << "...)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "暂停 torrent 时出错: " << e.what() << std::endl;
        return false;
    }
}

// 恢复指定的 torrent
bool TorrentManager::resume_torrent(const std::string& info_hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = torrents_.find(info_hash);
    if (it == torrents_.end() || !it->second.handle.is_valid()) {
        std::cerr << "错误: 未找到指定的 torrent (info_hash: " << info_hash << ")" << std::endl;
        return false;
    }
    
    try {
        it->second.handle.resume();
        std::cout << "已恢复 torrent (info_hash: " << info_hash.substr(0, 8) << "...)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "恢复 torrent 时出错: " << e.what() << std::endl;
        return false;
    }
}

// 暂停所有 torrent
void TorrentManager::pause_all()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : torrents_) {
        if (pair.second.handle.is_valid()) {
            try {
                pair.second.handle.pause();
            } catch (...) {
                // 忽略单个 torrent 的错误
            }
        }
    }
    
    std::cout << "已暂停所有 torrent" << std::endl;
}

// 恢复所有 torrent
void TorrentManager::resume_all()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : torrents_) {
        if (pair.second.handle.is_valid()) {
            try {
                pair.second.handle.resume();
            } catch (...) {
                // 忽略单个 torrent 的错误
            }
        }
    }
    
    std::cout << "已恢复所有 torrent" << std::endl;
}

// 从 status 创建 TorrentStatus
TorrentStatus TorrentManager::create_torrent_status(const TorrentInfo& info, const lt::torrent_status& status) const
{
    TorrentStatus ts;
    
    ts.info_hash = info.info_hash;
    ts.type = info.type;
    ts.torrent_path = info.torrent_path;
    ts.save_path = info.save_path;
    ts.is_valid = info.is_valid && info.handle.is_valid();
    
    ts.state = status.state;
    ts.total_size = status.total_wanted;
    ts.downloaded_bytes = status.total_wanted_done;
    ts.uploaded_bytes = status.total_upload;
    ts.download_rate = status.download_rate;
    ts.upload_rate = status.upload_rate;
    ts.peer_count = status.num_peers;
    ts.is_paused = (status.flags & lt::torrent_flags::paused) != 0;
    ts.is_finished = (status.state == lt::torrent_status::seeding || 
                      status.state == lt::torrent_status::finished);
    
    if (status.total_wanted > 0) {
        ts.progress = static_cast<double>(status.total_wanted_done) / 
                     static_cast<double>(status.total_wanted);
    } else {
        ts.progress = 0.0;
    }
    
    return ts;
}

// 获取指定 torrent 的状态
TorrentStatus TorrentManager::get_torrent_status(const std::string& info_hash) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    TorrentStatus ts;
    
    auto it = torrents_.find(info_hash);
    if (it == torrents_.end() || !it->second.handle.is_valid()) {
        return ts;
    }
    
    try {
        const TorrentInfo& info = it->second;
        lt::torrent_status status = info.handle.status();
        ts = create_torrent_status(info, status);
    } catch (const std::exception&) {
        // 返回默认状态
    }
    
    return ts;
}

// 获取所有 torrent 的状态
std::vector<TorrentStatus> TorrentManager::get_all_torrent_status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TorrentStatus> result;
    
    for (const auto& pair : torrents_) {
        const TorrentInfo& info = pair.second;
        if (!info.handle.is_valid()) {
            continue;
        }
        
        try {
            lt::torrent_status status = info.handle.status();
            result.push_back(create_torrent_status(info, status));
        } catch (const std::exception&) {
            // 跳过无效的 torrent
        }
    }
    
    return result;
}

// 获取所有下载任务的状态
std::vector<TorrentStatus> TorrentManager::get_download_status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TorrentStatus> result;
    
    for (const auto& pair : torrents_) {
        const TorrentInfo& info = pair.second;
        if (info.type != TorrentType::Download || !info.handle.is_valid()) {
            continue;
        }
        
        try {
            lt::torrent_status status = info.handle.status();
            result.push_back(create_torrent_status(info, status));
        } catch (const std::exception&) {
            // 跳过无效的 torrent
        }
    }
    
    return result;
}

// 获取所有做种任务的状态
std::vector<TorrentStatus> TorrentManager::get_seeding_status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TorrentStatus> result;
    
    for (const auto& pair : torrents_) {
        const TorrentInfo& info = pair.second;
        if (info.type != TorrentType::Seeding || !info.handle.is_valid()) {
            continue;
        }
        
        try {
            lt::torrent_status status = info.handle.status();
            result.push_back(create_torrent_status(info, status));
        } catch (const std::exception&) {
            // 跳过无效的 torrent
        }
    }
    
    return result;
}

// 检查指定 torrent 是否存在
bool TorrentManager::has_torrent(const std::string& info_hash) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return torrents_.find(info_hash) != torrents_.end();
}

// 获取 torrent 数量
size_t TorrentManager::get_torrent_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return get_torrent_count_unsafe();
}

// 获取下载任务数量
size_t TorrentManager::get_download_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return get_download_count_unsafe();
}

// 获取做种任务数量
size_t TorrentManager::get_seeding_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return get_seeding_count_unsafe();
}

// 无锁版本计数函数（仅在已持有 mutex_ 时调用）
size_t TorrentManager::get_torrent_count_unsafe() const
{
    return torrents_.size();
}

size_t TorrentManager::get_download_count_unsafe() const
{
    size_t count = 0;
    for (const auto& pair : torrents_) {
        if (pair.second.type == TorrentType::Download) {
            count++;
        }
    }
    return count;
}

size_t TorrentManager::get_seeding_count_unsafe() const
{
    size_t count = 0;
    for (const auto& pair : torrents_) {
        if (pair.second.type == TorrentType::Seeding) {
            count++;
        }
    }
    return count;
}

// 更新 torrent 状态（清理无效的 torrent）
void TorrentManager::update_torrents()
{
    std::vector<std::string> to_remove;
    
    for (const auto& pair : torrents_) {
        if (!pair.second.handle.is_valid()) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& info_hash : to_remove) {
        torrents_.erase(info_hash);
    }
}

// 等待并处理事件
bool TorrentManager::wait_and_process(int timeout_ms)
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
                auto* tfa = lt::alert_cast<lt::torrent_finished_alert>(alert);
                if (tfa) {
                    std::cout << std::endl;
                    std::cout << "=== Torrent 完成！===" << std::endl;
                    std::cout << std::endl;
                }
            } else if (lt::alert_cast<lt::tracker_announce_alert>(alert)) {
                // 可以在这里记录 tracker 公告信息（可选）
            } else if (lt::alert_cast<lt::torrent_error_alert>(alert)) {
                auto* tea = lt::alert_cast<lt::torrent_error_alert>(alert);
                if (tea) {
                    std::cerr << "Torrent 错误: " << tea->error.message() << std::endl;
                }
            } else if (lt::alert_cast<lt::file_error_alert>(alert)) {
                auto* fea = lt::alert_cast<lt::file_error_alert>(alert);
                if (fea) {
                    std::cerr << "文件错误: " << fea->error.message() << std::endl;
                    std::cerr << "  文件路径: " << fea->filename() << std::endl;
                }
            } else if (lt::alert_cast<lt::state_changed_alert>(alert)) {
                auto* sca = lt::alert_cast<lt::state_changed_alert>(alert);
                if (sca) {
                    // 状态改变时的处理（可选）
                }
            }
        }
        
        // 更新 torrent 状态（清理无效的）
        {
            std::lock_guard<std::mutex> lock(mutex_);
            update_torrents();
        }
        
        // 等待指定时间
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "处理事件时出错: " << e.what() << std::endl;
        return false;
    }
}

// 打印所有 torrent 的状态
void TorrentManager::print_all_status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (torrents_.empty()) {
        std::cout << "当前没有活动的 torrent" << std::endl;
        return;
    }
    
    std::cout << "=== 当前 Torrent 状态 (总数: " << torrents_.size() 
              << ", 下载: " << get_download_count() 
              << ", 做种: " << get_seeding_count() << ") ===" << std::endl;
    std::cout << std::endl;
    
    int index = 0;
    for (const auto& pair : torrents_) {
        ++index;
        const TorrentInfo& info = pair.second;
        
        if (!info.handle.is_valid()) {
            std::cout << "[Torrent #" << index << "] 句柄无效" << std::endl;
            continue;
        }
        
        try {
            lt::torrent_status status = info.handle.status();
            
            std::cout << "--- Torrent #" << index << " ---" << std::endl;
            std::cout << "Info Hash: " << info.info_hash.substr(0, 16) << "..." << std::endl;
            std::cout << "类型: " << (info.type == TorrentType::Download ? "下载" : "做种") << std::endl;
            std::cout << "Torrent 文件: " << info.torrent_path << std::endl;
            std::cout << "保存路径: " << info.save_path << std::endl;
            std::cout << "状态: ";
            
            if (status.state == lt::torrent_status::seeding) {
                std::cout << "做种中 (Seeding)" << std::endl;
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
            std::cout << "是否暂停: " << (status.flags & lt::torrent_flags::paused ? "是" : "否") << std::endl;
            
            std::cout << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Torrent #" << index << "] 获取状态时出错: " << e.what() << std::endl;
        }
    }
}

// 打印指定 torrent 的状态
void TorrentManager::print_torrent_status(const std::string& info_hash) const
{
    TorrentStatus ts = get_torrent_status(info_hash);
    
    if (!ts.is_valid) {
        std::cout << "未找到指定的 torrent (info_hash: " << info_hash << ")" << std::endl;
        return;
    }
    
    std::cout << "=== Torrent 状态 ===" << std::endl;
    std::cout << "Info Hash: " << ts.info_hash << std::endl;
    std::cout << "类型: " << (ts.type == TorrentType::Download ? "下载" : "做种") << std::endl;
    std::cout << "Torrent 文件: " << ts.torrent_path << std::endl;
    std::cout << "保存路径: " << ts.save_path << std::endl;
    std::cout << "状态: ";
    
    if (ts.state == lt::torrent_status::seeding) {
        std::cout << "做种中 (Seeding)" << std::endl;
    } else if (ts.state == lt::torrent_status::finished) {
        std::cout << "已完成 (Finished)" << std::endl;
    } else if (ts.state == lt::torrent_status::downloading) {
        std::cout << "下载中 (Downloading)" << std::endl;
    } else if (ts.state == lt::torrent_status::checking_files) {
        std::cout << "检查文件中 (Checking Files)" << std::endl;
    } else {
        std::cout << "其他状态 (" << static_cast<int>(ts.state) << ")" << std::endl;
    }
    
    std::cout << "进度: " << format_percent(ts.progress) << std::endl;
    std::cout << "已下载: " << format_bytes(ts.downloaded_bytes) 
              << " / " << format_bytes(ts.total_size) << std::endl;
    std::cout << "连接的对等节点数: " << ts.peer_count << std::endl;
    std::cout << "已上传: " << format_bytes(ts.uploaded_bytes) << std::endl;
    std::cout << "上传速度: " << format_speed(ts.upload_rate) << std::endl;
    std::cout << "下载速度: " << format_speed(ts.download_rate) << std::endl;
    std::cout << "是否暂停: " << (ts.is_paused ? "是" : "否") << std::endl;
    std::cout << "是否完成: " << (ts.is_finished ? "是" : "否") << std::endl;
    std::cout << std::endl;
}
