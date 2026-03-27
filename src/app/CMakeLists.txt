add_library(meos_app STATIC app.cpp)
target_include_directories(meos_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_app PUBLIC meos_net meos_io meos_domain meos_db meos_util)
