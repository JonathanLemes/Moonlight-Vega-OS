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
set(MBEDTLS_DIR "${REPOSITORY_ROOT}/third_party/mbedtls")

if(NOT EXISTS "${MOONLIGHT_COMMON_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "moonlight-common-c is missing. Run: git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled native dependencies statically" FORCE)
set(CODE_ANALYSIS OFF CACHE BOOL "Disable dependency static analysis" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Do not build Mbed TLS programs" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Do not build Mbed TLS tests" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Build static Mbed TLS" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Build static Mbed TLS" FORCE)

if(NOT EXISTS "${MBEDTLS_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "Mbed TLS is missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(
  "${MBEDTLS_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/mbedtls"
  EXCLUDE_FROM_ALL
)

set(USE_MBEDTLS ON CACHE BOOL "Use bundled Mbed TLS for moonlight-common-c" FORCE)
add_subdirectory(
  "${MOONLIGHT_COMMON_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/moonlight-common-c"
  EXCLUDE_FROM_ALL
)

set(HEADERS
  kepler/core/FragmentedMp4.h
  kepler/core/GameStreamClient.h
  kepler/core/ServerInfoClient.h
  kepler/core/StreamSession.h
  kepler/turbo-modules/generated/MoonlightVegaCoreSpec.h
  kepler/turbo-modules/MoonlightVegaCore.h
)

set(SOURCES
  kepler/core/FragmentedMp4.cpp
  kepler/core/GameStreamClient.cpp
  kepler/core/ServerInfoClient.cpp
  kepler/core/StreamSession.cpp
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

target_link_libraries(MoonlightVegaCore PRIVATE
  moonlight-common-c
  mbedcrypto
  mbedtls
  mbedx509
)

install(TARGETS MoonlightVegaCore)
