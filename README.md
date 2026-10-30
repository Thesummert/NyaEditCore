# NyaEditCore

NyaEdit 的独立 C++ 核心库，构建目标为 `nya_core`。依赖 Qt Core 与 ICU，不依赖 Widgets，可以用于 CLI、其他原生前端或后台任务。

- `EncodingService`：编码检测、严格编码与解码。
- `loadFile` / `saveFile`：文件读取、换行分析与原子保存。
- `Search`：Unicode 文本搜索、正则表达式与替换。
- `FileSearch`：可取消的递归文件名和路径搜索。
- `AppStorage`：跨平台配置与文件 / 文件夹历史。
- `TreeSitter`：可选的系统 Tree-sitter CLI 调用与高亮区间解析，无需 Tree-sitter 开发库。

## 独立构建与测试

需要 CMake ≥ 3.21、C++17 编译器、Qt ≥ 5.15（推荐 Qt 6）和 ICU ≥ 60。默认启用测试，额外需要 Qt Test。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

若 Qt / ICU 不在默认搜索路径，通过 `-DCMAKE_PREFIX_PATH="Qt安装目录;ICU安装目录"` 指定。只构建库可加 `-DBUILD_TESTING=OFF`，此时不需要 Qt Test。

测试涵盖编码与文件读写、搜索、配置与历史持久化，以及 Tree-sitter CLI 的调度、取消、超时和高亮位置。可设置 `NYA_TEST_TREE_SITTER_EXECUTABLE`、`NYA_TEST_TREE_SITTER_CONFIG` 和 `NYA_TEST_TREE_SITTER_SUFFIX` 运行真实 CLI 集成测试；未设置时跳过这些可选用例。

## 集成

```cmake
add_subdirectory(path/to/NyaEditCore)
target_link_libraries(your_target PRIVATE nya_core)
```

头文件搜索路径、C++17、Qt Core 与 ICU 链接依赖会自动传递给使用方，例如 `#include "Encoding.h"`。如父项目已选择 Qt 主版本，核心使用相同版本。

在 NyaEdit 中，本仓库作为 `src/core` 子模块使用。修改核心后先提交并推送本仓库，再在父仓库提交更新的子模块版本。
