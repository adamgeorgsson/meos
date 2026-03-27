add_library(meos_db STATIC db.cpp)
target_include_directories(meos_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_db PUBLIC meos_domain meos_util)
