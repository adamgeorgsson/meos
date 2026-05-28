add_executable(meos main.cpp app.cpp)
target_include_directories(meos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos PRIVATE meos_net)
