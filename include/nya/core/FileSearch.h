#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
namespace nya::core {
enum class FileSearchMode { Substring, Fuzzy };
struct FileSearchMatch { std::filesystem::path path; std::filesystem::path relativePath; };
struct FileSearchResult {
    std::vector<FileSearchMatch> matches;
    std::string error;
    int skippedDirectories = 0;
    bool truncated = false;
    bool canceled = false;
};
class FileSearch {
public:
    // Unicode case-insensitive path search; directory symlinks are never traversed.
    static FileSearchResult findFiles(const std::filesystem::path &root, const std::u16string &query,
        const std::shared_ptr<std::atomic_bool> &cancel = {}, int limit = 1000, FileSearchMode mode = FileSearchMode::Fuzzy);
};
}
