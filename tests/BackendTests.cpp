#include <nya/core/Encoding.h>
#include <nya/core/FileIO.h>
#include <nya/core/FileSearch.h>
#include <nya/core/Search.h>
#include <iostream>
#include <stdexcept>
#include <random>
using namespace nya::core;
namespace fs = std::filesystem;
void check(bool ok, const char *what) { if (!ok) throw std::runtime_error(what); }
struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        std::random_device random;
        for (int i = 0; i < 32; ++i) {
            path = fs::temp_directory_path() / ("nya-backend-test-" + std::to_string(random()));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("Cannot create test directory");
    }
    ~TemporaryDirectory() { std::error_code ec; fs::remove_all(path, ec); }
};
int main() {
    try {
        std::string error;
        const std::u16string sample = u"NyaEdit 你好 日本語 🌙\nsecond line\n";
        for (const auto &name : {"UTF-8", "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE", "GB18030"}) {
            for (bool bom : {false, true}) {
                const auto bytes = EncodingService::encode(sample, {name, bom}, &error);
                check(error.empty(), "encode unicode");
                const auto decoded = EncodingService::decode(bytes, {name});
                check(decoded.ok() && decoded.text == sample, "decode round trip");
                if (bom && !EncodingService::bomFor(name).empty()) {
                    const auto detected = EncodingService::detect(bytes);
                    check(detected.ok() && detected.text == sample && detected.encoding.name == name, "BOM detection");
                }
            }
        }
        check(!EncodingService::decode(std::string("\xc3\x28",2), {}).ok(), "invalid UTF-8");
        EncodingService::encode(sample, {"windows-1252"}, &error);
        check(!error.empty(), "unrepresentable text");
        check(!EncodingService::detect(std::string(100, '\0')).ok(), "binary rejection");
        check(EncodingService::detect({}).ok(), "empty input");
        auto found = Search::find(u"🌙你好\n你好", {u"你好"});
        check(found.matches.size() == 2 && found.matches[0].start == 2 && found.matches[1].start == 5, "UTF-16 offsets");
        check(Search::find(u"café caféine CAFÉ", {u"café", false, true}).matches.size() == 2, "Unicode whole words");
        check(!Search::find(u"abc", {u"[", false, false, true}).error.empty(), "invalid regex");
        check(Search::find(u"aaa", {u"a"}, 2).truncated, "search limit");
        int count = 0;
        auto replaced = Search::replaceAll(u"width=12 height=34", {u"(?<key>\\w+)=(\\d+)", true, false, true}, u"${key}:$2:$$", &count, &error);
        check(error.empty() && count == 2 && replaced == u"width:12:$ height:34:$", "regex replacement");
        auto one = Search::replacementForMatch(u"width=12 height=34", {u"(?<key>\\w+)=(\\d+)", true, false, true}, u"${key}:$2:$$", 1, &error);
        check(error.empty() && one == u"height:34:$", "single replacement uses search engine");
        replaced = Search::replaceAll(u"one\ntwo", {u"^", true, false, true}, u"> ", &count, &error);
        check(error.empty() && count == 2 && replaced == u"> one\n> two", "zero width matches");
        replaced = Search::replaceAll(u"a.b", {u"a.b"}, u"$1", &count, &error);
        check(replaced == u"$1", "literal replacement");
        TemporaryDirectory dir;
        const auto path = dir.path / fs::path(u"中文🌙.txt");
        for (auto ending : {LineEnding::LF, LineEnding::CRLF, LineEnding::CR}) {
            check(saveFile(path, sample, {"UTF-16LE", true}, ending, &error), "save file");
            auto loaded = loadFile(path);
            check(loaded.ok() && loaded.text == sample && loaded.ending == ending, "load file");
        }
        check(!saveFile(path, sample, {"windows-1252"}, LineEnding::LF, &error), "failed encoding save");
        check(loadFile(path).text == sample, "failed save preserves original");
        check(!saveFile(dir.path, sample, {}, LineEnding::LF, &error), "failed commit");
        check(!loadFile(dir.path / "missing").ok(), "missing file");
        check(!saveFile(dir.path / "missing/sub/file", sample, {}, LineEnding::LF, &error), "missing parent save");
        for (const auto &entry : fs::directory_iterator(dir.path)) {
            check(entry.path().filename().string().find(".nya-save-") == std::string::npos, "failed commit removes temporary file");
        }
#ifndef _WIN32
        fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write);
        const auto link = dir.path / "linked.txt";
        fs::create_symlink(path, link);
        check(saveFile(link, sample, {}, LineEnding::LF, &error), "save through symlink");
        check(fs::is_symlink(link) && loadFile(path).text == sample, "preserve symlink and update target");
        check(fs::status(path).permissions() == (fs::perms::owner_read | fs::perms::owner_write), "preserve permissions");
        fs::remove(link);
#endif
        check(saveFile(path, u"one\r\ntwo\rthree\n", {}, LineEnding::LF, &error), "mixed fixture");
        auto mixed = loadFile(path);
        check(mixed.ok() && mixed.mixedEndings && mixed.text == u"one\ntwo\nthree\n", "mixed endings");
        check(saveFile(path, u"a\u2028b\u2029c", {}, LineEnding::LF, &error), "separator save");
        check(loadFile(path).text == u"a\u2028b\u2029c", "backend preserves paragraph separators");
        fs::create_directories(dir.path / "src/ui");
        fs::create_directories(dir.path / ".git");
        check(saveFile(dir.path / "src/ui/MainWindow.cpp", u"", {}, LineEnding::LF, &error), "search fixture");
        check(saveFile(dir.path / ".git/MainWindow.cpp", u"", {}, LineEnding::LF, &error), "ignored fixture");
        auto files = FileSearch::findFiles(dir.path, u" S\\U\\MW ");
        check(files.error.empty() && files.matches.size() == 1, "fuzzy path search");
        check(FileSearch::findFiles(dir.path, u"WINDOW", {}, 100, FileSearchMode::Substring).matches.size() == 1, "substring path search");
        check(FileSearch::findFiles(dir.path, u"🌙").matches.size() == 1, "supplementary path search");
        check(FileSearch::findFiles(dir.path, u".", {}, 1).truncated, "file search limit");
        check(FileSearch::findFiles(dir.path, u"a", std::make_shared<std::atomic_bool>(true)).canceled, "cancellation");
        std::error_code ec;
        fs::create_directory_symlink(dir.path, dir.path / "loop", ec);
        check(FileSearch::findFiles(dir.path, u"WINDOW").matches.size() == 1, "directory symlink skipped");
        std::cout << "Backend checks passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
