cmake_minimum_required(VERSION 3.25)

project(OpenNFH VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
endif()

add_library(opennfh_core INTERFACE)
target_include_directories(opennfh_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

find_package(libzip CONFIG REQUIRED)
add_library(opennfh_io STATIC
    src/io/zip_vfs.cpp
    src/io/data_root.cpp
)
target_link_libraries(opennfh_io PUBLIC opennfh_core libzip::zip)

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

add_executable(zip_vfs_test
    tests/io/zip_vfs_test.cpp
)
target_include_directories(zip_vfs_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(zip_vfs_test PRIVATE opennfh_io)
add_test(NAME zip_vfs_test COMMAND zip_vfs_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(zip_vfs_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_executable(data_root_test
    tests/io/data_root_test.cpp
)
target_include_directories(data_root_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(data_root_test PRIVATE opennfh_io)
add_test(NAME data_root_test COMMAND data_root_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(data_root_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()
