find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(meos_db STATIC database.cpp seed.cpp)
target_include_directories(meos_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_db PUBLIC meos_domain unofficial::sqlite3::sqlite3)
