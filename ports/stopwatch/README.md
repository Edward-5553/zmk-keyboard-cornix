# Cornix StopWatch Dongle — experimental, USB-powered

独立的 **M5Stack StopWatch C152 / ESP32-S3R8** 接收器固件：

```text
Cornix 左半（ZMK peripheral） ─ BLE split ─┐
                                        ├─ StopWatch ─ USB HID / BLE HID ─ 电脑
Cornix 右半（ZMK peripheral） ─ BLE split ─┘
```

这不是可以刷入 ESP32 的 ZMK UF2。当前项目的 ZMK / Zephyr 4.1 USB 栈
不能直接用于 ESP32-S3，因此本目录独立使用 **ESP-IDF 5.4.4 + NimBLE + TinyUSB**。
Nordic 的板定义、原有 dongle 和 `build.yaml` 保持不变。

## 当前范围

2026-09-29 输入路径修正：两条分体 BLE 链路显式采用 ZMK 的 7.5 ms / latency 30 /
4 秒超时；空格/回车短按不再额外等待，宏与音量旋钮改为非阻塞定时行为，USB 使用保序队列。
主机回归测试通过；本次整体固件编译及实机延迟对比尚未完成。
对齐范围、上游依据和差异见 [ZMK 输入行为对齐](ZMK_COMPATIBILITY.md)。

- 实现两条分体 BLE 连接及一条电脑 BLE HID 连接、ZMK split GATT 服务发现、加密订阅、NVS 配对保存和定向广播重连。
- 构建时读取 `../../config/cornix.keymap` 作为 5 层 × 50 键的默认预设；
  支持通过 [HTML 编辑器](editor.html) 在线改键并保存到 NVS，重启后恢复。
- 支持当前预设的普通键、修饰组合、透传、禁用、Space/Enter 层切换、三个括号宏。
- 层切换实现 balanced 判定（200 ms）、快速重复短按（150 ms）；
  当前实现只覆盖本项目预设，不承诺完整 ZMK 行为兼容。
- 左旋钮滚轮、右旋钮音量，USB / BLE 键盘、多媒体、鼠标滚轮报告。
- 断开任一半、USB 挂起或事件溢出时释放所有按键，取消待判定的层键。
  另一半仍按住的键在其下一次状态通知时恢复；异常期间的短按可能丢失。

**状态：用户已实机反馈可以正常输入；完整异常与长期稳定性验收仍待完成。**
已实现 CO5300 圆屏、月薪喵工作/睡觉动画、左右连接与电量、当前层和 USB 状态；
**显示功能已本地编译及逻辑测试；2026-09-27 用户确认仅降至 100 kHz 后画面正常。**
断连、电量、挂起恢复及长期运行等场景仍按下方验收项目逐项验证。
当前蓝牙连接电脑为新增实验功能，尚待实机验证；不支持触摸、按钮切页、
ZMK Studio / DYA Studio、指点设备转发、键盘 LED 状态回传或 OTA。
本版只面向 USB 供电，不管理 M5PM1 充放电或电池待机。

[圆屏交互设计稿](design/display.html)保留为演示预览；固件读取真实状态。
实现细节及资源说明见[显示说明](design/README.md)。

已在 Windows 的 ESP-IDF 5.4.4 / ESP32-S3 工具链完成应用及引导程序编译，
并通过 16 组键位引擎场景、不支持配置拒绝、显示状态、动画校验和字库覆盖测试。已检查最终配置中的
三条 BLE 连接、NVS 配对持久化、16 MB Flash、HID 和 USB 调试串口禁用设置。
GitHub 工作流尚未运行，以上结果来自本地构建。

键位编译器遇到未支持的键名、行为、旋钮绑定、宏格式或层结构会报错。
已有行为范围内的日常改键可以使用 HTML，无需重刷。修改编译默认预设或增加行为时，
仍须运行测试并重新编译，不能直接把任意 ZMK 配置复制进来使用。

## 蓝牙连接电脑（第一版，待实机验证）

