cmake_minimum_required(VERSION 3.16)

project(test)

set(
    SOURCE
    source/main.cpp
)

find_package(SDL2 REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} ${SOURCE})

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
