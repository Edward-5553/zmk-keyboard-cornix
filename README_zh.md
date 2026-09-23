# ZMK Keyboard Cornix

用于 Cornix 分体式人体工学键盘的 ZMK 开发板与扩展板模块。

[在线文档：English / 简体中文](http://gh.bhee.online/zmk-keyboard-cornix/) ·
[English README](./README.md) ·
[日本語 README（AI 生成）](./README_jp.md)

**当前板卡版本：** [`v3.0.0`](https://github.com/hitsmaxft/zmk-keyboard-cornix/releases/tag/v3.0.0)

**ZMK 基线：** 基于 Zephyr 4.1 的 `main`

![带 dongle 的 Cornix](images/cornix_with_dongle.png)

## 项目内容

- `cornix_left//zmk`：标准分体构建的左半侧
- `cornix_right//zmk`：右侧外围设备
- `cornix_ph_left//zmk`：dongle 构建的左侧外围设备
- `cornix_dongle_adapter`：中央 dongle 的矩阵与蓝牙角色
- `cornix_dongle_eyelash`：可选的显示硬件 overlay
- `cornix_indicator`：已可用于生产的 RGB 电量与连接状态指示

Cornix 采用紧凑的 3×6 列交错布局，每侧有三个拇指键。硬件支持 USB-C、
蓝牙、Kailh Choc V2 热插拔轴座，以及 10°、18°、25° 三档帐篷角度。

## 当前键位预设

当前配置使用 **50 个绑定位置**。下图完整展示常用的三层，包含尚未分配功能的位置：

- **L0 基础层**：轻点 Space / Enter 输入空格 / 回车，按住分别进入 L1 / L2。
- **L1 数字 / 导航层**：E/S/D/F 对应方向键，Z/X/C/V 为 Ctrl 快捷键；最左列原有的四个蓝牙控制键留空。右手数字为 789 / 456 / 0123，小数点在 0 上方。
- **L2 功能 / 符号层**：上方两行符号，第三行保留左 Shift 并放置 F2–F12；F1 位于基础层 Caps 拇指键位置。 左手 A/S/D 位置一键输入 `{}` / `[]` / `<>` 并将光标左移一次，图中 `|` 表示光标。实际标点取决于输入法；编辑器自动补括号可能干扰结果。

所有层旋钮功能一致：左旋钮顺时针向下滚动、逆时针向上滚动；右旋钮顺时针增大音量、逆时针减小音量。左旋钮按下切换 Caps Lock，右旋钮按下切换静音（键位 30/31）。基础层 41 号为 Ctrl+Alt+Delete，42 号为 Alt+Space；Fn/Symbol 层 41 号仍为 F1，透明绑定沿用基础层功能。左侧使用鼠标滚轮事件，滚动量受系统设置影响。首次启用鼠标功能后，若蓝牙连接不能滚动，需刷新主机的 HID 缓存（通常移除设备后重新配对）。

![Cornix 最新键位图：基础、数字导航、功能符号三层，每层完整显示 50 个位置](keymap-drawer/cornix.svg)

[查看 PNG 原图](keymap-drawer/cornix.png) · [下载离线键位编辑器](keymap-drawer/cornix.html)
（下载 HTML 后在浏览器中打开）。编辑器支持实体键盘录入、拖动交换和导出修改后的 HTML。

绿色键兼具轻点 / 长按功能；`from L0` 表示透明键并显示基础层回落值；`—` 表示未分配。
Scroll、Snipe 为预留层，当前没有切入按键；两层也保留固定的旋钮按压功能。图中数据来自当前
`config/cornix.keymap`，不代表 v3.0.0 发行包或设备上通过 Studio 保存的改键。

修改键位后运行 `python scripts/generate-keymap-image.py`，同步生成 HTML、SVG 和 PNG
（需要 Pillow）。

## Zephyr 4.1 要求

必须使用 `nice_nano//zmk` 等带限定符的 ZMK 板名。未限定的 `nice_nano`
可能选中 `CONFIG_SETTINGS_NONE=y`，导致重启后丢失蓝牙身份并破坏已有分体绑定。

每次构建 nice!nano dongle 或 reset 固件后，须检查最终 `.config` 含有：

```text
CONFIG_NVS=y
CONFIG_SETTINGS_NVS=y
```

且不得含有 `CONFIG_SETTINGS_NONE=y`。

## 构建目标

标准分体：

```yaml
include:
  - board: cornix_left//zmk
    artifact-name: cornix_left

  - board: cornix_right//zmk
    artifact-name: cornix_right

  - board: cornix_right//zmk
    shield: settings_reset
    artifact-name: cornix_reset
```

Dongle 集成：

```yaml
include:
  - board: nice_nano//zmk
    shield: cornix_dongle_adapter cornix_dongle_eyelash cornix_dongle_display
    snippet: studio-rpc-usb-uart
    artifact-name: cornix_dongle

  - board: cornix_ph_left//zmk
    artifact-name: cornix_left_for_dongle

  - board: cornix_right//zmk
    artifact-name: cornix_right

  - board: nice_nano//zmk
    shield: settings_reset
    artifact-name: dongle_reset
```

仅当 dongle 开发板尚未提供 `zephyr,display` 时，方需加入
`cornix_dongle_eyelash`；本地 `cornix_dongle_display` shield 提供显示组件及月薪喵动画。
界面采用黑底白字的 64×128 竖屏：左上连接状态、右上固定 L/R 电量，两区同为 18 像素高且顶底对齐。
中部 84 像素动画区（约 66%）显示放大的 64×64 月薪喵，底部只保留层名，默认不显示修饰键和锁定状态。
默认循环播放 idle 动画；任意一侧首次按键即切换为 error，最后一次按键后满 3 秒恢复 idle。
依据实际按下事件判断，不依赖 WPM；单键长按、松键和旋钮旋转不会重新计时。
电量的 L/R 标识始终显示，未知读数显示 `--%`，0% 也正常显示。每次 dongle 启动后，
两边各按一次普通按键来识别电量来源；配对顺序不会改变 L 在上、R 在下的排列。
详见[竖屏布局与验证](docs/dongle-portrait.md)。
详见[实现与验证](boards/shields/cornix_dongle_display/UPSTREAM.md)及[素材署名](boards/shields/cornix_dongle_display/ASSETS.md)。
仅更新动画时，只刷 dongle 固件，无需刷 reset 或重新刷左右手。

## RGB 指示灯

3.0.0 已使可选的 `cornix_indicator` shield 达到生产可用状态。它通过
`zmk-rgbled-widget` 显示电量及分体连接状态。

该 shield 设置 `CONFIG_RGBLED_WIDGET_EXT_POWER_TIMEOUT_MS=1000`。动画结束且无
常亮指示后，WS2812 外部供电将在 1000 ms 后关闭，以降低空闲功耗；持续点亮的
LED 仍会耗电。RGB 须主动启用，默认 v3.0.0 发布包并未开启。

当前 `build.yaml` 的三种左右手构建均启用 `cornix_indicator`，其共用配置
`boards/shields/cornix_indicator/cornix_indicator.conf` 明确启用 USB 供电检测、
电量上报及充电呼吸动画。配置仅作用于带该 shield 的固件，不影响 reset 构建。
每侧独立使用第 0 颗 RGB 灯指示自身电量，第 1 颗继续指示连接状态：

- 插入 USB 且电量低于 99%：绿色呼吸，约 2 秒一个周期，50 ms 刷新一次。
- 插着 USB 且电量达到 99%：绿色常亮提示 2 秒，随后恢复其他指示或熄灭。
- 拔掉 USB：停止充电呼吸，短暂显示普通电量提示后恢复其他指示或熄灭。

这里依据 USB 供电与估算电量判断，并非充电芯片的真实充满信号。
分体的 USB 供电检测不需要启用 USB 键盘输出，dongle 模式仍通过蓝牙传输按键。
请使用最新构建的 `cornix_left_for_dongle_nosd.uf2` 与
`cornix_right_nosd.uf2` 分别更新左右手；标准分体模式左手使用
`cornix_left_default_nosd.uf2`。本次灯效配置无需更新 dongle 或清除配对。
实机验证时分别插拔两侧 USB，检查低于 99% 时持续呼吸、拔线后停止，以及
插线启动和同时充电时两侧各自的灯效；电脑 USB 和普通充电器都应检查。

## 刷写与恢复

1. 若需清除绑定，为涉及的每个角色刷入对应的 settings-reset UF2。
2. 将左、右及可选 dongle UF2 分别刷入对应设备。
3. 同时复位两侧，再由主机重新连接。

Cornix 自 v2.3 起使用无 SoftDevice 的闪存布局。若使用旧固件、需兼容原厂 RMK，
或设备已无法进入 UF2 模式，请依[引导程序恢复指南](./bootloader/README.md)处理。
切勿在未重置相关设备时混用固件角色或闪存布局。

## 文档与支持

- [中文安装指南](http://gh.bhee.online/zmk-keyboard-cornix/zh/)
- [English installation guide](http://gh.bhee.online/zmk-keyboard-cornix/en/)
- [ZMK 官方文档](https://zmk.dev/docs/)
- [问题追踪](https://github.com/hitsmaxft/zmk-keyboard-cornix/issues)
- [RMK 固件项目](https://rmk.rs/)
