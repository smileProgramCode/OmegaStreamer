cmake_minimum_required(VERSION 3.14)

#项目名称和版本
project(OmegaStreamer VERSION 0.1.0 LANGUAGES CXX)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录设置（上编译出来的可执行文件都在bin目录下）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =========================================================
# 第三方库集成 (Local Source Integration)
# =========================================================

# 1. Spdlog 集成
# Spdlog 自带很好的 CMake 支持，我们直接添加它的子目录
# 它会自动定义一个叫 spdlog::spdlog 的库给我们用

add_subdirectory(deps/spdlog)

# 2. ASIO 集成
# ASIO 是 Header-Only (主要部分)，不需要编译库文件
# 我们只需要定义一个 Interface Library 或者直接把它的 include 路径加进来
add_library(asio_lib INTERFACE)
target_include_directories(asio_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/include)

target_compile_definitions(asio_lib INTERFACE ASIO_STANDALONE)

# =========================================================
# 核心源码构建 (Manual Source Management)
# =========================================================

# 头文件路径

# [教学点] 手动列出源文件
# 以后每增加一个 .cpp，你都要来这里加一行
# 虽然麻烦一点，但你对项目的每一个文件都心知肚明
set(SERVER_SOURCES
    src/main.cpp
    # 比如以后我们会加: src/rtmp_server.cpp
    # src/rtmp_session.cpp
)

# 定义可执行文件
add_executable(${PROJECT_NAME} ${SERVER_SOURCES})


# 链接库
# 链接刚才定义的本地库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    asio_lib 
    spdlog::spdlog
)

# Linux 下必须链接线程库
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

message(STATUS "Build setup complete. Sources: ${SERVER_SOURCES}")