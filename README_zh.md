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
- **L1 数字 / 导航层**：E/S/D/F 对应方向键，Z/X/C/V 为 Ctrl 快捷键；最左列为 BT Clear、BT 1–3。右手数字为 789 / 456 / 0123，小数点在 0 上方。
- **L2 功能 / 符号层**：上方两行符号，第三行为 F1–F12。

![Cornix 最新键位图：基础、数字导航、功能符号三层，每层完整显示 50 个位置](keymap-drawer/cornix.svg)

[查看 PNG 原图](keymap-drawer/cornix.png) · [下载离线键位编辑器](keymap-drawer/cornix.html)
（下载 HTML 后在浏览器中打开）。编辑器支持实体键盘录入、拖动交换和导出修改后的 HTML。

绿色键兼具轻点 / 长按功能；`from L0` 表示透明键并显示基础层回落值；`—` 表示未分配。
Scroll、Snipe 仍为全透明的预留层，当前没有切入按键。图中数据来自当前
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
低速敲键盘、高速吃零食打字，闲置显示睡觉静态帧；切换依据 ZMK 的 WPM 采样。
详见[实现与验证](boards/shields/cornix_dongle_display/UPSTREAM.md)及[素材署名](boards/shields/cornix_dongle_display/ASSETS.md)。
仅更新动画时，只刷 dongle 固件，无需刷 reset 或重新刷左右手。

## RGB 指示灯

3.0.0 已使可选的 `cornix_indicator` shield 达到生产可用状态。它通过
`zmk-rgbled-widget` 显示电量及分体连接状态。

该 shield 设置 `CONFIG_RGBLED_WIDGET_EXT_POWER_TIMEOUT_MS=1000`。动画结束且无
常亮指示后，WS2812 外部供电将在 1000 ms 后关闭，以降低空闲功耗；持续点亮的
LED 仍会耗电。RGB 须主动启用，默认 v3.0.0 发布包并未开启。

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
