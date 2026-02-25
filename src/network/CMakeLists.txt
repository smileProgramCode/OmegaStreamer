file(GLOB NET_BASE_SRC base/*.cpp)
file(GLOB NET_NET_SRC net/*.cpp)
file(GLOB NET_SRC *.cpp)

add_library(tms_network STATIC ${NET_BASE_SRC} ${NET_NET_SRC} ${NET_SRC})
target_link_libraries(tms_network PUBLIC tms_base asio_config Threads::Threads)
