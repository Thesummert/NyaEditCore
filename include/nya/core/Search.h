#pragma once
#include <string>
#include <vector>
namespace nya::core {
struct SearchOptions {
    std::u16string pattern;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool regex = false;
};
struct SearchMatch { int start; int length; };
struct SearchResult {
    std::vector<SearchMatch> matches;
    std::string error;
    bool truncated = false;
};
class Search {
public:
    static SearchResult find(const std::u16string &text, const SearchOptions &options, int limit = 20000);
    // Expand the replacement for a match from the same global iterator as find().
    static std::u16string replacementForMatch(const std::u16string &text, const SearchOptions &options,
        const std::u16string &replacement, int matchIndex, std::string *error);
    // Regex replacement supports $1, ${name}, and $$.
    static std::u16string replaceAll(const std::u16string &text, const SearchOptions &options,
                                    const std::u16string &replacement, int *count, std::string *error);
};
}
