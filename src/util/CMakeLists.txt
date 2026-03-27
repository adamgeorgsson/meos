add_library(meos_util STATIC meos_util.cpp)
target_include_directories(meos_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(meos_util PUBLIC cxx_std_17)
