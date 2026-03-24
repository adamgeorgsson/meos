cmake_minimum_required(VERSION 3.20)
project(MeOS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)
find_package(unofficial-minizip CONFIG REQUIRED)

file(GLOB SOURCES "*.cpp")

add_executable(MeOS WIN32
    ${SOURCES}
    meos.rc
    meoslang.rc
)

set_target_properties(MeOS PROPERTIES
    VS_APPLICATION_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/meos_dpi_manifest.xml"
)

target_include_directories(MeOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libharu
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql
    ${CMAKE_CURRENT_SOURCE_DIR}/png
    ${CMAKE_CURRENT_SOURCE_DIR}/restbed
    ${CMAKE_CURRENT_SOURCE_DIR}/minizip
)

target_link_directories(MeOS PRIVATE
    $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/lib64>
    $<$<NOT:$<CONFIG:Release>>:${CMAKE_CURRENT_SOURCE_DIR}/lib64_db>
)

target_link_libraries(MeOS PRIVATE
    libharu
    libmysql
    libpng
    ZLIB::ZLIB
    unofficial::minizip::minizip
    RestBed
    Msimg32
    comctl32
    odbc32
    odbccp32
    winmm
    ws2_32
    wininet
)

target_compile_definitions(MeOS PRIVATE
    UNICODE
    _UNICODE
)

target_compile_options(MeOS PRIVATE
    /wd4267
    /wd4244
    /wd4018
    $<$<CONFIG:Release>:/GL>
    $<$<CONFIG:Release>:/MP>
)

target_link_options(MeOS PRIVATE
    $<$<CONFIG:Release>:/LTCG>
)

add_custom_command(TARGET MeOS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Lists"
        "$<TARGET_FILE_DIR:MeOS>"
    COMMENT "Copying Lists data files to output directory"
)
