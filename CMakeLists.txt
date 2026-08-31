cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(dotman C)

add_executable(dotman src/main.c src/platform.c)

enable_testing()
add_executable(platform_test tests/platform_test.c src/platform.c)
target_include_directories(platform_test PRIVATE src)
add_test(NAME platform_test COMMAND platform_test)
