#pragma once
#include <string>
#include <vector>
namespace nya::core {
// Text and offsets use UTF-16 code units; encoded bytes and codec names use std::string.
struct Encoding {
    std::string name = "UTF-8";
    bool bom = false;
    int confidence = 100;
    bool detected = false;
    std::string label() const;
};
struct DecodeResult {
    std::u16string text;
    Encoding encoding;
    std::string error;
    bool ok() const { return error.empty(); }
};
class EncodingService {
public:
    static std::vector<std::string> supported();
    static DecodeResult detect(const std::string &bytes);
    static DecodeResult decode(const std::string &bytes, const Encoding &encoding);
    static std::string encode(const std::u16string &text, const Encoding &encoding, std::string *error);
    static std::string bomFor(const std::string &name);
};
}
