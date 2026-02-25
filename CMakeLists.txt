cmake_minimum_required(VERSION 3.14)
project(tms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0 -g -Wall -Wextra)
else()
    add_compile_options(-O2 -g -Wall -Wextra)
endif()

# ══════════════════════════════════════════════════════════
#  依赖管理：优先用 deps/ 本地源码，没有才 FetchContent
#
#  用法：
#    方式1（推荐，离线可用）：
#      mkdir deps && cd deps
#      git clone --depth 1 -b asio-1-28-0 https://github.com/chriskohlhoff/asio.git
#      git clone --depth 1 -b v1.12.0 https://github.com/gabime/spdlog.git
#
#    方式2（自动下载，需联网）：
#      不放 deps/ 目录，cmake 会自动 FetchContent
# ══════════════════════════════════════════════════════════

include(FetchContent)

# ─── Asio ───
if(EXISTS ${PROJECT_SOURCE_DIR}/deps/asio/include/asio.hpp)
    message(STATUS "Found local Asio in deps/asio")
    set(ASIO_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/deps/asio/include)
else()
    message(STATUS "deps/asio not found, fetching from GitHub...")
    FetchContent_Declare(asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG        asio-1-28-0
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(asio)
    set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)
endif()

# ─── spdlog ───
if(EXISTS ${PROJECT_SOURCE_DIR}/deps/spdlog/CMakeLists.txt)
    message(STATUS "Found local spdlog in deps/spdlog")
    add_subdirectory(deps/spdlog)
else()
    message(STATUS "deps/spdlog not found, fetching from GitHub...")
    FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.12.0
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# ─── OpenSSL (Complex Handshake 需要 HMAC-SHA256) ───
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# ─── Asio interface target ───
add_library(asio_config INTERFACE)
target_include_directories(asio_config INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio_config INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_libraries(asio_config INTERFACE Threads::Threads)

# ─── 全局头文件 ───
include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src)
