add_library(meos_net STATIC net.cpp)
target_include_directories(meos_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_net PUBLIC meos_db)
