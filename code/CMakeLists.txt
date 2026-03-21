cmake_minimum_required(VERSION 3.20)
project(MeOS LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source files (from MeOS.vcxproj ClCompile list)
# ---------------------------------------------------------------------------
set(SOURCES
    animationdata.cpp
    autocomplete.cpp
    autotask.cpp
    binencoder.cpp
    cardsystem.cpp
    classconfiginfo.cpp
    csvparser.cpp
    download.cpp
    gdioutput.cpp
    generalresult.cpp
    HTMLWriter.cpp
    image.cpp
    importformats.cpp
    infoserver.cpp
    iof30interface.cpp
    listeditor.cpp
    liveresult.cpp
    localizer.cpp
    machinecontainer.cpp
    maprenderer.cpp
    meos.cpp
    MeOSFeatures.cpp
    MeosSQL.cpp
    meosversion.cpp
    meos_util.cpp
    metalist.cpp
    methodeditor.cpp
    mysqldaemon.cpp
    mysqlwrapper.cpp
    newcompetition.cpp
    oBase.cpp
    oCard.cpp
    oClass.cpp
    oClub.cpp
    oControl.cpp
    oCourse.cpp
    oDataContainer.cpp
    oEvent.cpp
    oEventDraw.cpp
    oEventResult.cpp
    oEventSpeaker.cpp
    oEventSQL.cpp
    oevent_transfer.cpp
    oFreeImport.cpp
    oFreePunch.cpp
    oImportExport.cpp
    oListInfo.cpp
    onlineinput.cpp
    onlineresults.cpp
    oPunch.cpp
    oReport.cpp
    oRunner.cpp
    oTeam.cpp
    oTeamEvent.cpp
    parser.cpp
    pdfwriter.cpp
    prefseditor.cpp
    printer.cpp
    printresultservice.cpp
    progress.cpp
    qf_editor.cpp
    qualification_final.cpp
    random.cpp
    recorder.cpp
    restserver.cpp
    RestService.cpp
    RunnerDB.cpp
    socket.cpp
    speakermonitor.cpp
    SportIdent.cpp
    StdAfx.cpp
    TabAuto.cpp
    TabBase.cpp
    TabClass.cpp
    TabClub.cpp
    TabCompetition.cpp
    TabControl.cpp
    TabCourse.cpp
    Table.cpp
    TabList.cpp
    TabMulti.cpp
    TabRunner.cpp
    TabSI.cpp
    TabSpeaker.cpp
    TabTeam.cpp
    testmeos.cpp
    tests.cpp
    TimeStamp.cpp
    toolbar.cpp
    utm.cpp
    xmlparser.cpp
    zip.cpp
)

# Resource files
set(RESOURCES
    meos.rc
    meoslang.rc
)

# ---------------------------------------------------------------------------
# Executable target (WIN32 = GUI subsystem)
# ---------------------------------------------------------------------------
add_executable(MeOS WIN32 ${SOURCES} ${RESOURCES})

# DPI manifest — add as Manifest tool source in VS generator
target_sources(MeOS PRIVATE meos_dpi_manifest.xml)
set_source_files_properties(meos_dpi_manifest.xml PROPERTIES
    VS_TOOL_OVERRIDE "Manifest"
)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(MeOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
target_compile_definitions(MeOS PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
    _USE_MATH_DEFINES
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# ---------------------------------------------------------------------------
# MSVC compiler/linker options
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(MeOS PRIVATE
        /W3
        /wd4267
        /wd4244
        /wd4018
        /MP
    )

    # Whole-program optimisation for Release (/GL compile + /LTCG link)
    set_property(TARGET MeOS PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# ---------------------------------------------------------------------------
# vcpkg-managed libraries
# ---------------------------------------------------------------------------
find_package(ZLIB REQUIRED)
find_package(unofficial-minizip CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(unofficial-libharu CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# ---------------------------------------------------------------------------
# Library directories and external libraries (x64 layout)
# ---------------------------------------------------------------------------
set(LIB_DIR_DEBUG   ${CMAKE_CURRENT_SOURCE_DIR}/lib64_db)
set(LIB_DIR_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/lib64)

target_link_libraries(MeOS PRIVATE
    # vcpkg-managed libraries
    ZLIB::ZLIB
    unofficial::minizip::minizip
    PNG::PNG
    unofficial::libharu::hpdf
    # Vendored external libraries — Debug
    $<$<CONFIG:Debug>:${LIB_DIR_DEBUG}/RestBed.lib>
    $<$<CONFIG:Debug>:${LIB_DIR_DEBUG}/libmysql.lib>
    # Vendored external libraries — Release
    $<$<CONFIG:Release>:${LIB_DIR_RELEASE}/RestBed.lib>
    $<$<CONFIG:Release>:${LIB_DIR_RELEASE}/libmysql.lib>
    # Windows system libraries
    Msimg32
    comctl32
    odbc32
    odbccp32
    winmm
    ws2_32
    wininet
)
