# 月薪喵素材来源

角色及原始动画作者：**@月薪喵**。图像素材不属于本目录代码的 MIT 许可范围。

沿用 Sofle 项目已使用的素材，来自 [myunwang/LLMPET](https://github.com/myunwang/LLMPET)
固定提交 `22e34036c53bdbfe169dccde015781470476bbb5` 的 `assets/cat/`：

| 用途 | 原始文件 |
|---|---|
| 启动后尚未输入，或最后一次按键后已满 3 秒 | `cat-idle.gif`（循环播放） |
| 首次按键立即切换，后续每次按键延长至 3 秒后 | `cat-error.gif`（循环播放） |

[上游署名说明](https://github.com/myunwang/LLMPET/blob/22e34036c53bdbfe169dccde015781470476bbb5/assets/cat/CREDITS.md)
将原始出处标为[《最近很火的月薪喵表情包第1弹》](https://www.mfuns.net/article/120254)，
注明版权归原作者、仅供个人桌宠皮肤使用、商用需联系原作者授权。
这里用于个人键盘显示，不主张拥有角色或图像版权，也不将代码许可扩展到图像。

转换：白底合成、32×32 Lanczos 缩小、阈值 150 无抖动单色化、2 倍最近邻放大到 64×64、每 100 ms 抽帧。
生成的 `widgets/salary_cat_images.c` 记录各 GIF 的 SHA256。
LVGL I1 数据包含黑/白双色表；每帧 520 字节，idle 3 帧、error 4 帧，共 7 帧、3640 字节图像数据，
另有描述符和指针开销。实际动画尺寸为 64×64 像素，预先生成，运行时不做缩放。
