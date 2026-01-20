#ifndef TORRENT_MANAGER_HPP
#define TORRENT_MANAGER_HPP

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_status.hpp>

// Torrent 类型枚举
enum class TorrentType {
    Download,  // 下载
    Seeding    // 做种
};

// Torrent 信息结构体
struct TorrentInfo {
    lt::torrent_handle handle;      // torrent 句柄
    TorrentType type;                // 类型（下载/做种）
    std::string torrent_path;        // torrent 文件路径
    std::string save_path;           // 保存路径
    std::string info_hash;           // info hash（用于唯一标识）
    bool is_valid;                   // 是否有效
    
    TorrentInfo() : is_valid(false) {}
};

// Torrent 状态结构体
struct TorrentStatus {
    std::string info_hash;           // info hash
    TorrentType type;                // 类型
    std::string torrent_path;        // torrent 文件路径
    std::string save_path;           // 保存路径
    bool is_valid;                   // 是否有效
    
    // 状态信息
    lt::torrent_status::state_t state;  // 状态
    double progress;                     // 进度 (0.0 - 1.0)
    std::int64_t total_size;            // 总大小（字节）
    std::int64_t downloaded_bytes;      // 已下载（字节）
    std::int64_t uploaded_bytes;        // 已上传（字节）
    int download_rate;                  // 下载速度（字节/秒）
    int upload_rate;                    // 上传速度（字节/秒）
    int peer_count;                     // 连接的 peer 数量
    bool is_paused;                     // 是否暂停
    bool is_finished;                   // 是否完成
    
    TorrentStatus() 
        : is_valid(false)
        , state(lt::torrent_status::checking_files)
        , progress(0.0)
        , total_size(0)
        , downloaded_bytes(0)
        , uploaded_bytes(0)
        , download_rate(0)
        , upload_rate(0)
        , peer_count(0)
        , is_paused(false)
        , is_finished(false)
    {}
};

// Torrent 管理器类（单例模式）
class TorrentManager
{
public:
    // 获取单例实例
    static TorrentManager& getInstance();
    
    // 禁止拷贝构造和赋值
    TorrentManager(const TorrentManager&) = delete;
    TorrentManager& operator=(const TorrentManager&) = delete;
    
    // 析构函数
    ~TorrentManager();
    
    // 开始下载
    // torrent_path: torrent 文件路径
    // save_path: 下载文件的保存目录
    // 返回: info_hash（用于后续操作），失败返回空字符串
    std::string start_download(const std::string& torrent_path, const std::string& save_path);
    
    // 开始做种
    // torrent_path: torrent 文件路径
    // save_path: 原始文件/目录的保存路径（必须与创建 torrent 时的路径一致）
    // 返回: info_hash（用于后续操作），失败返回空字符串
    std::string start_seeding(const std::string& torrent_path, const std::string& save_path);
    
    // 停止指定的 torrent（通过 info_hash）
    bool stop_torrent(const std::string& info_hash);
    
    // 停止所有 torrent
    void stop_all();
    
    // 停止所有下载
    void stop_all_downloads();
    
    // 停止所有做种
    void stop_all_seedings();
    
    // 暂停指定的 torrent
    bool pause_torrent(const std::string& info_hash);
    
    // 恢复指定的 torrent
    bool resume_torrent(const std::string& info_hash);
    
    // 暂停所有 torrent
    void pause_all();
    
    // 恢复所有 torrent
    void resume_all();
    
    // 获取指定 torrent 的状态
    TorrentStatus get_torrent_status(const std::string& info_hash) const;
    
    // 获取所有 torrent 的状态
    std::vector<TorrentStatus> get_all_torrent_status() const;
    
    // 获取所有下载任务的状态
    std::vector<TorrentStatus> get_download_status() const;
    
    // 获取所有做种任务的状态
    std::vector<TorrentStatus> get_seeding_status() const;
    
    // 检查指定 torrent 是否存在
    bool has_torrent(const std::string& info_hash) const;
    
    // 获取 torrent 数量
    size_t get_torrent_count() const;
    
    // 获取下载任务数量
    size_t get_download_count() const;
    
    // 获取做种任务数量
    size_t get_seeding_count() const;
    
    // 等待并处理事件（用于保持运行状态）
    // 返回 false 表示应该退出
    bool wait_and_process(int timeout_ms = 1000);
    
    // 打印所有 torrent 的状态
    void print_all_status() const;
    
    // 打印指定 torrent 的状态
    void print_torrent_status(const std::string& info_hash) const;

private:
    // 私有构造函数（单例模式）
    TorrentManager();
    
    // 初始化 session 设置
    void configure_session();
    
    // 验证路径
    bool validate_paths(const std::string& torrent_path, const std::string& save_path, bool create_save_path = false);
    
    // 从 torrent_info 获取 info_hash 字符串
    std::string get_info_hash_string(const lt::torrent_info& ti) const;
    
    // 更新 torrent 状态（清理无效的 torrent）
    void update_torrents();
    
    // 从 status 创建 TorrentStatus
    TorrentStatus create_torrent_status(const TorrentInfo& info, const lt::torrent_status& status) const;

    // 无锁版本计数函数（仅在已持有 mutex_ 时调用）
    size_t get_torrent_count_unsafe() const;
    size_t get_download_count_unsafe() const;
    size_t get_seeding_count_unsafe() const;

private:
    std::unique_ptr<lt::session> session_;              // libtorrent 会话（共享）
    std::map<std::string, TorrentInfo> torrents_;       // 管理的所有 torrent（以 info_hash 为键）
    mutable std::mutex mutex_;                          // 互斥锁（用于线程安全）
};

#endif // TORRENT_MANAGER_HPP
