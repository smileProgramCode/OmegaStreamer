file(GLOB BASE_SRC *.cpp)
add_library(tms_base STATIC ${BASE_SRC})
target_link_libraries(tms_base PUBLIC spdlog::spdlog)