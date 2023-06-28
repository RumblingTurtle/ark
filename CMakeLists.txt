cmake_minimum_required(VERSION 3.10)

project(ark)

find_package(SDL2 REQUIRED)
add_executable(ark main.cpp)
target_include_directories(ark PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})
target_link_libraries(ark PUBLIC SDL2)
