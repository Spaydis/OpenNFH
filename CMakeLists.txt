cmake_minimum_required(VERSION 3.25)

project(OpenNFH VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(opennfh_core INTERFACE)
target_include_directories(opennfh_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(opennfh_core INTERFACE /W4 /permissive-)
else()
    target_compile_options(opennfh_core INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_executable(opennfh
    src/app/main.cpp
)
target_link_libraries(opennfh PRIVATE opennfh_core)

include(CTest)
enable_testing()

add_executable(smoke_test
    tests/smoke_test.cpp
)
target_link_libraries(smoke_test PRIVATE opennfh_core)
add_test(NAME smoke_test COMMAND smoke_test)
