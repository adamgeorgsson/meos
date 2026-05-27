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

# Classes API integration test
meos_add_test(classes_api_test classes_api_test.cpp)
target_link_libraries(classes_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Runners API integration test
meos_add_test(runners_api_test runners_api_test.cpp)
target_link_libraries(runners_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Teams API integration test
meos_add_test(teams_api_test teams_api_test.cpp)
target_link_libraries(teams_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Competitions API integration test
meos_add_test(competitions_api_test competitions_api_test.cpp)
target_link_libraries(competitions_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Results API integration test
meos_add_test(results_api_test results_api_test.cpp)
target_link_libraries(results_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# Start list API integration test
meos_add_test(startlist_api_test startlist_api_test.cpp)
target_link_libraries(startlist_api_test PRIVATE meos_net nlohmann_json::nlohmann_json)

# XML export integration test
meos_add_test(xml_export_test xml_export_test.cpp)
target_link_libraries(xml_export_test PRIVATE meos_net)

# Domain oBase unit test
meos_add_test(domain_obase_test domain_obase_test.cpp)
target_link_libraries(domain_obase_test PRIVATE meos_domain)

# Domain oControl unit test
meos_add_test(domain_ocontrol_test domain_ocontrol_test.cpp)
target_link_libraries(domain_ocontrol_test PRIVATE meos_domain)

# Domain oPunch unit test
meos_add_test(domain_opunch_test domain_opunch_test.cpp)
target_link_libraries(domain_opunch_test PRIVATE meos_domain)

# Domain oClub unit test
meos_add_test(domain_oclub_test domain_oclub_test.cpp)
target_link_libraries(domain_oclub_test PRIVATE meos_domain)

# Domain oCourse unit test
meos_add_test(domain_ocourse_test domain_ocourse_test.cpp)
target_link_libraries(domain_ocourse_test PRIVATE meos_domain)

# Domain ClassConfigInfo + QualificationFinal unit test
meos_add_test(domain_classconfiginfo_qf_test domain_classconfiginfo_qf_test.cpp)
target_link_libraries(domain_classconfiginfo_qf_test PRIVATE meos_domain)

# Domain oClass unit test
meos_add_test(domain_oclass_test domain_oclass_test.cpp)
target_link_libraries(domain_oclass_test PRIVATE meos_domain)

# Domain oCard unit test
meos_add_test(domain_ocard_test domain_ocard_test.cpp)
target_link_libraries(domain_ocard_test PRIVATE meos_domain meos_util)

# Domain oFreePunch unit test
meos_add_test(domain_ofreepunch_test domain_ofreepunch_test.cpp)
target_link_libraries(domain_ofreepunch_test PRIVATE meos_domain)

# Domain oRunner unit test
meos_add_test(domain_orunner_test domain_orunner_test.cpp)
target_link_libraries(domain_orunner_test PRIVATE meos_domain)

# Domain oRunner result / evaluateCard test (US-003g2)
meos_add_test(domain_orunner_result_test domain_orunner_result_test.cpp)
target_link_libraries(domain_orunner_result_test PRIVATE meos_domain)

# Domain oTeam unit test (US-003h1)
meos_add_test(domain_oteam_test domain_oteam_test.cpp)
target_link_libraries(domain_oteam_test PRIVATE meos_domain)
