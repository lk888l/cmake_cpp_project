add_library(menu_st7789 STATIC st7789.cpp)
target_include_directories(menu_st7789 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(menu_st7789 PUBLIC menu_bsp)
