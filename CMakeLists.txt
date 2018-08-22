
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(quasicrystal CXX)

find_package(SDL REQUIRED)
add_library(sdl INTERFACE IMPORTED)
target_link_libraries(sdl INTERFACE ${SDL_LIBRARY})
target_include_directories(sdl INTERFACE ${SDL_INCLUDE_DIR})

add_executable(crystal crystal.cpp render.cpp BMP.cpp Config.cpp)

target_link_libraries(crystal sdl m)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel")
  target_compile_options(crystal PRIVATE -Wall -Wextra)
endif()
