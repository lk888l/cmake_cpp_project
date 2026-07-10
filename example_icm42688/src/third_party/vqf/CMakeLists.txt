set(TARGET_NAME vqf_lib)

add_library(${TARGET_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vqf.cpp"
)

target_include_directories(${TARGET_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(NOT MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-pedantic)
endif()
