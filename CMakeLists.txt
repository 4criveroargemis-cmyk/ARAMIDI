cmake_minimum_required(VERSION 3.20)
project(aramidi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(RtMidi CONFIG REQUIRED)

add_executable(aramidi WIN32 main.cpp)

target_link_libraries(aramidi PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    imgui::imgui
    RtMidi::rtmidi
)

# Copy DLLS
add_custom_command(TARGET aramidi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:aramidi>
    $<TARGET_FILE_DIR:aramidi>
)

# Copy Fonts
add_custom_command(TARGET aramidi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Fonts
    $<TARGET_FILE_DIR:aramidi>/Fonts
)