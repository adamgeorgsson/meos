add_library(meos_domain STATIC
  domain.cpp
  oBase.cpp
  oDataContainer.cpp
  oControl.cpp
  oPunch.cpp
  oClub.cpp
  domain_stubs.cpp
)
target_include_directories(meos_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_domain PUBLIC meos_util)
