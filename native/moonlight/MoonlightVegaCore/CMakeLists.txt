cmake_minimum_required(VERSION 3.15)

project(MoonlightVegaCore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_link_options("LINKER:--no-undefined" "LINKER:-z,defs")
endif()

set(REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(MOONLIGHT_COMMON_DIR "${REPOSITORY_ROOT}/third_party/moonlight-common-c")

if(NOT EXISTS "${MOONLIGHT_COMMON_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "moonlight-common-c is missing. Run: git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled native dependencies statically" FORCE)
set(CODE_ANALYSIS OFF CACHE BOOL "Disable dependency static analysis" FORCE)
add_subdirectory(
  "${MOONLIGHT_COMMON_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/moonlight-common-c"
  EXCLUDE_FROM_ALL
)

set(HEADERS
  kepler/core/ServerInfoClient.h
  kepler/turbo-modules/generated/MoonlightVegaCoreSpec.h
  kepler/turbo-modules/MoonlightVegaCore.h
)

set(SOURCES
  kepler/core/ServerInfoClient.cpp
  kepler/turbo-modules/generated/MoonlightVegaCoreSpec.cpp
  kepler/turbo-modules/MoonlightVegaCore.cpp
  kepler/AutoLinkInit.cpp
)

find_package(turbomoduleAPI CONFIG REQUIRED)

kepler_add_turbo_module_library(MoonlightVegaCore SHARED ${HEADERS} ${SOURCES})

target_include_directories(MoonlightVegaCore PRIVATE
  kepler
  "${MOONLIGHT_COMMON_DIR}/src"
)

target_link_libraries(MoonlightVegaCore PRIVATE moonlight-common-c)

install(TARGETS MoonlightVegaCore)

