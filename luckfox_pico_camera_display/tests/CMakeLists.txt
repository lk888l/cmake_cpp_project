add_executable(camera_display_core_tests core_tests.cpp)
target_link_libraries(camera_display_core_tests PRIVATE
    camera_display_core
    camera_display_st7789
)
add_test(NAME camera_display_core_tests COMMAND camera_display_core_tests)
