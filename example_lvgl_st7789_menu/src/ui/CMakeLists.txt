add_library(menu_ui STATIC motion.cpp)
target_include_directories(menu_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(menu_ui PUBLIC lvgl)
