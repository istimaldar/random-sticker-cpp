cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(random_sticker_sender)
add_subdirectory(thirdparty/td)
add_subdirectory(thirdparty/CLI11)

set(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -O3 -DNDEBUG -s")

add_executable(random_sticker_sender main.cpp telegram_client.hpp utility.hpp)
target_link_libraries(random_sticker_sender PRIVATE Td::TdStatic CLI11::CLI11)
