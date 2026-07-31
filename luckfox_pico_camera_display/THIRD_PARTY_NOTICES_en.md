# Third-party notices

[中文](THIRD_PARTY_NOTICES_cn.md) | **English**

This application dynamically uses the Rockchip media runtime already installed by
the Luckfox board image:

- Rockit (`librockit.so`)
- Rockchip AIQ (`librkaiq.so`)
- Rockchip RGA (`librga.so`)

Those libraries and their licenses are distributed by Luckfox/Rockchip and are
not copied into this repository or the deployment archive. Consult the license
files in the pinned Luckfox SDK and the board firmware before redistribution.

The application itself contains no OpenCV, FFmpeg, GStreamer, LVGL, or copied
font package.
