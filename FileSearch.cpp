#include <nya/core/FileSearch.h>
#include <unicode/unistr.h>
#include <algorithm>
namespace nya::core {
namespace fs = std::filesystem;
namespace {
icu::UnicodeString folded(const std::u16string &s) { return icu::UnicodeString(s.data(), static_cast<int32_t>(s.size())).foldCase(); }
bool fuzzy(const icu::UnicodeString &path, const icu::UnicodeString &query) {
    int next = 0;
    for (int i = 0; i < path.length(); ) {
        const auto ch = path.char32At(i); i += U16_LENGTH(ch);
        if (ch == query.char32At(next)) { next += U16_LENGTH(ch); if (next == query.length()) return true; }
    }
    return false;
}
}
FileSearchResult FileSearch::findFiles(const fs::path &root, const std::u16string &query,
    const std::shared_ptr<std::atomic_bool> &cancel, int limit, FileSearchMode mode) {
    FileSearchResult r;
    const auto canceled = [&] { return cancel && cancel->load(std::memory_order_relaxed); };
    if (canceled()) { r.canceled = true; return r; }
    auto needle = folded(query); needle.trim(); needle.findAndReplace(u"\\", u"/");
    if (needle.isEmpty() || limit <= 0) return r;
    std::error_code ec;
    const auto base = fs::absolute(root, ec);
    if (root.empty() || ec || !fs::is_directory(base, ec)) { r.error = "Folder does not exist or cannot be read"; return r; }
    std::vector<fs::path> pending{base};
    while (!pending.empty() && !r.truncated) {
        const auto path = pending.back(); pending.pop_back();
        fs::directory_iterator it(path, ec), end;
        if (ec) { ++r.skippedDirectories; ec.clear(); continue; }
        while (it != end) {
            if (canceled()) { r.canceled = true; return r; }
            const auto entry = *it;
            if (entry.is_directory(ec)) {
                const auto name = folded(entry.path().filename().u16string());
                if (!entry.is_symlink(ec) && name != u".git" && name != u".hg" && name != u".svn") pending.push_back(entry.path());
            } else if (entry.is_regular_file(ec)) {
                const auto relative = entry.path().lexically_relative(base);
                const auto name = folded(relative.generic_u16string());
                if (mode == FileSearchMode::Fuzzy ? fuzzy(name, needle) : name.indexOf(needle) >= 0) {
                    if (static_cast<int>(r.matches.size()) >= limit) { r.truncated = true; break; }
                    r.matches.push_back({entry.path(), relative});
                }
            }
            it.increment(ec);
            if (ec) { ++r.skippedDirectories; ec.clear(); break; }
        }
    }
    std::sort(r.matches.begin(), r.matches.end(), [](const auto &a, const auto &b) {
        const int order = folded(a.relativePath.generic_u16string()).compare(folded(b.relativePath.generic_u16string()));
        return order == 0 ? a.relativePath < b.relativePath : order < 0;
    });
    return r;
}
}
