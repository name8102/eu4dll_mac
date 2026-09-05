# eu4dll_multiplatform

[![Build and Test](https://github.com/name8102/eu4dll_multiplatform/actions/workflows/build-test.yml/badge.svg)](https://github.com/name8102/eu4dll_multiplatform/actions/workflows/build-test.yml)
[下载 / Releases](https://github.com/name8102/eu4dll_multiplatform/releases) · [Linux 验证记录](docs/LINUX_ADAPTATION.zh-CN.md) · [问题反馈](https://github.com/name8102/eu4dll_multiplatform/issues)

为 Europa Universalis IV 的 macOS 与 Linux 版本提供中文等多字节文本支持，包括显示、输入、拼音搜索和东亚姓名顺序。项目基于 [原始 EU4dll](https://github.com/matanki-saito/EU4dll) 及其 [macOS 实现](https://github.com/PoXiao-zero/eu4dll_mac)，将共享功能与各平台补丁适配分开维护。

Native localization patches for EU4 on macOS and Linux. Both packages use **`bash install.sh`**; the installer detects the operating system and asks for the game path when omitted.

> 当前版本处于预发布阶段。Linux 的本轮功能已由用户在游戏内确认正常；长期战役游玩、云存档/自动存档及更多模组组合仍待验证。

## 支持范围

| 平台 | 游戏与系统 | 当前状态 |
| --- | --- | --- |
| Linux | 原生 EU4 **1.37.5 x86-64**，启动前校验确切 ELF | 中文显示、输入编辑、拼音搜索、本地存档名、日期与姓名已完成本地验证及用户验收 |
| macOS | EU4 **1.37.x x86-64**；发布产物最低 macOS **11.0** | 保留 macOS 实现；云端编译与测试通过；1.37.4/1.37.5 有只读二进制预检证据 |
| Windows | 上游已有原始实现 | 本仓库暂不提供 Windows 构建与安装包 |

Linux 安装需要 **Python 3.11+**。云端 Linux 包基于 Ubuntu 24.04 构建，需要兼容的 glibc/libstdc++；精确依赖版本记录在包内 `BUILD_INFO.txt`。更旧发行版可尝试本机构建，不在预编译包的验证范围内。

Linux 输入法依赖支持输入法的系统 SDL2；目前实机验证组合为 **X11 + Fcitx5**。补丁本身不提供游戏、字体或汉化文本，仍需启用适用的中文字体/本地化模组。其他版本、架构及所有模组组合不保证适用。

## 功能

- 加载 UTF-8 BOM 本地化文件；支持 UI、提示、换行、地图及 3D 文本。
- Linux 中文输入、完整字符移动/删除、复制/剪切/粘贴。
- 中文、拼音全拼及首字母搜索，由 [cpp-pinyin](https://github.com/wolfgitpr/cpp-pinyin) 提供拼音数据。
- 本地中文存档名，以及列表、确认框、存档详情和继续游戏提示的显示。
- 年月日日期格式，按文化规则调整东亚人物姓氏顺序。

原项目通过 `¿` 字符手动反转姓名的功能尚未实现。Linux 编辑保留游戏原有的字节长度上限，但会在完整字符边界截断。

## 安装

1. 退出游戏，从 [Releases](https://github.com/name8102/eu4dll_multiplatform/releases) 下载对应平台的 ZIP：
   - Linux：`eu4dll-linux-x86_64.zip`
   - macOS：`eu4dll-macos-x86_64.zip`
2. 将完整压缩包解压到**游戏目录之外**，保留字典和脚本等全部文件。
3. 在解压目录打开终端，两个平台都运行：

   ```sh
   bash install.sh
   ```

   按提示输入游戏路径。也可以直接传入路径：

   ```sh
   # Linux：包含 eu4 可执行文件的游戏目录
   bash install.sh '/path/to/Europa Universalis IV'

   # macOS：eu4.app
   bash install.sh '/path/to/eu4.app'
   ```

4. 安装成功后，按平时方式启动游戏并启用相应本地化模组。

脚本在安装前检查游戏和安装材料。macOS 保留原可执行文件和安装状态；Linux 保留 `eu4.orig`，并将上一版补丁材料备份到游戏目录下的 `eu4dll-backups/<时间>/`。安装失败会尝试恢复本次修改，并在恢复未完成时报告备份位置。

无需对整个游戏 App 重新签名或清除扩展属性。发布时的 macOS 产物签名与修改用户游戏 App 是不同的操作。

如下载了 `SHA256SUMS.txt`，可校验对应 ZIP：

```sh
# Linux
sha256sum eu4dll-linux-x86_64.zip
# macOS
shasum -a 256 eu4dll-macos-x86_64.zip
```

将输出与校验文件中同名文件的值对照。

### 更新与恢复

更新时退出游戏，解压新版到另一个目录，再次运行 `bash install.sh`。

- macOS：检测到已安装补丁时，可选择更新/修复或卸载。也支持 `EU4DLL_ACTION=uninstall bash install.sh '/path/to/eu4.app'`。
- Linux：统一入口目前提供安装/更新；可用备份恢复上一版补丁。停用补丁可在退出游戏后直接启动保留的 `eu4.orig`，或通过游戏平台验证文件恢复原始启动器。不要把补丁启动器作为原始游戏文件备份。

## 常见问题

**Linux 无法激活输入法。** 启动器默认使用 `SDL_DYNAMIC_API=libSDL2-2.0.so.0` 与 X11。确认系统已安装 SDL2 和输入法；可用 `SDL_DYNAMIC_API=/absolute/path/to/libSDL2-2.0.so.0` 指定库。实机验证为 Fcitx5；Wayland 与其他输入法尚未完成验证。

**启动时拒绝安装补丁。** 检查游戏版本和平台是否在上表范围内，以及下载包是否完整。Linux 会核验 ELF 哈希；不要通过开发用的跳过校验开关强行加载未知版本。

**文字仍乱码或字体缺失。** 确认字体与本地化模组已启用，更新后完整重启游戏。补丁不包含汉化内容。

**提交问题时需要什么。** 请提供操作系统、游戏版本、Release 版本、模组列表、复现步骤和必要截图。Linux 启动日志默认位于 `${XDG_STATE_HOME:-$HOME/.local/state}/eu4dll_linux/eu4dll.log`；另附相关游戏错误日志即可，无需上传游戏程序。

## 从源码构建

需要 CMake **3.27+**、C++17 编译器及 Git。首次配置会获取固定版本 cpp-pinyin 1.0.2。Linux 可使用 GCC/Clang，macOS 使用 Xcode Command Line Tools。

```sh
# Linux
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel 4
ctest --test-dir build-linux --output-on-failure
bash install.sh '/path/to/Europa Universalis IV'

# macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4 --target eu4dll_tests eu4dll_manifest_tool
ctest --test-dir build --output-on-failure
bash install.sh '/path/to/eu4.app'
```

Linux 自定义构建目录可传入 `--build-dir /path/to/build`。macOS 源码安装默认读取 `build/`，仍保留已有 `EU4DLL_DYLIB_SOURCE`、`EU4DLL_INSERT_TOOL`、`EU4DLL_MANIFEST_TOOL`、`EU4DLL_DICT_SOURCE` 覆盖选项。

## 验证与发布

GitHub Actions 在 `main` 推送、拉取请求和手动触发时执行 macOS Intel 与 Linux x86-64 编译、CTest、产物检查和分平台打包。Linux 对项目代码启用严格编译警告。

云端不包含专有游戏程序，不能替代实机验证。本地 Linux 探针执行实际 EU4 ELF 的安装补丁和调用路径，但不加载战役；日期测试使用月份文本替身。具体已验证内容、用户确认和未完成项见 [Linux 适配记录](docs/LINUX_ADAPTATION.zh-CN.md)。

发布包取自同一提交的成功云端运行，包含统一安装入口、字典、许可材料和校验文件。长期游玩验证完成前维持预发布标记。

## 项目结构

- `src/features/`：共享文本编码、输入编辑、搜索及姓名规则。
- `src/platform/`：操作系统适配。
- `src/targets/`：指定游戏版本的补丁位置、对象布局与调用适配。
- `src/runtime/`：补丁预检、安装和回滚基础设施。
- `scripts/`、`install.sh`：安装、启动与验证工具。
- `tests/`：共享行为、平台适配与补丁契约测试。

## 截图

以下图片沿用 macOS 项目的功能示例，不代表所有平台组合均已完成实机验证。

![游戏内效果](img/screenshot.png)
![拼音搜索](img/pinyin.gif)
![东亚姓名顺序](img/east_asian_names.png)

## 致谢与许可

感谢 [matanki-saito/EU4dll](https://github.com/matanki-saito/EU4dll)、[PoXiao-zero/eu4dll_mac](https://github.com/PoXiao-zero/eu4dll_mac)、[cpp-pinyin](https://github.com/wolfgitpr/cpp-pinyin) 和 [insert_dylib](https://github.com/tyilo/insert_dylib)。

本仓库许可证见 [LICENSE](LICENSE)。第三方库与字典保留各自许可和来源说明，见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 及发布包中的许可文件。
