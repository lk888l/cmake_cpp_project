# 定义接口库
add_library(peripheral_lib INTERFACE)

# 明确头文件路径，使用 INTERFACE 关键字以便传播给调用者
target_include_directories(peripheral_lib INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/uart"
    "${CMAKE_CURRENT_SOURCE_DIR}/Epoll"
    "${CMAKE_CURRENT_SOURCE_DIR}/i2c"
)