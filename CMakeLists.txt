cmake_minimum_required(VERSION 3.16)
project(image_processor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(image_processor
    src/main.cpp
    src/Image.cpp
)

target_include_directories(image_processor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(image_processor PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(image_processor PRIVATE /W4)
else()
    target_compile_options(image_processor PRIVATE -Wall -Wextra -Wpedantic)
endif()