- 电脑蓝牙名称：**Cornix StopWatch BLE**。两半继续使用原有 ZMK split 固件；只刷 StopWatch。
- 同时保留一个电脑配对和两个分体配对。电脑身份存入独立 `sw_ble_host` 命名空间，
  启动计算分体配对数时排除电脑；电脑连接与分体扫描使用各自的 GAP 回调。
- 未保存电脑时，启动后开放 **120 秒**电脑配对窗口；超时重启或使用下方 USB 工具重新开放。
  配对为 Just Works，无需输入 PIN；保存后只接受该电脑，不自动替换。电脑重启/断线后可以重连。
- **USB 优先**：USB 已枚举且未挂起时向 USB 发送；没有 USB 主机、且电脑蓝牙已加密并订阅键盘报告时向 BLE 发送。
  插着充电器仍可以蓝牙输入。USB 电脑挂起时暂停输出，不自动把输入转给另一台蓝牙电脑。
- 切换时取消层键和宏、释放旧/新输出端状态；切换过程中的短按可能丢失，先松开所有键再测试。
  BLE 报告使用有界 FIFO 保留快速按下/松开的顺序；队列溢出或通知持续阻塞会断开电脑以避免卡键，随后允许重连。
- 圆屏顶部仅显示当前输出图标：绿色 USB / 蓝色蓝牙；配对、连接中或等待重连显示灰色省略号；未连接或暂停输出显示灰色 ×（休眠时整屏关闭）。动画、WPM、60 秒降亮度/120 秒息屏在两种输出模式下可用。
- HTML 改键及电脑配对管理仍走 **USB**；不支持蓝牙 WebHID、多个电脑配置或蓝牙唤醒休眠电脑。
  仍未增加 M5PM1 电池管理，首轮建议使用充电器/移动电源供电。

本地现有 `sdkconfig` 已更新。其他已有构建目录不会自动采用新的 defaults；通过 `idf.py menuconfig`
启用 NimBLE Peripheral / Broadcaster，并将 Maximum connections 和 Maximum bonds 设为 **3**，
控制器 `CONFIG_BT_CTRL_BLE_MAX_ACT` 至少 **6**。新构建直接使用本仓库 defaults。
不要为了更新编译配置擦除设备 Flash。

在本目录的 ESP-IDF 5.4.4 环境中重新构建、进入下载模式并刷写：

```sh
idf.py build
idf.py -p COMx flash
```

刷完正常启动，在电脑蓝牙设置中添加 `Cornix StopWatch BLE`。随后将 USB 改接充电器，
确认画面顶部显示蓝色蓝牙图标，验证两半普通键、组合键、长按层、括号宏、音量和滚轮。
再测试重启重连、蓝牙断开重连、两分钟息屏后首键、插回电脑 USB 切换。
三连接下的实际延迟、丢键率和长期稳定性仍需要实机验证。

更换电脑，或电脑端删除配对后要重新配对时：在旧电脑删除该蓝牙设备，接回数据 USB，运行：

```sh
python -m pip install hidapi
python tools/ble-host.py                # 查询电脑配对和蓝牙就绪状态
python tools/ble-host.py --forget-host  # 仅删除电脑配对，重新开放 120 秒
```

工具使用 USB 厂商 Feature Report 7，检查异步清除结果；左右分体配对、保存的键位不受影响。
`ready` 表示蓝牙链路可用，不代表当前优先输出端（接数据 USB 时仍优先 USB）。
不要使用 `erase-flash` 来更换电脑。

## HTML 在线改键（首次需要刷入支持版本）

1. 在 ESP-IDF 5.4.4 环境中 `idf.py build`，进入下载模式后 `idf.py -p COMx flash`。
   只刷 StopWatch，无需更新左右半或清除配对。刷完重新插拔 USB，进入正常键盘模式。
2. 用桌面 **Chrome / Edge** 打开本目录的 `editor.html`（不是圆屏设计稿 `design/display.html`）。
   点击“连接 StopWatch”，在浏览器设备选择框中选择 `StopWatch Dongle (experimental)`。
   首次连接会读取当前设备键位；若已有浏览器草稿，会先提示是否替换。
