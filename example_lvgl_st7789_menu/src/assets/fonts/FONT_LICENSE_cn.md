# UI 字体来源与许可证

**中文** | [English](FONT_LICENSE_en.md)

`lv_font_ui_cn_16.c` 和 `lv_font_ui_cn_20.c` 是使用本机
`NotoSansSC-VF.ttf` 生成的 **Noto Sans SC** 子集。Noto 字体依据
[SIL Open Font License 1.1](https://openfontlicense.org/) 分发。构建本项目
只需要生成后的 LVGL 位图子集。

子集只包含应用可能显示的中文字符、拉丁字母、数字、空格和标点。它们由
`lv_font_conv` 生成，格式为 LVGL C、每像素 4 位、字号 16 px 和 20 px。
每次可见 UI 文本变化后都要重新生成两个文件，否则缺失字形会显示为占位符。
