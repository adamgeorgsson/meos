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

# Localizer tests
meos_add_test(localizer_test localizer_test.cpp)
target_link_libraries(localizer_test PRIVATE meos_util)

# Domain tests (US-003a)
meos_add_test(domain_test domain_test.cpp)
target_link_libraries(domain_test PRIVATE meos_domain meos_util)

# Control and punch tests (US-003b)
meos_add_test(control_test control_test.cpp)
target_link_libraries(control_test PRIVATE meos_domain meos_util)

# Club tests (US-003c)
meos_add_test(club_test club_test.cpp)
target_link_libraries(club_test PRIVATE meos_domain meos_util)

# Course tests (US-003d)
meos_add_test(course_test course_test.cpp)
target_link_libraries(course_test PRIVATE meos_domain meos_util)

# Class tests (US-003e)
meos_add_test(class_test class_test.cpp)
target_link_libraries(class_test PRIVATE meos_domain meos_util)

# Card and FreePunch tests (US-003f)
meos_add_test(card_test card_test.cpp)
target_link_libraries(card_test PRIVATE meos_domain meos_util)

# Runner tests (US-003g)
meos_add_test(runner_test runner_test.cpp)
target_link_libraries(runner_test PRIVATE meos_domain meos_util)

# Team tests (US-003h)
meos_add_test(team_test team_test.cpp)
target_link_libraries(team_test PRIVATE meos_domain meos_util)

# Event aggregate root tests (US-003i)
meos_add_test(event_test event_test.cpp)
target_link_libraries(event_test PRIVATE meos_domain meos_util)

# GeneralResult + result engine tests (US-003j)
meos_add_test(generalresult_test generalresult_test.cpp)
target_link_libraries(generalresult_test PRIVATE meos_domain meos_util)

# SQLite database + migration tests (US-004a)
meos_add_test(db_test db_test.cpp)
target_link_libraries(db_test PRIVATE meos_db)