3. 选择层，拖动交换；或点选一个键位，用笔记本/外接键盘录入按键及组合。
   透明、禁用、轻点/长按切层和已有括号宏可在“特殊功能”中选择。
4. 松开所有键，点击“写入并保存到设备”。页面显示修改数量，确认后上传整份键位，
   设备校验并保存，页面回读核对后才显示成功。成功后立即生效，关机重启仍保留。
5. “恢复默认到草稿”读取**当前固件内**的默认键位，只修改页面；检查后仍须点击写入。
   “导出修改后的 HTML”是可再次打开的离线备份，也不会自动写入设备。

若直接打开本地文件时浏览器不提供连接功能，可在仓库根目录运行
`python -m http.server 8765 --bind 127.0.0.1`，然后用 Chrome / Edge 打开
`http://localhost:8765/ports/stopwatch/editor.html`。不要使用不支持 WebHID 的内嵌预览。
协议使用独立的厂商 HID collection，标准键盘/鼠标/媒体报告不变，也不占用串口。

本版支持全部 50 个位置、五层、常用键码与组合键、已有 `&lt`、`&trans`、`&none`
和三个括号宏的重新分配。旋钮**按压**可改，旋钮**转动**仍为滚轮/音量；不支持编辑
宏内容、添加新行为、增减层或修改长按时间。左右键盘固件无需配合增加此功能。

保存键位使用独立 NVS 命名空间 `sw_keymap`，不会清除蓝牙配对。设备不在 USB 回调
中写 Flash；整份数据接收并验证后，才在输入任务中保存和应用。中途断线或无效绑定
不会应用半份键位；如保存结果因断线/超时不明确，请重新连接并读取确认。
普通固件更新保留已保存键位，修改 `.keymap` 默认值不会自动覆盖它们。
如行为/宏定义或层结构的兼容标识改变，旧保存数据会被忽略并使用固件默认预设，
编辑器也须同步更新；可事先导出 HTML 备份。

生成和检查页面（在仓库根目录）：

```sh
python ports/stopwatch/tools/build-editor.py
node ports/stopwatch/tests/editor-test.js
```

`editor.html` 是生成的独立文件，无 CDN 或在线脚本；修改源文件 `tools/editor-*.js`
后重新生成。基础图形编辑器沿用 `keymap-drawer/cornix.html`，普通 ZMK 编辑器不变。
本地已通过固件编译、主机逻辑与存储模拟测试、DOM/模拟 USB 流程测试；
浏览器权限、真实 USB 写入及实际断电恢复仍需本次实机验收。

## 构建

激活 ESP-IDF **5.4.4** 的环境，在此目录运行：

```sh
idf.py build
```

目标由 `sdkconfig.defaults` 固定为 `esp32s3`，Flash 16 MB，应用分区 8 MB。
姿态版启用 8 MB Octal PSRAM（40 MHz，仅专用分配）；约 424 KiB 用于旋转画布。依赖版本在 `dependencies.lock` 中锁定。
输出：`build/cornix_stopwatch.bin`、`build/bootloader/bootloader.bin`、
`build/partition_table/partition-table.bin`、`build/flash_args`。

独立工作流 **Build experimental StopWatch dongle** 会运行主机测试并构建下载包，
不会把 ESP32 目标加入 ZMK 的构建矩阵。

主机逻辑测试（仓库根目录，Python 3 + GCC / Clang / Zig）：

```sh
python ports/stopwatch/tools/test.py
# Windows 可指定编译器：
python ports/stopwatch/tools/test.py --cc path/to/zig.exe
```

## 首次刷入和配对

**从已正常输入的 StopWatch 版本升级：直接重新 build / flash 即可，无需刷左右键盘，
无需 settings-reset 或 erase-flash。** 每次 StopWatch 重启后，两侧各按一个普通键
（或转动对应旋钮）识别左右；同一次开机中的重连会保留身份对应关系。

