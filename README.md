# NyaEditCore

NyaEdit 的独立编辑器后端，构建目标为 `nya_core`。仅依赖 C++17 标准库和 ICU ≥ 60；构建及测试均不需要 Qt、窗口或事件循环。

- `EncodingService`：编码检测、BOM、严格编码与解码。
- `loadFile` / `saveFile`：文件读取、换行分析与归一化、同目录临时文件及原子替换。编码或提交失败时保留目标文件；读取上限为 128 MiB。
- `Search`：ICU Unicode 正则及字面量搜索、整词搜索、捕获替换；支持 `$1`、`${name}`、`$$`。
- `FileSearch`：可取消的递归路径搜索，支持子串与有序模糊匹配，跳过版本控制元数据目录与目录符号链接。

## 接口约定

公开头文件位于 `include/nya/core/`，命名空间为 `nya::core`。正文与搜索模式使用 `std::u16string`，搜索位置使用 UTF-16 码元偏移；字节、编码名称和 UTF-8 错误信息使用 `std::string`；文件路径与修改时间使用 `std::filesystem` 类型。各次调用的 ICU 状态独立，可在调用方线程池中运行。

后端不管理配置偏好、界面信号、进程事件循环或文本视图。它保留 U+2028/U+2029；前端自行决定能否无损显示。正则语法由 ICU 定义。

NyaEdit 的 Qt 类型转换、`AppStorage`、`LspClient` 和基于 `QProcess` 的 Tree-sitter 调度已移至父项目 `src/qt`，构建目标为 `nya_qt`。原有 Qt 测试也由父项目维护。

## 独立构建与测试

需要 CMake ≥ 3.21、C++17 编译器和 ICU ≥ 60。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

ICU 不在默认路径时可使用 `-DCMAKE_PREFIX_PATH=ICU安装目录`。只构建库可加 `-DBUILD_TESTING=OFF`。

## 集成

```cmake
add_subdirectory(path/to/NyaEditCore)
target_link_libraries(your_target PRIVATE nya_core)
```

```cpp
#include <nya/core/Search.h>
auto result = nya::core::Search::find(u"你好，世界", {u"世界"});
```

本仓库作为 NyaEdit 的 `src/core` Git 子模块使用，修改需同时保留子模块工作区和父项目的适配层变更。
