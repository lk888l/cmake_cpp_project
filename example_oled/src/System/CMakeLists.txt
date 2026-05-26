# 定义系统库
set(TARGET_NAME System_lib)

add_library(${TARGET_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/SystemState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EpollManager.cpp"
)

# 明确头文件路径，传播给调用者
target_include_directories(${TARGET_NAME} PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/SystemState"
    "${CMAKE_CURRENT_SOURCE_DIR}/Epoll"
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src        
)

target_compile_options(${TARGET_NAME} PRIVATE -Wno-missing-braces -Wno-unused-parameter)
