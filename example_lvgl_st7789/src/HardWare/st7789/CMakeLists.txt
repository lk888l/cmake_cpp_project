add_library(hardware_st7789_lib STATIC st7789.cpp)
target_include_directories(hardware_st7789_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hardware_st7789_lib PUBLIC peripheral_lib)

