# Cornix + StopWatch 原生 RMK（实验版本）

本目录是独立的 Rust / RMK 固件，与 `ports/stopwatch` 的 ESP-IDF 接收器分开构建。
目标是复用 RMK 的按键引擎，让左右 Cornix 直接连接 ESP32-S3 StopWatch。

```text
Cornix 左半 nRF52840 ── RMK BLE split ──┐
                                      ├─ StopWatch ESP32-S3 ─ USB HID ─ 电脑
Cornix 右半 nRF52840 ── RMK BLE split ──┘
```

## 当前范围

- 三个固件目标：`cornix-left`、`cornix-right`、`cornix-rmk-stopwatch`。
- S3 作为无本地矩阵的 split central，两个 Cornix 均为 peripheral。
- 50 键、五层、两个旋钮、Space/Enter layer-tap、三种括号宏。
- 默认键位取自本仓库 `config/cornix.keymap` 的快照；后续独立编辑
  `config/layout.toml`，运行 `python tools/generate.py` 同步两个构建配置。
- USB 键盘、媒体键、鼠标滚轮和 Vial 改键。RMK 自带 BLE 主机输出也保留，
  首轮验收以 USB 输出为准。
- 从机电量 ADC、配对存储。RGB 电源默认关闭。
- **未接入 StopWatch AMOLED、触摸、IMU、LVGL 和旧版 WebHID 编辑器。**
  屏幕不亮并不能说明输入固件没有运行；这版先验证输入链路。
- **尚未完成实机验证。** CI 构建成功也不代表两半连接、旋钮方向、功耗和延迟已通过。

三个设备都必须刷本方案配套固件，不能混用 ZMK 两半或假定兼容原厂 RMK 二进制。
本方案使用 `split` central 模式，没有使用 RMK 的“单键盘报告转发”`dongle` feature。

## 固定依赖

- RMK：`f12257e89e9d07c461f4c774626b04af5b10600b`，三个目标保持一致。
- ESP Rust HAL：`5363c4c493f5f11654696ff192028cf45f04ba72`，与上述 RMK 的 S3 示例一致。
- 两个 Cargo 工程分别管理依赖，避免 ARM / Xtensa 平台 feature 相互影响。
- 初始 `Cargo.lock` 以对应上游示例为种子；CI 首次解析新工程依赖后会随产物保存
  完整 lockfile。固件按 commit 固定 RMK，但在回收 CI lockfile 前不宣称完全可复现。

## GitHub Actions 构建

工作流：**Build RMK Cornix StopWatch**（`.github/workflows/build-rmk.yml`）。

1. 推送 `codex/rmk-stopwatch-dongle` 分支下本目录的修改会自动启动构建。
2. 也可在 Actions 选择工作流和分支，点击 **Run workflow**。
   新工作流未进入默认分支时，GitHub 可能不显示手动按钮；先用分支推送触发。
3. `config` 只检查 TOML、键位、引脚、协议一致性和 Flash 边界。
4. 两个独立 job 在 Ubuntu 上编译 nRF52840 和 ESP32-S3，不需要本地 Rust 或 ESP-IDF。
5. 下载同一次运行的两个 Artifacts：
   - `cornix-rmk-peripherals`：左右 `.uf2`、`.hex`、配置与依赖记录。
   - `cornix-rmk-stopwatch`：S3 合并 `.bin`、调试 `.elf`、Vial 定义和分区表。

无需本地编译的检查：在仓库根目录运行 `python ports/rmk/tools/check.py`（Python 3.11+）。

## 刷写

### Cornix 两半

现有 no-SoftDevice UF2 bootloader：双击复位进入磁盘，分别复制 `cornix-left.uf2`
和 `cornix-right.uf2`。这里不生成或覆盖 bootloader。

链接范围固定为 `0x1000..0xD4000`；`0xD4000..0xF4000` 留给 RMK 存储；
`0xF4000` 及以上保留给 bootloader/settings。不要把应用 UF2 当作 bootloader 恢复包。

### StopWatch

在下载模式连接原生 USB，使用 esptool 将 **合并镜像写到地址 0x0**，例如 esptool 5：

```shell
python -m esptool --chip esp32s3 --port COM5 write-flash 0x0 cornix-rmk-stopwatch-merged.bin
```

替换实际串口。合并镜像包含启动程序和分区表，不是旧 ESP-IDF 工程的 app-only bin。
使用前 4 MiB 的保守布局；RMK 存储为 `0x3F0000..0x400000`，与镜像中的应用不重叠。
镜像不依赖 PSRAM。不支持从旧编辑器升级到这个固件。

### 配对与清理

第一次建议先只开启本套设备：接收器上电，再开左右两半，检查普通键与旋钮。
更换固件体系后可能需要清理旧配对和存储。已连接时可以在 Vial 分配 **Clear Peer**，
长按 5 秒清理分体配对；Vial 解锁键为最外侧第一行两键（默认 Escape 和反斜杠）。

若旧存储导致无法启动或配对，可手动运行 Actions 并勾选 `reset_storage`，刷写该次
两个 `-reset` 包中的全部三个镜像并上电一次，然后立即刷回正常包。reset 固件每次
启动都会清除 RMK 配对和改键，**不能留作日常固件**。清理会丢失保存的键位。

## 与现有 ZMK 行为的差别

- RMK 的 permissive hold + 200 ms 接近当前 balanced layer-tap，但不是同一实现。
- RMK quick-tap 150 ms 从上次释放计时，ZMK 配置对应的规则从按下计时。
- 括号宏使用 RMK 原生 text/tap 操作；没有声称和 ZMK 的 30 ms tap/wait 时序相同。
- 滚轮使用 RMK 原生鼠标行为，方向配置为左旋钮 CW 向下、CCW 向上。
- 默认请求 7.5 ms split 间隔不等于端到端延迟；需要实机记录协商结果和输入表现。
- 上游睡眠、宏等待、断线后的按键状态仍需实测；不要把“使用原生 RMK”当作已解决
  所有延迟或丢包问题。初版请专门测试按住 Shift/层键时关闭一个半键盘的恢复情况。

## 验收顺序

1. USB 枚举、两半连接、全部普通键和旋钮按压。
2. 两个旋钮方向与步进，跨两半组合键，Space/Enter 连击和长按切层。
3. 快速输入、括号宏与实体按键交错，比较原有 ZMK dongle。
4. 断线松键、重连、三设备掉电后的配对保持、USB 拔插和电脑休眠。
5. 输入稳定后再接入 StopWatch 显示，并复测绘制时的输入延迟。

## 来源

Cargo、构建脚本和平台启动方式改编自 [RMK 官方示例](https://github.com/rmk-rs/rmk)，
沿用其 MIT 许可（`LICENSE-RMK-MIT`）。三设备角色和 Cornix 硬件配置参考
[cornix-prospector-rmk](https://github.com/cffnpwr/cornix-prospector-rmk)，并与本仓库
Cornix DTS 核对。这里没有使用或声称拥有 Cornix 原厂完整源码。
