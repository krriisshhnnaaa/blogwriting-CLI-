cmake_minimum_required(VERSION 3.16)
project(blogWRITER CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)

file(GLOB SOURCES "src/*.cpp")

add_executable(blogwriter ${SOURCES})

target_link_libraries(blogwriter PRIVATE
    CURL::libcurl
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)
