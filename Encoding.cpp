#include <nya/core/Encoding.h>

#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/ucsdet.h>
#include <memory>
#include <algorithm>
#include <cctype>

namespace nya::core {
namespace {
using Converter = std::unique_ptr<UConverter, decltype(&ucnv_close)>;

std::string canonical(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
    if (name == "GB2312" || name == "GBK") return "GB18030";
    if (name == "SHIFT_JIS" || name == "SJIS") return "Shift_JIS";
    if (name == "WINDOWS-1252" || name == "ISO-8859-1") { std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); }); return name; }
    return name;
}

bool binaryText(const std::u16string &text)
{
    int control = 0;
    const int count = std::min(8192, int(text.size()));
    for (int i = 0; i < count; ++i) {
        const char16_t c = text.at(i);
        if (c == 0) return true;
        if (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != '\f') ++control;
    }
    return control > std::max(2, count / 100);
}
}

std::string Encoding::label() const
{
    return name + (bom ? " BOM" : std::string());
}

std::vector<std::string> EncodingService::supported()
{
    return {"UTF-8", "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE", "GB18030",
            "Big5", "Shift_JIS", "EUC-JP", "EUC-KR", "windows-1252", "windows-1251", "ISO-8859-1"};
}

std::string EncodingService::bomFor(const std::string &name)
{
    std::string upper = canonical(name);
    if (upper == "UTF-8") return std::string("\xef\xbb\xbf", 3);
    if (upper == "UTF-16LE") return std::string("\xff\xfe", 2);
    if (upper == "UTF-16BE") return std::string("\xfe\xff", 2);
    if (upper == "UTF-32LE") return std::string("\xff\xfe\x00\x00", 4);
    if (upper == "UTF-32BE") return std::string("\x00\x00\xfe\xff", 4);
    return {};
}

DecodeResult EncodingService::decode(const std::string &bytes, const Encoding &encoding)
{
    DecodeResult result;
    result.encoding = encoding;
    UErrorCode status = U_ZERO_ERROR;
    Converter converter(ucnv_open(encoding.name.c_str(), &status), ucnv_close);
    if (U_FAILURE(status)) {
        result.error = "不支持的编码：" + encoding.name;
        return result;
    }
    ucnv_setToUCallBack(converter.get(), UCNV_TO_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);
    const std::string bom = bomFor(encoding.name);
    const int offset = !bom.empty() && bytes.compare(0, bom.size(), bom) == 0 ? int(bom.size()) : 0;
    result.encoding.bom = offset != 0;
    const char *source = bytes.data() + offset;
    const int length = int(bytes.size()) - offset;
    int32_t required = ucnv_toUChars(converter.get(), nullptr, 0, source, length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        result.error = "文件包含无效的字节序列，编码：" + encoding.name;
        return result;
    }
    status = U_ZERO_ERROR;
    result.text.resize(required);
    ucnv_toUChars(converter.get(), reinterpret_cast<UChar *>(result.text.data()), required,
                 source, length, &status);
    if (U_FAILURE(status)) result.error = "解码失败：" + std::string(u_errorName(status));
    return result;
}

DecodeResult EncodingService::detect(const std::string &bytes)
{
    // Check longer BOMs first: UTF-32LE begins with the UTF-16LE BOM.
    const std::vector<std::string> unicode = {"UTF-32LE", "UTF-32BE", "UTF-8", "UTF-16LE", "UTF-16BE"};
    for (const std::string &name : unicode) {
        if (bytes.compare(0, bomFor(name).size(), bomFor(name)) == 0) {
            auto result = decode(bytes, {name, true, 100, true});
            if (result.ok() && binaryText(result.text)) result.error = "该文件包含二进制内容，无法作为文本打开。";
            return result;
        }
    }
    auto utf8 = decode(bytes, {"UTF-8", false, 100, true});
    if (utf8.ok() && !binaryText(utf8.text)) return utf8;

    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UCharsetDetector, decltype(&ucsdet_close)> detector(ucsdet_open(&status), ucsdet_close);
    if (U_SUCCESS(status)) {
        // Detection is bounded; every proposed codec is validated against the entire file.
        ucsdet_setText(detector.get(), bytes.data(), std::min(int(bytes.size()), 256 * 1024), &status);
        int32_t count = 0;
        const UCharsetMatch **matches = ucsdet_detectAll(detector.get(), &count, &status);
        for (int i = 0; U_SUCCESS(status) && i < count; ++i) {
            const std::string name = canonical(std::string(ucsdet_getName(matches[i], &status)));
            const int confidence = ucsdet_getConfidence(matches[i], &status);
            if (U_FAILURE(status)) break;
            auto result = decode(bytes, {name, false, confidence, true});
            if (result.ok() && !binaryText(result.text)) return result;
        }
    }
    DecodeResult result;
    result.error = "无法可靠解码此文件，或文件包含二进制内容。";
    return result;
}

std::string EncodingService::encode(const std::u16string &text, const Encoding &encoding, std::string *error)
{
    error->clear();
    UErrorCode status = U_ZERO_ERROR;
    Converter converter(ucnv_open(encoding.name.c_str(), &status), ucnv_close);
    if (U_FAILURE(status)) {
        *error = "不支持的编码：" + encoding.name;
        return {};
    }
    ucnv_setFallback(converter.get(), false);
    ucnv_setFromUCallBack(converter.get(), UCNV_FROM_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);
    const auto *source = reinterpret_cast<const UChar *>(text.data());
    const int32_t required = ucnv_fromUChars(converter.get(), nullptr, 0, source, int(text.size()), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        *error = "文本包含当前编码无法表示的字符：" + encoding.name;
        return {};
    }
    status = U_ZERO_ERROR;
    std::string bytes(required, '\0');
    ucnv_fromUChars(converter.get(), bytes.data(), required, source, int(text.size()), &status);
    if (U_FAILURE(status)) {
        *error = "编码失败：" + std::string(u_errorName(status));
        return {};
    }
    if (encoding.bom) bytes.insert(0, bomFor(encoding.name));
    return bytes;
}
}
