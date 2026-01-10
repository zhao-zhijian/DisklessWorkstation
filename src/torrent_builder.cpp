#include "torrent_builder.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <algorithm>
#include <vector>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/sha1_hash.hpp>

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

#ifdef _WIN32
// 辅助函数：将 Windows 错误代码转换为 UTF-8 字符串（修复乱码问题）
static std::string get_windows_error_message(DWORD error_code)
{
    // 使用 FormatMessageW 获取宽字符消息，然后转换为 UTF-8，避免乱码
    LPWSTR message_buffer = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error_code, MAKELANGID(LANG_CHINESE_SIMPLIFIED, SUBLANG_DEFAULT),
        (LPWSTR)&message_buffer, 0, NULL);
    
    std::string message;
    if (message_buffer && size > 0) {
        // 将宽字符转换为 UTF-8
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, message_buffer, -1, NULL, 0, NULL, NULL);
        if (utf8_len > 0) {
            std::vector<char> utf8_buffer(utf8_len);
            WideCharToMultiByte(CP_UTF8, 0, message_buffer, -1, utf8_buffer.data(), utf8_len, NULL, NULL);
            message = utf8_buffer.data();
        } else {
            // 如果转换失败，使用错误代码
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Windows 错误代码: %lu", error_code);
            message = buffer;
        }
        LocalFree(message_buffer);
        
        // 移除末尾的换行符
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
            message.pop_back();
        }
    } else {
        // 如果获取失败，根据错误代码提供中文描述
        switch (error_code) {
            case 995:
                message = "I/O 操作被中止（线程退出或应用程序请求）";
                break;
            case 5:
                message = "访问被拒绝";
                break;
            case 32:
                message = "文件正在被其他程序使用";
                break;
            case 112:
                message = "磁盘空间不足";
                break;
            case 1450:
                message = "系统资源不足，无法完成请求的服务";
                break;
            default: {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "Windows 错误代码: %lu", error_code);
                message = buffer;
                break;
            }
        }
    }
    
    return message;
}
#endif

// 辅助函数：格式化异常信息，处理乱码问题
static std::string format_exception_message(const std::exception& e)
{
    std::string msg = e.what();
    
#ifdef _WIN32
    // 尝试从错误消息中提取 Windows 错误代码
    size_t pos = msg.find("[system:");
    if (pos != std::string::npos) {
        size_t end_pos = msg.find("]", pos);
        if (end_pos != std::string::npos) {
            std::string error_code_str = msg.substr(pos + 8, end_pos - pos - 8);
            try {
                DWORD error_code = std::stoul(error_code_str);
                std::string windows_msg = get_windows_error_message(error_code);
                return "系统错误: " + windows_msg + " (错误代码: " + error_code_str + ")";
            } catch (...) {
                // 如果转换失败，使用原始消息
            }
        }
    }
    
    // 如果是 system_error，尝试获取错误代码
    const std::system_error* sys_err = dynamic_cast<const std::system_error*>(&e);
    if (sys_err) {
        std::error_code ec = sys_err->code();
        if (ec.category() == std::system_category()) {
            DWORD error_code = static_cast<DWORD>(ec.value());
            std::string windows_msg = get_windows_error_message(error_code);
            return "系统错误: " + windows_msg + " (错误代码: " + std::to_string(error_code) + ")";
        }
    }
#endif
    
    return msg;
}

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

        // 计算总文件大小，为大文件设置合适的分片大小
        std::int64_t total_size = fs_storage.total_size();
        if (total_size > 0) {
            // 对于大文件（>4GB），使用16MB的分片大小
            // 对于超大文件（>50GB），使用16MB分片大小以确保性能
            if (total_size > 4LL * 1024 * 1024 * 1024) {  // 4GB
                int recommended_piece_size = 16 * 1024 * 1024;  // 16MB
                if (piece_size_ == 0 || piece_size_ < recommended_piece_size) {
                    fs_storage.set_piece_length(recommended_piece_size);
                    std::cout << "检测到大文件（总大小: " 
                              << (total_size / 1024.0 / 1024.0 / 1024.0) 
                              << " GB），已设置分片大小为 16MB" << std::endl;
                }
            } else if (piece_size_ > 0) {
                // 如果用户指定了分片大小，使用用户指定的值
                fs_storage.set_piece_length(piece_size_);
            }
        }

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
        std::cout << "文件数量: " << fs_storage.num_files() << std::endl;
        std::cout << "总大小: " << format_bytes(fs_storage.total_size()) << std::endl;
        std::cout << "分片大小: " << format_bytes(fs_storage.piece_length()) << std::endl;
        std::cout << "分片数量: " << fs_storage.num_pieces() << std::endl;
        
        // 显示前几个文件的路径，用于调试和确定正确的根路径
        namespace fs = std::filesystem;
        std::string actual_root_path = root_path;
        
        if (fs_storage.num_files() > 0) {
            std::cout << "前几个文件在 storage 中的路径（用于调试）:" << std::endl;
            for (int i = 0; i < std::min(5, static_cast<int>(fs_storage.num_files())); ++i) {
                lt::file_index_t idx(i);
                std::string file_path_in_storage = fs_storage.file_path(idx);
                std::cout << "  [" << i << "] " << file_path_in_storage << std::endl;
                
                // 根据 storage 中的文件路径来确定正确的根路径
                // 如果路径以目录名开头（如 "Data/file.txt"），说明 root_path 应该是父目录
                // 如果路径直接是文件名（如 "file.txt"），说明 root_path 应该是目录本身
                if (i == 0 && !file_path_in_storage.empty()) {
                    fs::path storage_path(file_path_in_storage);
                    if (storage_path.has_parent_path() && !storage_path.parent_path().empty()) {
                        // 路径包含目录，说明 root_path 应该是父目录
                        // 已经设置正确
                    } else {
                        // 路径直接是文件名，说明 root_path 应该是目录本身
                        // 需要调整 root_path
                        fs::path input_path_obj(file_path);
                        if (!input_path_obj.is_absolute()) {
                            input_path_obj = fs::absolute(input_path_obj);
                        }
                        std::string input_path_str = input_path_obj.lexically_normal().string();
                        while (!input_path_str.empty() && (input_path_str.back() == '\\' || input_path_str.back() == '/')) {
                            input_path_str.pop_back();
                        }
                        if (fs::is_directory(input_path_obj)) {
                            actual_root_path = input_path_obj.string();
                            std::replace(actual_root_path.begin(), actual_root_path.end(), '\\', '/');
                            std::cout << "  注意：根据 storage 路径，将根路径调整为: " << actual_root_path << std::endl;
                        }
                    }
                }
            }
        }
        
        std::cout << "使用的根路径: " << actual_root_path << std::endl;
        std::cout << "这可能需要一些时间，请稍候..." << std::endl;
        std::cout << std::endl;
        
        // 验证根路径存在且可访问
        // 将正斜杠路径转换回 Windows 格式进行验证
        std::string verify_path = actual_root_path;
        std::replace(verify_path.begin(), verify_path.end(), '/', '\\');
        
        if (!fs::exists(verify_path)) {
            std::cerr << "错误: 根路径不存在: " << verify_path << std::endl;
            return false;
        }
        
        // 验证文件访问权限（Windows 特定）
