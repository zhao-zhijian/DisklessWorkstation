#include "torrent_builder.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/sha1_hash.hpp>

TorrentBuilder::TorrentBuilder()
    : creator_("DisklessWorkstation")
    , piece_size_(0)  // 0 表示使用默认大小
{
}

bool TorrentBuilder::create_torrent(const std::string& file_path, const std::string& output_path)
{
    try {
        // 验证输入路径
        if (!validate_path(file_path)) return false;

        // 确定根路径
        std::string root_path = determine_root_path(file_path);

        // 创建文件存储对象
        lt::file_storage fs_storage;
        
        // 添加文件到存储
        add_files_to_storage(fs_storage, file_path);

        // 自定义分片大小
        // fs_storage.set_piece_length(piece_size_);

        // 创建 torrent 对象
        lt::create_torrent torrent(fs_storage);
        
        // 注意：分片大小在创建 create_torrent 时自动计算，通常不需要手动设置
        // 如果需要自定义分片大小，可以在创建 fs_storage 后、创建 torrent 前设置
        
        // 添加 tracker
        for (const auto& tracker : trackers_) {
            torrent.add_tracker(tracker);
        }
        
        // 设置注释
        if (!comment_.empty()) {
            torrent.set_comment(comment_.c_str());
        }
        
        // 设置创建者
        if (!creator_.empty()) {
            torrent.set_creator(creator_.c_str());
        }
        
        // 计算哈希值
        std::cout << "正在计算文件哈希值..." << std::endl;
        lt::set_piece_hashes(torrent, root_path.c_str());
        
        // 生成 torrent 字典
        lt::entry torrent_entry = torrent.generate();
        
        // 提取信息哈希
        lt::sha1_hash info_hash_v1 = extract_info_hash(torrent_entry);
        
        // 写入文件
        if (!write_torrent_file(torrent_entry, output_path)) {
            return false;
        }
        
        // 显示结果信息
        std::cout << "成功生成 torrent 文件: " << output_path << std::endl;
        if (!info_hash_v1.is_all_zeros()) {
            std::cout << "Info Hash v1: " << info_hash_v1 << std::endl;
        }
        
        // 显示 tracker 信息
        if (!trackers_.empty()) {
            std::cout << "已添加 " << trackers_.size() << " 个 Tracker:" << std::endl;
            for (size_t i = 0; i < trackers_.size(); ++i) {
                std::cout << "  [" << (i + 1) << "] " << trackers_[i] << std::endl;
            }
            std::cout << std::endl;
            std::cout << "提示: Tracker URL 已写入 torrent 文件。" << std::endl;
            std::cout << "      使用 BitTorrent 客户端打开 torrent 文件并开始做种后，" << std::endl;
            std::cout << "      客户端会自动向这些 Tracker 报告，Tracker 会记录你的做种信息。" << std::endl;
        } else {
            std::cout << "警告: 未添加任何 Tracker。" << std::endl;
            std::cout << "      建议添加 Tracker URL 以便其他用户能够发现你的做种。" << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "生成 torrent 时出错: " << e.what() << std::endl;
        return false;
    }
}

bool TorrentBuilder::validate_path(const std::string& file_path)
{
    namespace fs = std::filesystem;
    
    if (!fs::exists(file_path)) {
        std::cerr << "错误: 路径不存在: " << file_path << std::endl;
        return false;
    }
    return true;
}

std::string TorrentBuilder::determine_root_path(const std::string& file_path)
{
    namespace fs = std::filesystem;
    
    std::string root_path = fs::path(file_path).parent_path().string();
    if (root_path.empty()) {
        root_path = ".";
    }
    return root_path;
}

void TorrentBuilder::add_files_to_storage(lt::file_storage& fs_storage, const std::string& file_path)
{
    lt::add_files(fs_storage, file_path, [](std::string const&) { return true; });
}

lt::sha1_hash TorrentBuilder::extract_info_hash(const lt::entry& torrent_entry)
{
    lt::sha1_hash info_hash_v1;
    
    if (torrent_entry.type() == lt::entry::dictionary_t) {
        const lt::entry::dictionary_type& dict = torrent_entry.dict();
        auto info_it = dict.find("info");
        if (info_it != dict.end()) {
            // 计算 info 部分的 SHA1 哈希
            std::vector<char> info_buf;
            lt::bencode(std::back_inserter(info_buf), info_it->second);
            info_hash_v1 = lt::sha1_hash(lt::hasher(info_buf).final());
        }
    }
    
    return info_hash_v1;
}

bool TorrentBuilder::write_torrent_file(const lt::entry& torrent_entry, const std::string& output_path)
{
    // 编码为 bencode 格式
    std::vector<char> torrent_data;
    lt::bencode(std::back_inserter(torrent_data), torrent_entry);
    
    // 写入文件
    std::ofstream out(output_path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "错误: 无法创建输出文件: " << output_path << std::endl;
        return false;
    }
    
    out.write(torrent_data.data(), torrent_data.size());
    out.close();
    
    std::cout << "文件大小: " << torrent_data.size() << " 字节" << std::endl;
    
    return true;
}