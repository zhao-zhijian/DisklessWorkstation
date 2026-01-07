#ifndef SEEDER_HPP
#define SEEDER_HPP

#include <string>
#include <memory>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>

// Torrent 做种类
class Seeder
{
public:
    Seeder();
    ~Seeder();
    
    // 禁止拷贝构造和赋值
    Seeder(const Seeder&) = delete;
    Seeder& operator=(const Seeder&) = delete;
    
    // 从 torrent 文件开始做种
    // torrent_path: torrent 文件路径
    // save_path: 原始文件/目录的保存路径（必须与创建 torrent 时的路径一致）
    bool start_seeding(const std::string& torrent_path, const std::string& save_path);
    
    // 停止做种
    void stop_seeding();
    
    // 检查是否正在做种
    bool is_seeding() const;
    
    // 获取做种状态信息
    void print_status() const;
    
    // 等待并处理事件（用于保持做种状态）
    // 返回 false 表示应该退出
    bool wait_and_process(int timeout_ms = 1000);
    
    // 获取当前连接的 peer 数量
    int get_peer_count() const;
    
    // 获取已上传的字节数
    std::int64_t get_uploaded_bytes() const;
    
    // 获取下载的字节数（通常做种时为0）
    std::int64_t get_downloaded_bytes() const;

private:
    // 初始化 session 设置
    void configure_session();
    
    // 验证路径
    bool validate_paths(const std::string& torrent_path, const std::string& save_path);

private:
    std::unique_ptr<lt::session> session_;      // libtorrent 会话
    lt::torrent_handle torrent_handle_;          // torrent 句柄
    bool is_seeding_;                            // 是否正在做种
};

#endif // SEEDER_HPP

