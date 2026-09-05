cmake_minimum_required(VERSION 3.25)

project(OpenNFH VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-linux")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-linux")
endif()

if(UNIX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-linux/lib")
    list(APPEND CMAKE_BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-linux/lib")
    list(APPEND CMAKE_BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-linux/debug/lib")
endif()
add_library(opennfh_core INTERFACE)
target_include_directories(opennfh_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_path(STB_IMAGE_INCLUDE_DIR NAMES stb_image.h REQUIRED)
add_library(opennfh_io STATIC
    src/io/zip_vfs.cpp
    src/io/data_root.cpp
    src/io/xml_fragments.cpp
    src/io/text_codec.cpp
)
target_link_libraries(opennfh_io PUBLIC opennfh_core libzip::zip pugixml::pugixml)

if(MSVC)
    target_compile_options(opennfh_core INTERFACE /W4 /permissive-)
else()
    target_compile_options(opennfh_core INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_executable(opennfh
    src/app/main.cpp
)
target_link_libraries(opennfh PRIVATE opennfh_presentation)

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

add_executable(xml_fragments_test
    tests/io/xml_fragments_test.cpp
)
target_include_directories(xml_fragments_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(xml_fragments_test PRIVATE opennfh_io)
add_test(NAME xml_fragments_test COMMAND xml_fragments_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(xml_fragments_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_executable(text_codec_test
    tests/io/text_codec_test.cpp
)
target_link_libraries(text_codec_test PRIVATE opennfh_io)
add_test(NAME text_codec_test COMMAND text_codec_test)
add_library(opennfh_media STATIC
    src/io/tga_decoder.cpp
    src/io/png_decoder.cpp
    src/io/audio_catalog.cpp
)
target_link_libraries(opennfh_media PUBLIC opennfh_core)
target_include_directories(opennfh_media PRIVATE "${STB_IMAGE_INCLUDE_DIR}")

add_executable(tga_decoder_test
    tests/io/tga_decoder_test.cpp
)
target_link_libraries(tga_decoder_test PRIVATE opennfh_media)
add_test(NAME tga_decoder_test COMMAND tga_decoder_test)

add_executable(png_decoder_test
    tests/io/png_decoder_test.cpp
)
target_link_libraries(png_decoder_test PRIVATE opennfh_media)
add_test(NAME png_decoder_test COMMAND png_decoder_test)

add_executable(audio_catalog_test
    tests/io/audio_catalog_test.cpp
)
target_link_libraries(audio_catalog_test PRIVATE opennfh_media)
add_test(NAME audio_catalog_test COMMAND audio_catalog_test)

add_library(opennfh_content STATIC
    src/content/loader.cpp
)
target_link_libraries(opennfh_content PUBLIC opennfh_io)

add_executable(content_loader_test
    tests/content/content_loader_test.cpp
)
target_include_directories(content_loader_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(content_loader_test PRIVATE opennfh_content)
add_test(NAME content_loader_test COMMAND content_loader_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(content_loader_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_library(opennfh_simulation STATIC
    src/simulation/world.cpp
    src/simulation/clock.cpp
    src/simulation/navigation.cpp
    src/simulation/scene.cpp
    src/simulation/input.cpp
)
target_link_libraries(opennfh_simulation PUBLIC opennfh_content)

add_executable(scene_test
    tests/simulation/scene_test.cpp
)
target_link_libraries(scene_test PRIVATE opennfh_simulation)
add_test(NAME scene_test COMMAND scene_test)

add_executable(input_test
    tests/simulation/input_test.cpp
)
target_link_libraries(input_test PRIVATE opennfh_simulation)
add_test(NAME input_test COMMAND input_test)

add_executable(navigation_test
    tests/simulation/navigation_test.cpp
)
target_link_libraries(navigation_test PRIVATE opennfh_simulation)
add_test(NAME navigation_test COMMAND navigation_test)

add_executable(clock_test
    tests/simulation/clock_test.cpp
)
target_link_libraries(clock_test PRIVATE opennfh_simulation)
add_test(NAME clock_test COMMAND clock_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(navigation_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_library(opennfh_gameplay STATIC
    src/simulation/actions.cpp
    src/simulation/inventory.cpp
    src/simulation/combinations.cpp
)
target_link_libraries(opennfh_gameplay PUBLIC opennfh_simulation)

add_executable(actions_test
    tests/simulation/actions_test.cpp
)
target_link_libraries(actions_test PRIVATE opennfh_gameplay)
add_test(NAME actions_test COMMAND actions_test)

add_executable(combinations_test
    tests/simulation/combinations_test.cpp
)
target_link_libraries(combinations_test PRIVATE opennfh_gameplay)
add_test(NAME combinations_test COMMAND combinations_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(actions_test combinations_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_library(opennfh_runtime STATIC
    src/simulation/control.cpp
    src/simulation/neighbor_ai.cpp
    src/simulation/progression.cpp
    src/simulation/replay.cpp
)
target_link_libraries(opennfh_runtime PUBLIC opennfh_gameplay)

add_executable(control_test tests/simulation/control_test.cpp)
target_link_libraries(control_test PRIVATE opennfh_runtime)
add_test(NAME control_test COMMAND control_test)

add_executable(neighbor_ai_test
    tests/simulation/neighbor_ai_test.cpp
)
target_link_libraries(neighbor_ai_test PRIVATE opennfh_runtime)
add_test(NAME neighbor_ai_test COMMAND neighbor_ai_test)

add_executable(progression_test
    tests/simulation/progression_test.cpp
)
target_link_libraries(progression_test PRIVATE opennfh_runtime)
add_test(NAME progression_test COMMAND progression_test)

add_executable(replay_test
    tests/simulation/replay_test.cpp
)
target_link_libraries(replay_test PRIVATE opennfh_runtime)
add_test(NAME replay_test COMMAND replay_test)

add_executable(replay_runner_test
    tests/simulation/replay_runner_test.cpp
)
target_link_libraries(replay_runner_test PRIVATE opennfh_runtime)
add_test(NAME replay_runner_test COMMAND replay_runner_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(neighbor_ai_test progression_test replay_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_library(opennfh_audio STATIC
    src/presentation/audio.cpp
)
target_link_libraries(opennfh_audio PUBLIC opennfh_io opennfh_media)

add_library(opennfh_presentation STATIC
    src/presentation/viewport.cpp
    src/presentation/renderer.cpp
    src/presentation/ui.cpp
    src/presentation/assets.cpp
    src/presentation/live.cpp
    src/presentation/wav_player.cpp
)
target_link_libraries(opennfh_presentation PUBLIC opennfh_runtime opennfh_audio SDL3::SDL3)
add_executable(viewport_test
    tests/presentation/viewport_test.cpp
)
target_link_libraries(viewport_test PRIVATE opennfh_presentation)
add_test(NAME viewport_test COMMAND viewport_test)

add_executable(layer_order_test
    tests/presentation/layer_order_test.cpp
)
target_link_libraries(layer_order_test PRIVATE opennfh_presentation)
add_test(NAME layer_order_test COMMAND layer_order_test)

add_executable(assets_test
    tests/presentation/assets_test.cpp
)
target_link_libraries(assets_test PRIVATE opennfh_presentation)
add_test(NAME assets_test COMMAND assets_test)

add_executable(live_options_test
    tests/presentation/live_options_test.cpp
)
target_link_libraries(live_options_test PRIVATE opennfh_presentation)
add_test(NAME live_options_test COMMAND live_options_test)

add_executable(wav_player_test
    tests/presentation/wav_player_test.cpp
)
target_link_libraries(wav_player_test PRIVATE opennfh_presentation)
add_test(NAME wav_player_test COMMAND wav_player_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(viewport_test layer_order_test assets_test live_options_test wav_player_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()

add_executable(ui_test
    tests/presentation/ui_test.cpp
)
target_include_directories(ui_test PRIVATE tests)
target_link_libraries(ui_test PRIVATE opennfh_presentation)
add_test(NAME ui_test COMMAND ui_test)

add_executable(presentation_audio_test
    tests/presentation/audio_catalog_test.cpp
)
target_link_libraries(presentation_audio_test PRIVATE opennfh_audio)
add_test(NAME presentation_audio_test COMMAND presentation_audio_test)

add_executable(local_corpus_test
    tests/integration/local_corpus_test.cpp
)
target_link_libraries(local_corpus_test PRIVATE opennfh_content opennfh_presentation)
add_test(NAME local_corpus_test COMMAND local_corpus_test)

add_executable(playable_slice_test
    tests/integration/playable_slice_test.cpp
)
target_link_libraries(playable_slice_test PRIVATE opennfh_content opennfh_presentation)
add_test(NAME playable_slice_test COMMAND playable_slice_test)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set_tests_properties(ui_test presentation_audio_test local_corpus_test playable_slice_test PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin;$ENV{PATH}"
    )
endif()
