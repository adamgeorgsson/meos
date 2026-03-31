find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(meos_net STATIC
    net.cpp
    ClubsApi.cpp
    ControlsApi.cpp
)
target_include_directories(meos_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_net PUBLIC meos_domain meos_util meos_db httplib::httplib nlohmann_json::nlohmann_json)
