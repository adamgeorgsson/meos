find_package(GTest REQUIRED)

# Macro for standardized test registration
macro(meos_add_test TEST_NAME)
  add_executable(${TEST_NAME} ${ARGN})
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# Smoke test
meos_add_test(smoke_test smoke_test.cpp)

# Util unit tests
meos_add_test(util_test util_test.cpp)
target_link_libraries(util_test PRIVATE meos_util)

# XML parser tests
meos_add_test(xmlparser_test xmlparser_test.cpp)
target_link_libraries(xmlparser_test PRIVATE meos_util)

# CSV parser tests
meos_add_test(csvparser_test csvparser_test.cpp)
target_link_libraries(csvparser_test PRIVATE meos_util)

# TimeStamp tests
meos_add_test(timestamp_test timestamp_test.cpp)
target_link_libraries(timestamp_test PRIVATE meos_util)