1. 如需保留原厂应用及数据，先按 M5Stack 的说明备份；本固件会替换原厂应用。
2. StopWatch 接 USB，按设备文档进入下载模式：长按电源键约 2 秒至绿灯亮后松开。
   在 ESP-IDF 环境中运行（用实际端口替换 `COMx`）：

   ```sh
   idf.py -p COMx flash
   ```

   从 Actions 下载包刷入时，在解压后的目录运行：

   ```sh
   python -m esptool --chip esp32s3 --port COMx write_flash @flash_args
   ```

   使用 ESP-IDF 配套的 esptool 4.x；保留包内 `bootloader/` 和 `partition_table/`
   目录结构。不要把单独的应用 bin 写到地址 0。
3. 首次从旧接收器迁移，需要清除**左右键盘**原有的 split 配对；使用项目匹配的
   settings-reset 固件，然后恢复左 `cornix_ph_left//zmk` 和右 `cornix_right//zmk`
   的正常固件。不要把标准 central 左半 `cornix_left//zmk` 用作 dongle 左半。
4. 关闭旧 dongle，只给计划配对的两半上电。StopWatch 保存的配对少于两台时，
   每次启动开放 **60 秒**新配对窗口；满两台后只接受保存的身份。
   配对采用 BLE Just Works，窗口内附近其他 ZMK split 键盘也可能被选中。
5. 正常更新 StopWatch 应用不需要清空配对。若首次刷入出现 NVS 错误，或明确要重新
   配对，可在下载模式运行 `idf.py -p COMx erase-flash` 后重新 `flash`。
   **erase-flash 会删除整个 StopWatch Flash，包括原厂数据和配对。**
   更换身份后还须重置左右半的旧 split 配对。

应用运行时 USB 为 HID，**不提供串口**，`idf.py monitor` 不能通过此 USB 接口读取日志。
日志在 UART0 TX43/RX44；后盖复用引脚、电源和硬件版本应按官方引脚表核对。
再次刷入要重新进入下载模式。

## 实机验收

### 屏幕初始化速率

M5IOE1 使用 100 kHz 初始化。2026-09-27 实机对照：原流程 400 kHz 在 IOE
寄存器配置阶段返回 `0x103`，尚未开始画面传输；仅降到 100 kHz 后显示正常。
最终实现只保留这一项降速，已移除临时 USB 诊断和额外等待、重试、回读、顺序调整。
屏幕像素通过独立 QSPI 传输，因此该降速不降低动画帧率。

### 自动息屏

- 无新按键/旋钮操作 60 秒：亮度由 35% 降至 20%；120 秒：关闭 CO5300 显示输出。
- 息屏期间暂停 LVGL 刷新和动画更新，不持续发送屏幕像素；保持 BLE/USB 和键位引擎运行。
- 任意键盘按下（包括修饰键、层键）或当前滚轮/音量旋钮操作唤醒屏幕，操作照常发送给电脑，不吞首键。
- USB 挂起立即息屏；USB 重新连接/恢复后重新计时并亮屏。未连接 USB 时最多显示 120 秒。
- 唤醒时先刷新最新状态，再开启显示输出；不重新初始化面板或切换 IOE 供电。
- 本版不是整机深睡，也未接入触摸/机身按钮唤醒。长按不产生新按下事件，主机自动重复不重置计时。
- 减少点亮与刷新时间有助于降低屏幕老化和显示负载；实际温升改善需实机测量。

时间定义在 `main/display_model.h` 的 `DISPLAY_DIM_MS` / `DISPLAY_OFF_MS`，修改后需重新编译。
验收：保持连接静置两分钟，确认全黑；按一个普通键检查首键正常输入且亮屏；再试旋钮唤醒、
电脑睡眠/恢复。超时边界、恢复计时和毫秒回绕已由主机逻辑测试覆盖，屏幕开关仍需实机验证。

### 重力自动转正（实验功能，待实机验证）

BMI270 地址 0x68，与 IOE 共用 SDA47/SCL48；两者均保持 100 kHz。
仅开启加速度计（25 Hz），显示任务亮屏时约每 100 ms 采样，按屏幕平面重力方向选择 0/90/180/270 度补偿。
滤波后的方向稳定至少 300 ms 才应用，边界额外 10° 滞回；快速晃动、接近平放时保留上次角度。
首次开机平放而没有有效方向时使用默认方向。息屏不采样、不刷新，也不因转动亮屏。
唤醒先恢复上次角度，再在新姿态稳定后转正；按键、旋钮和 USB 唤醒逻辑保持原样。

