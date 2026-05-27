find_package(GTest REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Macro for standardized test registration
macro(meos_add_test TEST_NAME)
  add_executable(${TEST_NAME} ${ARGN})
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# Smoke test
meos_add_test(smoke_test smoke_test.cpp)

# Seed test
meos_add_test(seed_test seed_test.cpp)
target_link_libraries(seed_test PRIVATE meos_db)

# Clubs API integration test
meos_add_test(clubs_api_test clubs_api_test.cpp)
target_link_libraries(clubs_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Controls API integration test
meos_add_test(controls_api_test controls_api_test.cpp)
target_link_libraries(controls_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Courses API integration test
meos_add_test(courses_api_test courses_api_test.cpp)
target_link_libraries(courses_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)
