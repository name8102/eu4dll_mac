# EU4 1.37.5 Linux 中文适配

适用于原生 x86-64 Linux ELF，版本 `EU4 v1.37.5.0 Inca`，SHA-256：
`af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3`。
其他二进制在安装前拒绝打补丁。中文字体与本地化内容仍由中文模组提供。

## 当前结果（2026-09-05）

| 功能 | 实现与证据 |
| --- | --- |
| UI、换行、提示、地图、3D 文本 | canonical Linux target 已接入；真实 ELF 安装所有对应补丁 |
| 单字国名 | CurveText 三处计数统一为逻辑字符；用户确认单字国名显示正常 |
| 两字国名字距 | 已修复加空格循环把 AL 返回值按 EAX 读取、以及末尾 ASCII 混入旧汉字载荷的问题；真实机器码回归通过，用户确认字距正常 |
| 输入与编辑 | 用户确认输入法可激活、中文显示正常；整段 UTF-8 提交、左右移动、原子删除、选区、长度边界经真实 CTextBuffer 验证，用户确认退格正常 |
| 剪贴板 | Copy、Cut、Paste 三条调用均转换；真实游戏函数配合隔离的 SDL dummy 剪贴板通过 |
| 搜索 | 中文、全拼、首字母、大小写、混合名称经真实 Process 验证；用户确认拼音搜索正常 |
| 本地存档名 | 文件系统保留 UTF-8，列表与选择框独立转为显示编码；用户确认中文名存档正常 |
| 存档详情与继续游戏提示 | 新增两处文件名显示副本转换；真实调用验证保留颜色前缀和原始 UTF-8 文件名，用户确认游戏内显示正常 |
| 顶栏日期 | 已接入年月日格式；原生日期格式器与调用约定验证通过，月份本地化在探针内使用替身，用户确认游戏内显示正常 |
| 东亚姓名 | 君主、共和国显式姓氏/姓氏表/随机王朝三路径均接入现有文化规则，支持 Bimillennium 模式；真实调用适配器通过，用户确认游戏内正常 |

游戏内置 SDL 的 `X11_StartTextInput`/`X11_StopTextInput` 是空函数，无法仅靠
`SDL_IM_MODULE=fcitx` 启用输入法。已用 `SDL_DYNAMIC_API` 接入本机
`/usr/lib/libSDL2-2.0.so.0`（sdl2-compat 2.32.70），真实 ELF 程序级探针通过；
用户已确认可输入并正常显示中文。该覆盖机制由
[SDL 官方 Dynamic API 文档](https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-dynapi.md)定义。

## 构建、验证与安装

下载 Release 的 `eu4dll-linux-x86_64.zip` 后解压，在解压目录执行：

```sh
python3 install.py '/path/to/Europa Universalis IV'
```

云端 Linux 包在 Ubuntu 24.04 x86-64 上构建，运行环境需提供对应 glibc/libstdc++
与 SDL2；具体符号版本写入包内 `BUILD_INFO.txt`。更旧发行版可从源码本机构建。
云端不含游戏二进制，只验证编译、测试和打包；实际游戏验证范围如下述记录。

从仓库根目录构建和安装：

```sh
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-linux -j 6
ctest --test-dir build-linux --output-on-failure
scripts/linux/probe.sh '/path/to/Europa Universalis IV/eu4.orig'
SDL_DYNAMIC_API=/usr/lib/libSDL2-2.0.so.0 scripts/linux/probe.sh '/path/to/Europa Universalis IV/eu4.orig'
python3 scripts/linux/install.py '/path/to/Europa Universalis IV'
```

安装器先校验原始 ELF，再备份现有启动器、动态库和字典，逐文件原子替换。
备份及带 SHA-256 的安装清单位于游戏目录 `eu4dll-backups/<时间>/`。
原始 ELF 保留为 `eu4.orig`。输入开启时启动器默认设置
`SDL_DYNAMIC_API=libSDL2-2.0.so.0` 和 X11 后端，保留用户显式设置；
系统须提供支持输入法的 SDL2，可用 `SDL_DYNAMIC_API` 指定库路径。
`EU4DLL_USE_SYSTEM_SDL=0` 可关闭该默认选择。启动器使用无空格运行时软链接，避免 Steam 默认
安装路径中的空格被 `LD_PRELOAD` 当作分隔符。字典必须位于动态库旁。

`scripts/linux/probe.sh` 只在真实 ELF 中替换测试入口，执行游戏构造器与
已安装机器码，不载入战役、不触碰用户存档。普通 CTest 与真实 ELF 探针覆盖
不同层次，均不能替代游戏内候选框、地图视觉及长期运行验证。

## 实现边界

- 编辑保留游戏原来的字节长度限制，但只在完整逻辑字符处截断，避免半个汉字。
- 拼音字典在首次搜索时初始化，避免预加载构造器早于 cpp-pinyin 全局对象。
- 存档补丁覆盖手动本地存档、选择、列表及覆盖/载入/删除确认框；云存档、
  自动生成的存档名与所有读取路径仍未逐项验证。
- 日期与姓名由 `EU4DLL_ENABLE_DISPLAY_FORMATTING=1` 开启，启动器默认启用。东亚文化组与分隔符沿用共享规则；启用模组列表含 `Bimillennium_Universalis_` 时使用对应模式。
- 用户已确认新版日期、姓名和存档提示均正常；此确认不扩展为所有文化、模组组合或长期稳定性验证。
- 尚未完成长期战役运行、连续切换地图模式及所有模组组合的稳定性验证。

诊断可用 `EU4DLL_TRACE_INPUT=1` 记录输入生命周期与事件字节数，不记录输入正文。
测试目标 `eu4dll_linux_curve_trace` 可通过 `EU4DLL_EXTRA_PRELOAD` 加载；仅在
`EU4DLL_TRACE_MAP_NAMES=1` 时记录黑羊/白羊的有限组顶点到临时文件。
诊断库不属于正常安装内容。
