# app_task 模块 — 平台抽象的任务基类
# - AppTask          抽象基类
# - LinuxThreadTask  std::jthread 实现（Linux / 本地开发）
# - FreeRTOSTask     xTaskCreate 实现（ESP32 / FreeRTOS 移植时添加）

add_library(app_task STATIC
    app_task_linux.cpp
)

target_include_directories(app_task PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(app_task PUBLIC)
