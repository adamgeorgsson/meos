add_library(meos_domain STATIC
  domain.cpp
  oBase.cpp
  oDataContainer.cpp
  oPunch.cpp
  oControl.cpp
  oClub.cpp
  oCourse.cpp
  oClass.cpp
  classconfiginfo.cpp
  qualification_final.cpp
  oCard.cpp
  oFreePunch.cpp
  oRunner.cpp
  oTeam.cpp
  oEventDraw.cpp
  oEventResult.cpp
  parser.cpp
  generalresult.cpp
)

target_include_directories(meos_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_domain PUBLIC meos_util)
target_compile_features(meos_domain PUBLIC cxx_std_17)
