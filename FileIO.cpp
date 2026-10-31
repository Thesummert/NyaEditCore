#include <nya/core/FileIO.h>
#include <fstream>
#include <cstdio>
#include <random>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif
namespace nya::core {
namespace fs = std::filesystem;
std::string lineEndingName(LineEnding e) { return e == LineEnding::CRLF ? "CRLF" : e == LineEnding::CR ? "CR" : "LF"; }
LoadedFile loadFile(const fs::path &path, const std::string &forced) {
    LoadedFile r;
    std::error_code ec;
    r.path = fs::canonical(path, ec);
    if (ec) { ec.clear(); r.path = fs::absolute(path, ec); }
    std::ifstream file(r.path, std::ios::binary);
    if (!file) { r.error = "Cannot open file"; return r; }
    auto size = fs::file_size(r.path, ec);
    if (ec || size > MaxFileBytes) { r.error = "Cannot read file or file exceeds 128 MiB"; return r; }
    // Read one extra byte to detect files that grow while being read.
    std::string bytes(static_cast<size_t>(size)+1, '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto read = file.gcount();
    if (file.bad() || read > static_cast<std::streamsize>(size)) { r.error = "File changed or read failed"; return r; }
    bytes.resize(static_cast<size_t>(read)); r.byteSize = read;
    r.modified = fs::last_write_time(r.path, ec);
    auto decoded = forced.empty() ? EncodingService::detect(bytes) : EncodingService::decode(bytes, {forced});
    if (!decoded.ok()) { r.error = decoded.error; return r; }
    r.encoding = decoded.encoding;
    int crlf = 0, lf = 0, cr = 0;
    for (size_t i = 0; i < decoded.text.size(); ++i) {
        const auto ch = decoded.text[i];
        if (ch == u'\r') {
            if (i+1 < decoded.text.size() && decoded.text[i+1] == u'\n') { ++crlf; ++i; } else ++cr;
            r.text += u'\n';
        } else { if (ch == u'\n') ++lf; r.text += ch; }
    }
    r.ending = crlf > 0 && crlf >= lf && crlf >= cr ? LineEnding::CRLF : cr > lf ? LineEnding::CR : LineEnding::LF;
    r.mixedEndings = int(crlf > 0) + int(cr > 0) + int(lf > 0) > 1;
    return r;
}
bool saveFile(const fs::path &requestedPath, const std::u16string &text, const Encoding &encoding, LineEnding ending, std::string *error) {
    error->clear();
    std::error_code pathError;
    auto path = requestedPath;
    if (fs::is_symlink(path, pathError)) {
        path = fs::canonical(path, pathError);
        if (pathError) { *error = pathError.message(); return false; }
    }
    std::u16string output;
    for (auto ch : text) {
        if (ch == u'\n' && ending != LineEnding::LF) {
            output += u'\r'; if (ending == LineEnding::CRLF) output += u'\n';
        } else output += ch;
    }
    const auto bytes = EncodingService::encode(output, encoding, error);
    if (!error->empty()) return false;
    fs::path temporary;
    FILE *file = nullptr;
    std::random_device random;
    for (int attempt = 0; attempt < 32 && !file; ++attempt) {
        temporary = path; temporary += ".nya-save-" + std::to_string(random());
#ifdef _WIN32
        file = _wfopen(temporary.c_str(), L"wbx");
#else
        file = std::fopen(temporary.c_str(), "wbx");
#endif
        if (!file && errno != EEXIST) break;
    }
    if (!file) { *error = std::strerror(errno); return false; }
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    const bool closed = std::fclose(file) == 0;
    std::error_code ec;
    if (written && closed) {
        auto permissions = fs::status(path, ec).permissions();
        if (!ec) fs::permissions(temporary, permissions, ec);
        if (!ec || !fs::exists(path)) {
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                ec = std::error_code(GetLastError(), std::system_category());
            else ec.clear();
#else
            fs::rename(temporary, path, ec);
#endif
            if (!ec) return true;
        }
    }
    *error = ec ? ec.message() : "File write failed";
    fs::remove(temporary, ec);
    return false;
}
}
