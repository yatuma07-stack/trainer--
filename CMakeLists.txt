cmake_minimum_required(VERSION 3.15)
project(aimtrainer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib: use an installed copy if there is one, otherwise download 5.5.
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(aimtrainer src/main.cpp)
target_link_libraries(aimtrainer PRIVATE raylib)
if(MSVC)
  target_compile_options(aimtrainer PRIVATE /W3)
else()
  target_compile_options(aimtrainer PRIVATE -Wall -Wextra)
endif()

# Copy assets (config.ini, maps/, sounds/, models/, sky/) next to the executable.
add_custom_command(TARGET aimtrainer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:aimtrainer>)

# Logic tests (no graphics needed): ctest, or ./sim_test
enable_testing()
add_executable(sim_test tests/sim_test.cpp)
add_test(NAME sim_test COMMAND sim_test)
