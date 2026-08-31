cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(dotman C)

add_library(dotman_platform STATIC src/platform.c)
target_include_directories(dotman_platform PUBLIC src)

if(NOT WIN32)
  add_executable(dotman src/main.c)
  target_link_libraries(dotman PRIVATE dotman_platform)
endif()

if(NOT WIN32)
  enable_testing()
  add_executable(platform_test tests/platform_test.c)
  target_link_libraries(platform_test PRIVATE dotman_platform)
  add_test(NAME platform_test COMMAND platform_test)
endif()
