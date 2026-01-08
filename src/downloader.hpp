#ifndef DOWNLOADER_HPP
#define DOWNLOADER_HPP

#include <string>
#include <memory>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>

// Torrent 下载类
class Downloader
{
public:
    Downloader();
    ~Downloader();
    
    // 禁止拷贝构造和赋值
    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;
    
    // 从 torrent 文件开始下载
    // torrent_path: torrent 文件路径
    // save_path: 下载文件的保存目录
    bool start_download(const std::string& torrent_path, const std::string& save_path);
    
    // 停止下载
    void stop_download();
    
    // 暂停下载
    void pause();
    
    // 恢复下载
    void resume();
    
    // 检查是否正在下载
    bool is_downloading() const;
    
    // 检查是否已完成下载
    bool is_finished() const;
    
    // 检查是否已暂停
    bool is_paused() const;
    
    // 获取下载状态信息
    void print_status() const;
    
    // 等待并处理事件（用于保持下载状态）
    // 返回 false 表示应该退出
    bool wait_and_process(int timeout_ms = 1000);
    
    // 获取当前连接的 peer 数量
    int get_peer_count() const;
    
    // 获取已下载的字节数
    std::int64_t get_downloaded_bytes() const;
    
    // 获取已上传的字节数
    std::int64_t get_uploaded_bytes() const;
    
    // 获取下载速度（字节/秒）
    int get_download_rate() const;
    
    // 获取上传速度（字节/秒）
    int get_upload_rate() const;
    
    // 获取下载进度（0.0 - 1.0）
    double get_progress() const;
    
    // 获取总文件大小（字节）
    std::int64_t get_total_size() const;

private:
    // 初始化 session 设置
    void configure_session();
    
    // 验证路径
    bool validate_paths(const std::string& torrent_path, const std::string& save_path);

private:
    std::unique_ptr<lt::session> session_;      // libtorrent 会话
    lt::torrent_handle torrent_handle_;          // torrent 句柄
    bool is_downloading_;                        // 是否正在下载
};

#endif // DOWNLOADER_HPP

