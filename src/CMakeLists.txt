cmake_minimum_required(VERSION 3.13)
project(shortwave_station_list_sdrpp)

find_package(CURL)
if(NOT ${CURL_FOUND})
    message(FATAL_ERROR "Could not find libcurl4 dev package (install libcurl4-gnutls-dev or libcurl4-openssl-dev)")
endif()

file(GLOB_RECURSE SRC "*.cpp")

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(shortwave_station_list_sdrpp SHARED ${SRC})

target_link_libraries(shortwave_station_list_sdrpp PRIVATE sdrpp_core)
target_link_libraries(shortwave_station_list_sdrpp PRIVATE ${CURL_LIBRARIES})

if(MSVC)
    target_compile_options(shortwave_station_list_sdrpp PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(shortwave_station_list_sdrpp PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else()
    target_compile_options(shortwave_station_list_sdrpp PRIVATE -O3 -std=c++17)
endif()
