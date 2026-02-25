add_executable(tms main.cpp)
target_link_libraries(tms PRIVATE
        tms_media tms_network tms_base
        asio_config spdlog::spdlog Threads::Threads
        OpenSSL::SSL OpenSSL::Crypto
)