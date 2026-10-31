#include <nya/core/Search.h>
#include <unicode/regex.h>
#include <memory>
namespace nya::core {
namespace {
icu::UnicodeString unicode(const std::u16string &s) { return icu::UnicodeString(s.data(), static_cast<int32_t>(s.size())); }
std::u16string text(const icu::UnicodeString &s) { return {s.getBuffer(), s.getBuffer() + s.length()}; }
std::unique_ptr<icu::RegexPattern> compile(const SearchOptions &o, UErrorCode &status) {
    auto p = unicode(o.pattern);
    if (!o.regex) {
        icu::UnicodeString escaped;
        for (int i = 0; i < p.length(); ++i) {
            if (icu::UnicodeString(u"\\.^$|?*+()[]{}").indexOf(p[i]) >= 0) escaped += u'\\';
            escaped += p[i];
        }
        p = escaped;
    }
    if (o.wholeWord) p = icu::UnicodeString(u"(?<![\\p{L}\\p{N}_])(?:") + p + u")(?![\\p{L}\\p{N}_])";
    return std::unique_ptr<icu::RegexPattern>(icu::RegexPattern::compile(p, UREGEX_MULTILINE | (o.caseSensitive ? 0 : UREGEX_CASE_INSENSITIVE), status));
}
icu::UnicodeString expand(const std::u16string &r, icu::RegexMatcher &m, const icu::RegexPattern &p, UErrorCode &status) {
    icu::UnicodeString out;
    for (size_t i = 0; i < r.size(); ++i) {
        if (r[i] != u'$' || i + 1 == r.size()) { out += r[i]; continue; }
        const auto next = r[i+1];
        if (next == u'$') { out += u'$'; ++i; }
        else if (next >= u'0' && next <= u'9') {
            int group = 0;
            while (i + 1 < r.size() && r[i+1] >= u'0' && r[i+1] <= u'9') {
                group = group > m.groupCount() ? m.groupCount()+1 : group*10 + (r[i+1]-u'0'); ++i;
            }
            if (group <= m.groupCount()) out += m.group(group, status);
        } else if (next == u'{') {
            const auto end = r.find(u'}', i+2);
            if (end == r.npos) { out += u'$'; continue; }
            UErrorCode lookup = U_ZERO_ERROR;
            const int group = p.groupNumberFromName(unicode(r.substr(i+2, end-i-2)), lookup);
            if (U_SUCCESS(lookup)) out += m.group(group, status);
            i = end;
        } else out += u'$';
    }
    return out;
}
}
SearchResult Search::find(const std::u16string &s, const SearchOptions &o, int limit) {
    SearchResult result;
    if (o.pattern.empty()) return result;
    UErrorCode status = U_ZERO_ERROR;
    auto p = compile(o, status);
    if (U_FAILURE(status)) { result.error = u_errorName(status); return result; }
    const auto input = unicode(s);
    std::unique_ptr<icu::RegexMatcher> m(p->matcher(input, status));
    while (U_SUCCESS(status) && m->find(status)) {
        if (static_cast<int>(result.matches.size()) >= limit) { result.truncated = true; break; }
        const int start = m->start(status);
        result.matches.push_back({start, m->end(status)-start});
    }
    if (U_FAILURE(status)) result.error = u_errorName(status);
    return result;
}
std::u16string Search::replacementForMatch(const std::u16string &s, const SearchOptions &o,
    const std::u16string &r, int index, std::string *error) {
    error->clear();
    if (!o.regex) return r;
    UErrorCode status = U_ZERO_ERROR;
    auto p = compile(o, status);
    if (U_FAILURE(status)) { *error = u_errorName(status); return {}; }
    const auto input = unicode(s);
    std::unique_ptr<icu::RegexMatcher> m(p->matcher(input, status));
    for (int i = 0; U_SUCCESS(status) && m->find(status); ++i) {
        if (i != index) continue;
        const auto expanded = expand(r, *m, *p, status);
        if (U_FAILURE(status)) { *error = u_errorName(status); return {}; }
        return text(expanded);
    }
    *error = U_FAILURE(status) ? u_errorName(status) : "Search match no longer exists";
    return {};
}
std::u16string Search::replaceAll(const std::u16string &s, const SearchOptions &o, const std::u16string &r, int *count, std::string *error) {
    *count = 0; error->clear();
    if (o.pattern.empty()) return s;
    UErrorCode status = U_ZERO_ERROR;
    auto p = compile(o, status);
    if (U_FAILURE(status)) { *error = u_errorName(status); return s; }
    const auto input = unicode(s);
    std::unique_ptr<icu::RegexMatcher> m(p->matcher(input, status));
    icu::UnicodeString output;
    int last = 0;
    while (U_SUCCESS(status) && m->find(status)) {
        const int start = m->start(status), end = m->end(status);
        output += input.tempSubStringBetween(last, start);
        output += o.regex ? expand(r, *m, *p, status) : unicode(r);
        last = end; ++*count;
    }
    if (U_FAILURE(status)) { *error = u_errorName(status); *count = 0; return s; }
    output += input.tempSubString(last);
    return text(output);
}
}
