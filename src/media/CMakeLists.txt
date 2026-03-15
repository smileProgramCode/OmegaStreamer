file(GLOB MEDIA_BASE_SRC base/*.cpp)
file(GLOB MEDIA_RTMP_SRC rtmp/*.cpp)
file(GLOB MEDIA_RTMP_AMF_SRC rtmp/AMF/*.cpp)

add_library(tms_media STATIC ${MEDIA_BASE_SRC} ${MEDIA_RTMP_SRC} ${MEDIA_RTMP_AMF_SRC})
target_link_libraries(tms_media PUBLIC tms_network OpenSSL::SSL OpenSSL::Crypto)
