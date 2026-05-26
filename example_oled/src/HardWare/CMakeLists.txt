# 定义硬件库，包含 OLED 驱动源文件
set(TARGET_NAME HardWare_lib)

add_library(${TARGET_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/oled/HAL_OLED.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/oled/OLED_Data.cpp"
)

# 明确头文件路径，传播给调用者
target_include_directories(${TARGET_NAME} PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/oled"
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src        
)

target_compile_options(${TARGET_NAME} PRIVATE -Wno-missing-braces -Wno-unused-parameter)
