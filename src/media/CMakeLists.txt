file(GLOB MEDIA_BASE_SRC base/*.cpp)
file(GLOB MEDIA_RTMP_SRC rtmp/*.cpp)

add_library(tms_media STATIC ${MEDIA_BASE_SRC} ${MEDIA_RTMP_SRC})
target_link_libraries(tms_media PUBLIC tms_network OpenSSL::SSL OpenSSL::Crypto)
