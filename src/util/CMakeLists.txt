add_library(meos_util STATIC string_util.cpp time_util.cpp meos_util.cpp)
target_include_directories(meos_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