#ifdef _WIN32
        // 将路径转换为宽字符以进行 Windows API 调用
        int path_len = MultiByteToWideChar(CP_UTF8, 0, verify_path.c_str(), -1, NULL, 0);
        if (path_len > 0) {
            std::vector<wchar_t> wide_path(path_len);
            MultiByteToWideChar(CP_UTF8, 0, verify_path.c_str(), -1, wide_path.data(), path_len);
            
            DWORD attrs = GetFileAttributesW(wide_path.data());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                DWORD error_code = GetLastError();
                std::cerr << "错误: 无法访问根路径: " << verify_path << std::endl;
                std::cerr << "      " << get_windows_error_message(error_code) << std::endl;
                return false;
            }
            
            // 检查是否为目录且可读
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                std::cerr << "错误: 根路径不是一个目录: " << verify_path << std::endl;
                return false;
            }
        }
#endif
        
        // 更新 root_path 为实际使用的路径
        root_path = actual_root_path;
        
        try {
            // 错误 995 (ERROR_OPERATION_ABORTED) 通常表示 I/O 操作被中断
            // 可能原因：
            // 1. 路径不匹配：set_piece_hashes 的 root_path 与 add_files 时使用的路径不一致
            // 2. 文件句柄问题：同时打开太多文件或文件被锁定
            // 3. 异步 I/O 问题：libtorrent 内部的 I/O 操作被中断
            // 4. 资源限制：虽然内存足够，但可能有其他资源限制
            
            // 使用根据 storage 路径确定的根路径
            std::string libtorrent_path = root_path;
            std::replace(libtorrent_path.begin(), libtorrent_path.end(), '\\', '/');
            
            // 对于大文件，输出进度提示
            std::cout << "开始计算哈希值..." << std::endl;
            std::cout << "使用根路径: " << libtorrent_path << std::endl;
            std::cout << "注意：对于 50GB+ 的大文件，这可能需要几分钟到十几分钟，请耐心等待..." << std::endl;
            std::cout << "正在处理中，请勿中断程序..." << std::flush;
            
            // 使用 set_piece_hashes 计算哈希值
            // 这个函数会读取所有文件并计算每个分片的 SHA1 哈希
            // 错误 995 可能是由于路径不匹配、文件句柄限制或其他资源问题
            lt::set_piece_hashes(torrent, libtorrent_path.c_str());
            
            std::cout << "\r文件哈希值计算完成！                              " << std::endl;
        } catch (const std::system_error& e) {
            std::cerr << "计算文件哈希值时发生系统错误: " << format_exception_message(e) << std::endl;
            std::cerr << std::endl;
            std::cerr << "可能的原因:" << std::endl;
            std::cerr << "  1. 文件正在被其他程序使用，请关闭相关程序后重试" << std::endl;
            std::cerr << "  2. 磁盘空间不足，请检查可用磁盘空间" << std::endl;
            std::cerr << "  3. 内存不足，请关闭其他程序释放内存" << std::endl;
            std::cerr << "  4. 文件权限不足，请检查文件访问权限" << std::endl;
            std::cerr << "  5. 磁盘错误，请运行磁盘检查工具" << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "计算文件哈希值时出错: " << format_exception_message(e) << std::endl;
            std::cerr << std::endl;
            std::cerr << "提示: 对于大文件（>50GB），请确保:" << std::endl;
            std::cerr << "  - 有足够的磁盘空间（建议至少是文件大小的 10%）" << std::endl;
            std::cerr << "  - 有足够的可用内存" << std::endl;
            std::cerr << "  - 文件没有被其他程序锁定" << std::endl;
            throw;
        }
        
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
    catch (const std::system_error& e) {
        std::cerr << "生成 torrent 时发生系统错误: " << format_exception_message(e) << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "生成 torrent 时出错: " << format_exception_message(e) << std::endl;
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
    
    // 检查是否为空目录
    if (fs::is_directory(file_path)) {
        bool has_files = false;
        for (const auto& entry : fs::recursive_directory_iterator(file_path)) {
            if (fs::is_regular_file(entry)) {
                has_files = true;
                break;
            }
        }
        if (!has_files) {
            std::cerr << "错误: 目录为空，无法创建 torrent: " << file_path << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string TorrentBuilder::determine_root_path(const std::string& file_path)
{
    namespace fs = std::filesystem;
    
    fs::path path_obj(file_path);
    
    // 标准化路径（转换为绝对路径并规范化）
    if (!path_obj.is_absolute()) {
        path_obj = fs::absolute(path_obj);
    }
    
    // 移除末尾的反斜杠（Windows 路径问题）
    std::string path_str = path_obj.lexically_normal().string();
    while (!path_str.empty() && (path_str.back() == '\\' || path_str.back() == '/')) {
        path_str.pop_back();
    }
    path_obj = fs::path(path_str);
    
    // 对于 libtorrent 的 add_files 和 set_piece_hashes:
    // 根据 libtorrent 2.0 的实际行为（经过测试）：
    // - 如果输入是文件 "D:/A/file.txt": add_files 会在 storage 中创建路径 "file.txt"
    //   所以 set_piece_hashes 的 root_path 应该是 "D:/A"（文件的父目录）
    // - 如果输入是目录 "D:/A/B": add_files 会在 storage 中创建路径，这些路径是相对于输入目录的父目录的
    //   例如：文件 "D:/A/B/file.txt" 在 storage 中可能是 "B/file.txt"
    //   所以 set_piece_hashes 的 root_path 应该是 "D:/A"（目录的父目录）
    // 但是，某些版本的 libtorrent 可能会保持相对于输入目录的路径
    // 为了兼容性，我们先尝试使用父目录作为根路径
    std::string root_path;
    
    if (fs::is_directory(path_obj)) {
        // 如果是目录，使用目录的父目录作为根路径
        // 因为 libtorrent 通常会在 storage 中保持目录名作为路径的一部分
        root_path = path_obj.parent_path().string();
        
        // 如果父目录为空或只有根路径，使用目录本身
        if (root_path.empty() || root_path == path_obj.root_path().string()) {
            root_path = path_obj.string();
        }
    } else {
        // 如果是文件，使用文件的父目录作为根路径
        root_path = path_obj.parent_path().string();
    }
    
    // 处理边界情况
    if (root_path.empty()) {
        root_path = fs::absolute(".").string();
    }
    
    // libtorrent 在 Windows 上使用正斜杠路径（根据文档和实际测试）
    // 转换为正斜杠以确保兼容性
    std::replace(root_path.begin(), root_path.end(), '\\', '/');
    
    return root_path;
}

void TorrentBuilder::add_files_to_storage(lt::file_storage& fs_storage, const std::string& file_path)
{
    namespace fs = std::filesystem;
    
    // 规范化路径：移除末尾的反斜杠，转换为标准格式
    std::string normalized_path = file_path;
    while (!normalized_path.empty() && (normalized_path.back() == '\\' || normalized_path.back() == '/')) {
        normalized_path.pop_back();
    }
    
    // 转换为绝对路径
    fs::path path_obj(normalized_path);
    if (!path_obj.is_absolute()) {
        path_obj = fs::absolute(path_obj);
    }
    normalized_path = path_obj.lexically_normal().string();
    
    // 在 Windows 上，libtorrent 期望使用正斜杠路径（根据文档）
    // 根据 libtorrent 文档，add_files 接受正斜杠路径，即使是在 Windows 上
    std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
    
    // 添加文件到存储
    // 注意：根据 libtorrent 的行为：
    // - 如果输入是目录 "D:/A/B"，add_files 会在 storage 中创建相对于父目录的路径
    // - 例如：文件 "D:/A/B/file.txt" 在 storage 中会是 "B/file.txt"
    // - 但是，在某些版本中，也可能是 "file.txt"（相对于输入目录）
    // 为了确保兼容性，我们需要确保路径格式正确
    lt::add_files(fs_storage, normalized_path, [](std::string const&) { return true; });
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