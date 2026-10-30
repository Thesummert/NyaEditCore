# NyaEditCore

NyaEdit 的独立 C++ 核心库，构建目标为 `nya_core`。依赖 Qt Core 与 ICU，不依赖 Widgets，可以用于 CLI、其他原生前端或后台任务。

- `EncodingService`：编码检测、严格编码与解码。
- `loadFile` / `saveFile`：文件读取、换行分析与原子保存。
- `Search`：Unicode 文本搜索、正则表达式与替换。
- `FileSearch`：可取消的递归文件名和路径搜索。
- `AppStorage`：跨平台配置与文件 / 文件夹历史。
- `TreeSitter`：可选的系统 Tree-sitter CLI 调用与高亮区间解析，无需 Tree-sitter 开发库。
- `LspClient`：通过 stdio 连接语言服务器，提供异步 JSON-RPC、文档同步、补全 / 悬停 / 定义请求和诊断信号。

## 独立构建与测试

需要 CMake ≥ 3.21、C++17 编译器、Qt ≥ 5.15（推荐 Qt 6）和 ICU ≥ 60。默认启用测试，额外需要 Qt Test。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

若 Qt / ICU 不在默认搜索路径，通过 `-DCMAKE_PREFIX_PATH="Qt安装目录;ICU安装目录"` 指定。只构建库可加 `-DBUILD_TESTING=OFF`，此时不需要 Qt Test。

测试涵盖编码与文件读写、搜索、配置与历史持久化、Tree-sitter CLI 的调度，以及 LSP 初始化、分包 / 连包、Unicode 文档同步、请求取消 / 超时、协议错误和进程退出。LSP 测试使用测试程序自身作为模拟服务器，无需安装语言服务器；可设置 `NYA_TEST_CLANGD_EXECUTABLE` 运行真实 clangd 初始化和悬停测试。可设置 `NYA_TEST_TREE_SITTER_EXECUTABLE`、`NYA_TEST_TREE_SITTER_CONFIG` 和 `NYA_TEST_TREE_SITTER_SUFFIX` 运行真实 Tree-sitter CLI 集成测试；未设置时跳过这些可选用例。

## 集成

```cmake
add_subdirectory(path/to/NyaEditCore)
target_link_libraries(your_target PRIVATE nya_core)
```

头文件搜索路径、C++17、Qt Core 与 ICU 链接依赖会自动传递给使用方，例如 `#include "Encoding.h"`。如父项目已选择 Qt 主版本，核心使用相同版本。

## 简单 LSP 接口

`LspClient.h` 提供一个语言服务器进程对应一个客户端的接口；服务器需另行安装，命令及参数由调用方指定，不经过 shell。所有调用在客户端所属 QObject 线程执行，需要 Qt 事件循环。`start()` 返回是否接受启动，收到 `ready()` 后才能发送业务请求；初始化结果可通过 `serverCapabilities()` 查询。

```cpp
#include "LspClient.h"

auto *lsp = new nya::LspClient(owner); // owner 为调用方的 QObject
const auto uri = QUrl::fromLocalFile("/path/to/project/main.cpp");
QObject::connect(lsp, &nya::LspClient::ready, lsp, [lsp, uri] {
    lsp->openDocument(uri, "cpp", "int main() { return 0; }\n", 1);
    const auto id = lsp->hover(uri, 0, 4);
    // 保存 id，与 responseReceived / requestFailed 对应。
    Q_UNUSED(id);
});
QObject::connect(lsp, &nya::LspClient::responseReceived, lsp,
    [](qint64 id, const QJsonValue &result) { /* 由前端解释 LSP 结果 */ });
QObject::connect(lsp, &nya::LspClient::diagnosticsPublished, lsp,
    [](const QUrl &uri, const QJsonArray &diagnostics) { /* 前端显示诊断 */ });
QObject::connect(lsp, &nya::LspClient::failed, lsp,
    [](const QString &message) { /* 启动、协议或进程错误 */ });

nya::LspServerOptions options;
options.program = "/path/to/clangd";
options.rootPath = "/path/to/project";
lsp->start(options);
```

- `openDocument` / `changeDocument` / `saveDocument` / `closeDocument` 管理同步。文本使用 LF，版本号由调用方递增；URI 使用 `QUrl::fromLocalFile()` 转换本地路径。仅在实际保存成功后调用 `saveDocument`。客户端按服务端静态 `textDocumentSync` 能力发送通知；全量模式发送全文，增量模式发送覆盖旧全文范围的单次编辑，不做差异计算。需要保留已打开文档的文本快照。
- `completion` / `hover` / `definition` 返回请求 ID，行列均从 0 开始，列按 UTF-16 单元计数，与 QString 一致。通用 `request(method, params)` / `notify(method, params)` 可用于其他协议方法；结果保留 LSP 原始 JSON 对象、数组或 null，由前端解释。能力是否适用、结果与当前文档版本是否一致，由调用方判断。
- 请求无法入队时返回 `-1`，通知 / 文档操作失败返回 `false`。已入队请求通过 `responseReceived` 或 `requestFailed` 完成；错误信号保留 JSON-RPC 的 `code`、`message` 和可选 `data`。本地停止为 `-32000`，超时为 `-32001`，主动取消为 `-32800`。`cancelRequest(id)` 发送取消通知并忽略迟到结果。
- `shutdown()` 执行 `shutdown → exit`，默认 1.5 秒后强制结束；初始化期间调用会直接停止。`stop()` 立即终止，未完成业务请求会报告失败。销毁也会清理当前进程；重新启动需在停止后调用 `start()` 并重新打开文档。
- 默认每个请求及启动限时 10 秒，可用 `requestTimeoutMs` 调整。单条消息体上限 16 MiB，消息头上限 8 KiB，待发送内容约 32 MiB；stderr 单独通过 `serverLog` 输出。可指定工作目录、进程环境及 `initializationOptions`。

此接口使用 [LSP 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) 的 stdio / Content-Length 消息格式，仅协商 UTF-16 位置编码。暂不支持 TCP、动态能力注册、服务端发起的工作区编辑或 UI 交互；服务端请求统一返回 `MethodNotFound`，服务端通知通过 `notificationReceived` 转发。核心不自动安装服务器，也不提供补全弹窗、诊断绘制或跳转 UI。

在 NyaEdit 中，本仓库作为 `src/core` 子模块使用。修改核心后先提交并推送本仓库，再在父仓库提交更新的子模块版本。
