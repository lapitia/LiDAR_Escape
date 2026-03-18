cmake_minimum_required(VERSION 3.29)
project(lidar_escape)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
  set(SFML_DIR "C:/SFML/lib/cmake/SFML")
endif()

find_package(SFML 2.6.1 COMPONENTS graphics window system audio REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(lidar_escape main.cpp)

# Link SFML and OpenGL
target_link_libraries(lidar_escape
        sfml-graphics
        sfml-window
        sfml-system
        sfml-audio
        OpenGL::GL
        OpenGL::GLU
)

# Copy SFML DLLs after build
if (WIN32)
  add_custom_command(
        TARGET lidar_escape POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SFML_DIR}/../../../bin"
        $<TARGET_FILE_DIR:lidar_escape>
  )
endif()
