add_subdirectory(Peripheral)

# 定义可执行文件目标
add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE peripheral_lib)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(USE_STATIC_LINKING)
    target_link_options(${PROJECT_NAME} PRIVATE -static -Wl,--gc-sections)
endif()

# 安装规则：定义执行 'install' 时文件去哪
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)