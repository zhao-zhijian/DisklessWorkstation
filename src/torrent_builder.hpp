#ifndef TORRENT_BUILDER_HPP
#define TORRENT_BUILDER_HPP

#include <string>
#include <vector>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/sha1_hash.hpp>

// Torrent 种子生成器类
class TorrentBuilder
{
public:
    TorrentBuilder();
    
    // 生成 torrent 文件
    bool create_torrent(const std::string& file_path, const std::string& output_path);

    // 设置 tracker 列表
    inline void set_trackers(const std::vector<std::string>& trackers) { trackers_ = trackers; }
    
    // 添加单个 tracker
    inline void add_tracker(const std::string& tracker) { trackers_.push_back(tracker); }
    
    // 设置注释
    inline void set_comment(const std::string& comment) { comment_ = comment; }
    
    // 设置创建者
    inline void set_creator(const std::string& creator) { creator_ = creator; }
    
    // 设置分片大小（字节），0 表示使用默认大小或自动选择
    inline void set_piece_size(int piece_size) { piece_size_ = piece_size; }
    
    // 获取当前配置的 tracker 列表
    inline const std::vector<std::string>& get_trackers() const { return trackers_; }

private:
    // 验证路径是否存在
    bool validate_path(const std::string& file_path);
    
    // 确定根路径（父目录）
    std::string determine_root_path(const std::string& file_path);
    
    // 添加文件或目录到存储
    void add_files_to_storage(lt::file_storage& fs_storage, const std::string& file_path);
    
    // 从 entry 中提取 info_hash
    lt::sha1_hash extract_info_hash(const lt::entry& torrent_entry);
    
    // 写入 torrent 文件
    bool write_torrent_file(const lt::entry& torrent_entry, const std::string& output_path);

private:
    std::vector<std::string> trackers_;  // tracker 列表
    std::string comment_;                // 注释
    std::string creator_;                 // 创建者
    int piece_size_;                     // 分片大小（0 表示使用默认或自动选择）
};

#endif // TORRENT_BUILDER_HPP

