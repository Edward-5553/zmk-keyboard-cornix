# ZMK 输入行为对齐

核对日期：2026-09-29。基准为本仓库 `config/cornix.keymap`、原生
`cornix_dongle_adapter` 配置，以及其 manifest 指向的 ZMK `main`。
仓库没有保存原生接收器实际刷入版本的完整 `.config` / 上游提交号，因此
这里是对源码基准的对齐，不是对某个已刷固件的逐字节等价认证。

## 本次修正

| 项目 | 原实现 | 当前实现 / ZMK 基准 |
| --- | --- | --- |
| 两条 split BLE 连接 | NimBLE 默认连接参数 | 显式请求 interval=6（7.5 ms）、latency=30、timeout=400（4 秒） |
| 实际连接参数 | 未记录 | 连接和参数更新时输出实际 interval / latency / timeout；与首选值不符时告警 |
| 空格/回车层键短按 | 复用宏 tap，阻塞输入任务 30+30 ms | 判定 tap 后立即发送按下、回放捕获事件、发送松开，无额外定时等待 |
| quick-tap | 每个位置独立记录松开时间 | 最近一次 tap 的按下时间，150 ms 按下到按下；其他非修饰键打断 |
| balanced 捕获 | 也扣留先前已按下的普通键松开 | 该类非修饰键松开立即处理，不因其松开判 hold |
| 三步括号宏 | 在输入任务内同步等待约 180 ms | 有界行为队列：每步按下 30 ms、松开后等 30 ms，物理按键持续处理 |
| 右旋钮音量 | 每格阻塞 30+30 ms | `inc_dec_kp` 默认按下 5 ms、松开后无额外等待，异步排队 |
| USB 报告 | 忙时同步等待最多 100 ms，超时后状态可能被覆盖 | 有界 FIFO 按顺序保留按下/松开；不等待端点，拒绝提交时保留队首 |
| 无关 HID 页 | 普通按键也发送消费控制报告 | 只发送发生变化的键盘/消费控制状态；取消时强制释放 |

`latency=30` 是允许外围设备在空闲时跳过的连接事件数，**不是 30 ms**。
沿用 ZMK 值，而非为了“优化”随意改成 0。最终无线参数、重传和双连接调度仍须实机确认。
这些值集中在 `main/zmk_compat.h`，不是 ESP-IDF 的全局默认值，也不影响电脑 BLE HID 链路。

宏/音量队列有 64 项，每项保存一次按下/松开的行为。USB FIFO 有 64 条报告。
这些容量和 ESP-IDF 任务实现是本移植的选择，不声称与 Zephyr 队列配置完全相同。
队列溢出取消未完成动作并释放状态；USB 连续 100 ms 无提交进展时请求同样的恢复，
不是阻塞输入任务 100 ms。蓝牙输出通过代次标记丢弃取消前尚未发送的报告。
USB 已提交到控制器的报告不能撤回；恢复依靠随后提交全释放报告。

USB HID 的 1 ms 轮询、6 键加独立修饰键，以及两半直连 dongle 的拓扑保持原样。
物理按键仍使用接收时记录的事件时间；处理积压事件前，不用当前墙钟提前把短按判成长按。

## 上游依据

- [Split Kconfig](https://github.com/zmkfirmware/zmk/blob/main/app/src/split/bluetooth/Kconfig)：6 / 30 / 400。
- [Split central](https://github.com/zmkfirmware/zmk/blob/main/app/src/split/bluetooth/central.c)：用首选参数建立连接。
- [Hold-tap](https://github.com/zmkfirmware/zmk/blob/main/app/src/behaviors/behavior_hold_tap.c)：`decide_balanced`、`is_quick_tap`、tap 按下/捕获回放/松开顺序。
- [Behavior queue](https://github.com/zmkfirmware/zmk/blob/main/app/src/behavior_queue.c)：延迟工作推进行为，不在输入调用里睡眠。
- [Sensor rotate](https://github.com/zmkfirmware/zmk/blob/main/app/src/behaviors/behavior_sensor_rotate_common.c)：按下后等 tap-ms、松开后等待为零。
- [Sensor rotate 默认值](https://github.com/zmkfirmware/zmk/blob/main/app/dts/bindings/behaviors/zmk%2Cbehavior-sensor-rotate-var.yaml)：tap-ms=5。
- 宏的 tap-ms=30 / wait-ms=30、balanced 的 200 ms / 150 ms 来自本仓库键位文件。

## 验证与边界

主机测试编译实际 `engine.c` 和 `usb.c`，使用可控时钟及模拟 USB 端点覆盖：

- 原有 16 组引擎场景及路由、存储、显示、姿态测试。
- 空格后字母立即输出、滚动按法的报告顺序、quick-tap 起点与打断。
- 宏执行时继续输入、音量 5 ms 释放和连续旋钮事件。
- 取消、队列溢出、毫秒回绕、调度延后不压缩按下时间。
- USB 忙/提交失败时保留所有边沿，取消后丢弃旧报告、发送释放。
- USB 持续无进展恢复，以及挂起不误报为正常连接的发送超时。

2026-09-29：以上主机测试在 Windows/GCC 上通过。测试替换了 TinyUSB/硬件接口，
不能证明真实 USB 控制器、NimBLE API 集成或射频时延。当前环境未找到 ESP-IDF 工具链，
本次尚未完成 ESP32-S3 整体编译、刷机和新旧接收器实测。

S3 仍是独立 ESP-IDF 实现，并非运行原版 ZMK。仅支持 README 列出的行为子集；
未实现完整 ZMK HID/行为引擎、Studio、鼠标加速/滚动曲线或完整主机兼容逻辑。
左旋钮沿用每格一个相对滚轮报告，不能宣称与原生 `&msc` 的 30 ms 脉冲/加速曲线完全等价。
多个消费控制键、宏与实体键使用同键码/隐式修饰键等组合仍需差分实机验收。
显示、USB 主机切换和断线恢复策略仍采用本移植的设计。

## 实机对比

1. 在 ESP-IDF 5.4.4 中构建并仅更新 StopWatch，不清除配对或已保存键位。
2. 从 UART 日志检查两条 split 链路是否为 `interval=6, latency=30, timeout=400`；
   本固件的正常 USB 接口仍然不提供日志串口。
3. 相同电脑、USB 口和两半固件，分别测试纯字母、带空格的句子、回车后继续打字、
   转音量时同时打字、括号宏执行时继续输入。
4. 单独检查 USB 和 BLE 输出，按住修饰键断开一半、主机睡眠/恢复和快速重连。
5. 使用已保存的 WebHID 键位时，先核实与原生 dongle 的键位一致；刷入默认预设不会
   覆盖 NVS 中的改键。尚未实测前，不给出延迟已达到原生 ZMK 的结论。
