add_library(camera_display_st7789 STATIC st7789.cpp)
target_include_directories(camera_display_st7789 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(camera_display_st7789 PUBLIC camera_display_core)
