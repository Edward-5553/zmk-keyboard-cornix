# 月薪喵素材来源

角色及原始动画作者：**@月薪喵**。图像素材不属于本目录代码的 MIT 许可范围。

沿用 Sofle 项目已使用的素材，来自 [myunwang/LLMPET](https://github.com/myunwang/LLMPET)
固定提交 `22e34036c53bdbfe169dccde015781470476bbb5` 的 `assets/cat/`：

| 用途 | 原始文件 |
|---|---|
| 打字 | `cat-working-3.gif`（捂耳敲键盘） |
| 高速打字 | `cat-working-4.gif`（吃零食打字） |
| 闲置 | `cat-sleeping.gif`（被窝睡觉首帧） |

[上游署名说明](https://github.com/myunwang/LLMPET/blob/22e34036c53bdbfe169dccde015781470476bbb5/assets/cat/CREDITS.md)
将原始出处标为[《最近很火的月薪喵表情包第1弹》](https://www.mfuns.net/article/120254)，
注明版权归原作者、仅供个人桌宠皮肤使用、商用需联系原作者授权。
这里用于个人键盘显示，不主张拥有角色或图像版权，也不将代码许可扩展到图像。

转换：白底合成、32×32 Lanczos 缩小、阈值 150 无抖动单色化、每 100 ms 抽帧。
生成的 `widgets/salary_cat_images.c` 记录各 GIF 的 SHA256。
LVGL I1 数据包含白/黑双色表；每帧 136 字节，共 29 帧、3944 字节图像数据，
另有描述符和指针开销。预览图仅放大这些单色帧，实际屏幕大小为 32×32 像素。
