# 定义接口库
add_library(peripheral_lib INTERFACE)

# 明确源文件路径[cite: 2]
target_sources(peripheral_lib INTERFACE 
    # "${CMAKE_CURRENT_SOURCE_DIR}/uart/uart.cpp"
)

# 明确头文件路径，使用 INTERFACE 关键字以便传播给调用者[cite: 2]
target_include_directories(peripheral_lib INTERFACE 
    # "${CMAKE_CURRENT_SOURCE_DIR}/uart"
)