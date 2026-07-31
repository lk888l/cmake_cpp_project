find_package(Threads REQUIRED)

add_library(camera_display_linux STATIC
    linux/gpio_output.cpp
    linux/spi_bus.cpp
)
target_include_directories(camera_display_linux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(camera_display_linux PUBLIC camera_display_core)

add_library(camera_display_rockchip STATIC
    rockchip/dma_buffer.cpp
    rockchip/media_pipeline.cpp
    rockchip/preflight.cpp
    rockchip/probe.cpp
)
target_include_directories(camera_display_rockchip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(camera_display_rockchip PUBLIC camera_display_core Threads::Threads ${CMAKE_DL_LIBS})

set(_media_roots
    "${LUCKFOX_MEDIA_ROOT}"
    "${LUCKFOX_SDK_ROOT}/output/out/media_out"
    "${LUCKFOX_SDK_ROOT}/output/out"
    "${LUCKFOX_SDK_ROOT}/media/rockit/rockit/mpi/sdk"
    "${LUCKFOX_SDK_ROOT}/media/rockit/rockit"
    "${LUCKFOX_SDK_ROOT}/media/isp/release_camera_engine_rkaiq_rv1106_arm-rockchip830-linux-uclibcgnueabihf"
    "${LUCKFOX_SDK_ROOT}/media/rga/release_rga_rv1106_arm-rockchip830-linux-uclibcgnueabihf"
)
find_path(ROCKIT_INCLUDE_DIR rk_mpi_vi.h
    PATHS ${_media_roots}
    PATH_SUFFIXES include include/rockit
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
find_path(RKAIQ_INCLUDE_DIR rk_aiq_user_api2_sysctl.h
    PATHS ${_media_roots}
          "${LUCKFOX_SDK_ROOT}/media/isp/release_camera_engine_rkaiq_rv1106_arm-rockchip830-linux-uclibcgnueabihf"
    PATH_SUFFIXES include/rkaiq/uAPI2 include/uAPI2 include/rkaiq include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
find_path(RGA_INCLUDE_DIR im2d.h
    PATHS ${_media_roots}
          "${LUCKFOX_SDK_ROOT}/media/rga/release_rga_rv1106_arm-rockchip830-linux-uclibcgnueabihf"
    PATH_SUFFIXES include include/rga
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
find_library(ROCKIT_LIBRARY rockit PATHS ${_media_roots} PATH_SUFFIXES lib
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_library(RKAIQ_LIBRARY rkaiq PATHS ${_media_roots} PATH_SUFFIXES lib
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_library(RGA_LIBRARY rga PATHS ${_media_roots} PATH_SUFFIXES lib
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

foreach(required ROCKIT_INCLUDE_DIR RKAIQ_INCLUDE_DIR RGA_INCLUDE_DIR
                 ROCKIT_LIBRARY RKAIQ_LIBRARY RGA_LIBRARY)
    if(NOT ${required})
        message(FATAL_ERROR "${required} was not found. Build SDK media and set LUCKFOX_MEDIA_ROOT.")
    endif()
endforeach()

get_filename_component(RKAIQ_HEADERS_ROOT "${RKAIQ_INCLUDE_DIR}/.." ABSOLUTE)
target_include_directories(camera_display_rockchip SYSTEM PRIVATE
    ${ROCKIT_INCLUDE_DIR}
    ${RKAIQ_INCLUDE_DIR}
    ${RKAIQ_HEADERS_ROOT}
    ${RKAIQ_HEADERS_ROOT}/common
    ${RKAIQ_HEADERS_ROOT}/xcore
    ${RKAIQ_HEADERS_ROOT}/algos
    ${RKAIQ_HEADERS_ROOT}/iq_parser
    ${RKAIQ_HEADERS_ROOT}/iq_parser_v2
    ${RGA_INCLUDE_DIR}
)
target_link_libraries(camera_display_rockchip PUBLIC
    ${ROCKIT_LIBRARY}
    ${RKAIQ_LIBRARY}
    ${RGA_LIBRARY}
    m
    rt
)
