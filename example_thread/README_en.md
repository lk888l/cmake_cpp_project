# Example_Cmake

[中文](README_cn.md) | **English**

A CMake-based project template for Luckfox Pico.

## File layout

```text
example_cmake/
├── cmake/                       # CMake helper scripts
│   └── arm-rockchip830.cmake    # Cross-toolchain file
├── include/                     # Public headers (.h, .hpp)
│   └── my_project/
│       └── main.h
├── src/                         # Source files (.c, .cpp)
│   ├── CMakeLists.txt           # Core build logic
│   └── main.c
├── libs/                        # Third-party source or prebuilt libraries
│   └── example_lib/
├── assets/                      # Non-code resources deployed to the board
├── scripts/                     # Deployment and packaging scripts
│   └── deploy.sh
├── .vscode/
│   └── settings.json
├── CMakeLists.txt               # Top-level project entry
├── CMakePresets.json            # Cross-platform build presets
└── README.md
```