- 约 424 KiB PSRAM 存储 LVGL 原始画面，内部 RAM 另加约 15 KiB DMA 条带缓冲。
- 整数索引四向旋转后分块送屏；方向不变时只传输脏区域覆盖的行带，切换方向时重绘全屏。
- 显示任务保持低优先级，分块主动让出 CPU；实际帧率、温升和键盘并发响应需实机测量。
- PSRAM 分配或 IMU 初始化失败时退回固定方向；连续 5 次读取失败后冻结上次方向。
- 使用 Bosch 官方 SensorAPI（BSD-3-Clause），版本与许可证见 `main/vendor/bmi270/`，
  下载包附 `BMI270-LICENSE.txt`。无需额外安装驱动组件。

已有构建目录不会自动覆盖旧 sdkconfig：在 menuconfig 的 SPI RAM 中启用 Octal / 40 MHz /
仅 capability allocator / 忽略 PSRAM 未发现错误，并在 Cornix StopWatch display 中启用
`Gravity-aligned display (experimental)`。本工作区已更新配置；全新构建使用 sdkconfig.defaults。
如果关闭姿态功能可保留此前固定布局进行对比。

安装轴先沿用官方 StopWatch HAL 的 X/Y 交换映射，尚未实机校准。
在 menuconfig 的 Cornix StopWatch display 下可用 `Mounting correction in degrees`
修正固定偏差；若旋转方向相反，可调整 `Invert IMU screen X/Y axis`。
验证请依次竖放、顺时针斜放 30°、侧放 90°、倒放 180°，每个姿态停留约 1 秒；
观察画面是否在四个方向切换；斜放 30° 时维持原方向。再平放旋转，应保持上次相对机身补偿而随机器一起转。
最后验证旋转过程中持续输入、两分钟息屏、换方向后键盘唤醒。

### 验收项目

本地完整编译和逻辑测试通过后，仍须用设备完成：

- USB 识别为 `StopWatch Dongle (experimental)`，连续输入与多键组合正常。
- 两半首次配对、先开任意一半、两半及 StopWatch 分别断电重启均可恢复连接。
- 五层映射、两只层键的短按 / 长按 / 滚动按法、Ctrl+Alt+Delete 和括号宏符合预期。
- 左旋钮双向滚动、右旋钮双向音量及两只旋钮按压。
- 按住修饰键或层键时关闭一半、拔 USB、电脑睡眠恢复，不出现持续按键。
- 至少一次长时间双手输入，检查丢键、重连风暴和队列溢出。
- 圆屏显示完整、方向与颜色正确，没有偏移或裁边；边缘偏移可通过 menuconfig
  的 Cornix StopWatch display / CO5300 horizontal RAM offset 调整（默认 6）。
- 两侧各按键后 L/R 对应正确；电量未知显示 `--%`，断连不显示旧电量。
- 长按层键显示实际层；输入后工作动画，3 秒无操作切回睡觉，60 秒后降亮度。
- USB 挂起息屏、恢复后亮屏；静置 120 秒息屏，按键/旋钮唤醒且输入正常。
- HTML 读取五层键位；修改一个普通键并保存，输入验证后重启确认保留。
- 修改组合键和长按切层，验证按下/松开；读取默认到草稿不会立即改变设备。
- 取消写入、断线重连、恢复默认与导出备份；确认配对始终保留。

## 硬件依据

