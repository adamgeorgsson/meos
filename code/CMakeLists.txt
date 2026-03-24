cmake_minimum_required(VERSION 3.16)
project(MeOS LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg-provided packages (US-002/US-005)
find_package(ZLIB REQUIRED)
find_package(unofficial-minizip CONFIG REQUIRED)
# vcpkg-provided packages (US-003)
find_package(PNG REQUIRED)
# vcpkg-provided packages (US-004)
find_package(OpenSSL REQUIRED)
# vcpkg-provided packages (US-006)
find_package(unofficial-libmysql CONFIG REQUIRED)
# vcpkg-provided packages (US-007)
find_package(haru CONFIG REQUIRED)

# Dynamically discover all C++ sources in this directory (non-recursive,
# so subdirectories like minizip/, libharu/, etc. are not included)
file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")

# Resource files
list(APPEND SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/meos.rc"
    "${CMAKE_CURRENT_SOURCE_DIR}/meoslang.rc"
)

# WIN32: suppress console window (standard for GUI apps)
add_executable(MeOS WIN32 ${SOURCES})

# Unicode build (matches original VS project CharacterSet=Unicode)
target_compile_definitions(MeOS PRIVATE UNICODE _UNICODE)

# DPI-aware manifest: embed alongside default manifest (VS generator + MT tool)
target_sources(MeOS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/meos_dpi_manifest.xml")
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/meos_dpi_manifest.xml"
    PROPERTIES VS_TOOL_OVERRIDE "MT"
)

# Include paths for vendored third-party headers
# code/minizip removed: zlib and minizip headers are now provided by vcpkg (US-002/US-005)
# code/png removed: image.cpp uses "#include "png/png.h"" which resolves relative to code/ (US-003)
# code/libharu removed: pdfwriter.cpp uses "#include "hpdf.h""; vcpkg haru::haru provides include dirs (US-007)
target_include_directories(MeOS PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/mysql"
    "${CMAKE_CURRENT_SOURCE_DIR}/restbed"
)

# Vendored library search paths:
#   Release  → lib64/
#   Debug    → lib64_db/
target_link_directories(MeOS PRIVATE
    $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/lib64>
    $<$<NOT:$<CONFIG:Release>>:${CMAKE_CURRENT_SOURCE_DIR}/lib64_db>
)

# Link libraries
target_link_libraries(MeOS PRIVATE
    # Vendored third-party (resolved from lib64/ or lib64_db/ above)
    RestBed
    # vcpkg-provided: replaces vendored libharu.lib (US-007)
    haru::haru
    # vcpkg-provided: replaces vendored libmysql.lib (US-006)
    unofficial::libmysql::libmysql
    # vcpkg-provided: replaces Chocolatey OpenSSL; also satisfies RestBed's transitive dep (US-004)
    OpenSSL::SSL
    OpenSSL::Crypto
    # vcpkg-provided: replaces vendored libpng.lib (US-003)
    PNG::PNG
    # vcpkg-provided: replaces vendored zlibstat.lib which bundled zlib + minizip (US-002/US-005)
    ZLIB::ZLIB
    unofficial::minizip::minizip
    # Windows system libraries
    Msimg32
    comctl32
    odbc32
    odbccp32
    winmm
    ws2_32
    wininet
)

# MSVC-specific compiler and linker settings
if(MSVC)
    target_compile_options(MeOS PRIVATE
        # Suppress known legacy warnings
        /wd4267 /wd4244 /wd4018
        # Multi-processor compilation
        /MP
        # Whole program optimisation for Release
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(MeOS PRIVATE
        # Link-time code generation for Release
        $<$<CONFIG:Release>:/LTCG>
    )
endif()
