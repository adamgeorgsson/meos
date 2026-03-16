cmake_minimum_required(VERSION 3.20)
project(MeOS LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source files — dynamically enumerated, never hardcoded
# ---------------------------------------------------------------------------
file(GLOB MEOS_SOURCES     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB MEOS_RESOURCES   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.rc")

add_executable(MeOS WIN32
    ${MEOS_SOURCES}
    ${MEOS_RESOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/meos_dpi_manifest.xml
)

# ---------------------------------------------------------------------------
# Include directories
# Flat headers: use CMAKE_CURRENT_SOURCE_DIR so that bare includes work.
# All other vendored headers (mysql/, restbed/) are included with their
# subdirectory prefix, e.g. #include "mysql/mysql.h".
# libharu headers are provided by vcpkg (unofficial::libharu::hpdf target).
# ---------------------------------------------------------------------------
target_include_directories(MeOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# Compile definitions — match existing vcxproj per configuration/platform
# ---------------------------------------------------------------------------
target_compile_definitions(MeOS PRIVATE
    _WINDOWS
    $<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>
    # WIN32 define for 32-bit builds only (mirrors vcxproj behaviour)
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:WIN32>
)

# ---------------------------------------------------------------------------
# Compiler options (MSVC only)
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(MeOS PRIVATE
        /W3           # warning level 3
        /wd4267       # conversion from size_t to smaller type
        /wd4244       # conversion from larger to smaller integral type
        /wd4018       # signed/unsigned mismatch
        /MP           # multi-processor compilation
    )
endif()

# ---------------------------------------------------------------------------
# DPI manifest embedding
# ---------------------------------------------------------------------------
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/meos_dpi_manifest.xml
    PROPERTIES VS_TOOL_OVERRIDE "Manifest"
)

# ---------------------------------------------------------------------------
# External library paths
# Debug uses *_db variant; Release uses the plain variant.
# x64 (8-byte pointer) uses lib64_db / lib64.
# Win32 (4-byte pointer) uses lib_db / lib.
# ---------------------------------------------------------------------------
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_MEOS_LIB_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/lib64_db")
    set(_MEOS_LIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib64")
else()
    set(_MEOS_LIB_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/lib_db")
    set(_MEOS_LIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib")
endif()

target_link_directories(MeOS PRIVATE
    "$<IF:$<CONFIG:Debug>,${_MEOS_LIB_DEBUG},${_MEOS_LIB_RELEASE}>"
)

# ---------------------------------------------------------------------------
# vcpkg-managed libraries: zlib, minizip, libpng, libharu
# zlibstat.lib / zlibstat_vc15.lib (vendored combined zlib+minizip static lib)
# has been replaced by vcpkg's separate zlib and minizip packages.
# libpng.lib (vendored) has been replaced by vcpkg's libpng package.
# libharu.lib / libhpdf.lib (vendored) replaced by vcpkg's libharu package.
# ---------------------------------------------------------------------------
find_package(ZLIB REQUIRED)
find_package(unofficial-minizip CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(unofficial-libharu CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# OpenSSL — RestBed links dynamically against OpenSSL.
# Provided by vcpkg (declared in vcpkg.json); no OPENSSL_ROOT_DIR needed.
# ---------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)

target_link_libraries(MeOS PRIVATE
    # vcpkg-managed libraries (replaced vendored libs)
    ZLIB::ZLIB
    unofficial::minizip::minizip
    PNG::PNG
    unofficial::libharu::hpdf
    # Vendored static libraries (path resolved by target_link_directories)
    RestBed
    libmysql
    # OpenSSL (required by RestBed)
    OpenSSL::SSL
    OpenSSL::Crypto
    # Windows system libraries
    Msimg32
    comctl32
    odbc32
    odbccp32
    winmm
    ws2_32
    wininet
)