来自用户提供的 `docs/assets/StopWatch.pdf` 和 [M5Stack 官方资料](https://docs.m5stack.com/en/core/StopWatch)：

| 功能 | StopWatch 引脚 / 器件 | 本版 |
|---|---|---|
| USB D− / D+ | GPIO19 / GPIO20，原生 USB OTG | 使用 |
| 蓝牙 | ESP32-S3 BLE | 使用 |
| 系统 I2C | SDA47 / SCL48 | 使用 |
| 圆屏 | CO5300，466 × 466，QSPI | 使用 |
| 屏幕总线 | CS39、CLK40、D0 41、D1 42、D2 46、D3 45、TE38 | 40 MHz QSPI；TE 未使用 |
| 屏幕复位 / 供电 | M5IOE1 PYG5 / PYG8 | 使用 |
| 触摸 | CST820B，INT13、复位 PYG4 | 未使用 |
| 电源 / IO 扩展 | M5PM1 / M5IOE1（0x4F，备选 0x6F） | 仅配置 IOE 屏幕引脚与关闭 IOE 休眠；不改 PMIC |

后盖 MUX_IO_1/2 的串口 / USB 复用由 M5IOE1 PYG1 控制，不等同于主 Type-C
插座的使用方式。v1.0 / v1.0.1 后盖 BAT 标识存在区别；本适配不使用后盖供电。

## Protocol

Protocol references (MIT-licensed ZMK source; implementation here is independent):

- [Split UUIDs](https://github.com/zmkfirmware/zmk/blob/main/app/include/zmk/split/bluetooth/uuid.h)
- [Split service and encrypted permissions](https://github.com/zmkfirmware/zmk/blob/main/app/src/split/bluetooth/service.c)
- [Packed sensor payload](https://github.com/zmkfirmware/zmk/blob/main/app/include/zmk/split/bluetooth/service.h)
- [Sensor channel layout](https://github.com/zmkfirmware/zmk/blob/main/app/include/zmk/sensors.h)
- [Encoder degree accumulation](https://github.com/zmkfirmware/zmk/blob/main/app/src/behaviors/behavior_sensor_rotate_common.c)

The service is `00000000-0096-7107-c967-c5cfb1c2482a`; position, sensor and layout
characteristics have first groups `00000001`, `00000003`, `00000005` respectively.
Positions are a 16-byte bitmap of **global** physical-layout positions. Both peers'
bitmaps are ORed, with no additional right-half offset. Layout index 0 selects this
project's 50-position layout. Sensors use the packed 14-byte little-endian ABI:
index, channel count, int32 degrees, int32 fractional degrees, int32 channel.
Current Cornix has 20 triggers per rotation (18° / detent); the legacy tick format
is also accepted. Protocol changes in future ZMK revisions require revalidation.

USB descriptor defaults are TinyUSB's development VID/PID, not an allocated
commercial product identity. USB uses 6-key rollover plus independent modifiers.

## 简化显示

已移除两侧彩色装饰柱及其更新逻辑，不采音。保留 180 × 180 全帧动画、横向镜像电池条、WPM 和层名，位置不变。HTML 已同步。

## 全帧动画与四向转正

固件保留本地 GIF 全部帧：输入 21 帧 / 880 ms，睡眠 35 帧 / 1640 ms，逐帧沿用 40/80 ms 时长。
动画调度独立于 100 ms 状态栏更新。重力只切换 0/90/180/270 度，带 10 度边界滞回与 300 ms 稳定等待；平放保持最后方向。
0 度直接拷贝，其他方向用整数像素索引，不再逐像素计算任意角度旋转。HTML 同步四向目标效果。
全帧 RGB565 约 5.8 MB；16 MB Flash 的 factory 分区由 4 MB 扩为 8 MB，起始地址和 NVS 不变。
首次升级务必使用完整 `idf.py -p COMx flash`，同时更新分区表和应用；不要仅刷应用 bin。无需擦除 NVS 或重新配对。
帧资源和时长与 GIF 一致；实际显示帧率仍受屏幕传输耗时影响，需实机验证。

当前动图尺寸调整为 180 × 180，中心保持 (233, 257)，左上角为 (143, 167)。全帧与原始时长不变，其余界面元素位置不变；全帧 RGB565 资源合计 3,628,800 字节，保留现有 8 MB 应用分区。

连接图标位置为 (216, 40)，占 34 × 34 px，与 HTML 预览一致。图标使用 LVGL 原生线条和圆点，不增加图片资源或动画定时器；仅状态改变时切换。猫、电量、WPM、层名位置不变，移除底部电脑连接提示，保留原有旋钮短暂提示。
