cmake_minimum_required(VERSION 3.16)

project(Shift_game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(Shift_game
    src/main.cpp
)

target_link_libraries(Shift_game
    PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)