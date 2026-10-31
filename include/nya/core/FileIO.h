#pragma once
#include "Encoding.h"
#include <filesystem>
#include <cstdint>
namespace nya::core {
enum class LineEnding { LF, CRLF, CR };
std::string lineEndingName(LineEnding ending);
struct LoadedFile {
    std::filesystem::path path;
    std::u16string text;
    Encoding encoding;
    LineEnding ending = LineEnding::LF;
    bool mixedEndings = false;
    std::filesystem::file_time_type modified{};
    std::int64_t byteSize = 0;
    std::string error;
    bool ok() const { return error.empty(); }
};
constexpr std::int64_t MaxFileBytes = 128LL * 1024 * 1024;
LoadedFile loadFile(const std::filesystem::path &path, const std::string &forcedEncoding = {});
bool saveFile(const std::filesystem::path &path, const std::u16string &text, const Encoding &encoding,
              LineEnding ending, std::string *error);
}
