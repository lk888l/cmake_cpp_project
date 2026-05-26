add_subdirectory(Peripheral)

# 定义可执行文件目标
add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE peripheral_lib)

# 仅针对 my_app 这个目标设置头文件搜索路径
target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 静态编译优化（如前所述）
if(USE_STATIC_LINKING) # 可以在 Presets 中定义这个变量
    target_link_options(${PROJECT_NAME} PRIVATE -static -Wl,--gc-sections)
endif()

# 安装规则：定义执行 'install' 时文件去哪
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)