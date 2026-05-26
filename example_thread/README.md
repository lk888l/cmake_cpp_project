# Example_Cmake

基于cmake的luckfox_pico的工程模板



## 文件系统：

```Plaintext
example_cmake/
├── cmake/                      # 存放所有 CMake 辅助脚本
│   └── arm-rockchip830.cmake    # 你的工具链文件（移动到这里）
├── include/                    # 公共头文件 (.h, .hpp)
│   └── my_project/             # 用项目名做子目录，防止重名冲突
│       └── main.h
├── src/                        # 源代码文件 (.c, .cpp)
│   ├── CMakeLists.txt          # 核心构建逻辑
│   └── main.c
├── libs/                       # 第三方依赖库（源码或预编译库）
│   └── example_lib/
├── assets/                     # 存放要推送到板子上的非代码资源（图片、模型、配置）
├── scripts/                    # 存放部署脚本（如 rsync 同步、自动打包）
│   └── deploy.sh
├── .vscode/                    # VS Code 配置
│   └── settings.json
├── CMakeLists.txt              # 项目顶层入口
├── CMakePresets.json           # 跨平台/跨环境编译预设（核心）
└── README.md
```

