# UI font source and license

[中文](FONT_LICENSE_cn.md) | **English**

`lv_font_ui_cn_16.c` and `lv_font_ui_cn_20.c` are generated subsets of
**Noto Sans SC**, using the locally installed `NotoSansSC-VF.ttf` source.
Noto fonts are distributed under the [SIL Open Font License 1.1](https://openfontlicense.org/).
Only the generated LVGL bitmap subsets are required to build this project.

The subsets contain only the Chinese characters, Latin letters, digits, spaces,
and punctuation that can be shown by the application. They were generated with
`lv_font_conv` in LVGL C format, 4 bits per pixel, at 16 px and 20 px.
Regenerate both files whenever visible UI text changes, otherwise a missing
glyph will render as a placeholder.
