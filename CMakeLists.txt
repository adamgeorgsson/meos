cmake_minimum_required(VERSION 3.28)
project(meos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional: clang-tidy integration
option(MEOS_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
if(MEOS_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  else()
    message(WARNING "clang-tidy requested but not found")
  endif()
endif()

# Optional: code coverage
option(MEOS_ENABLE_COVERAGE "Enable code coverage (GCC/Clang)" OFF)
if(MEOS_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage)
    add_link_options(--coverage)
  else()
    message(WARNING "Code coverage is only supported with GCC or Clang")
  endif()
endif()

# Module libraries (dependency order: util -> domain -> db/io -> net -> app)
add_subdirectory(src/util)
add_subdirectory(src/domain)
add_subdirectory(src/db)
add_subdirectory(src/io)
add_subdirectory(src/net)
add_subdirectory(src/app)

# Main executable
add_executable(meos src/main.cpp)
target_link_libraries(meos PRIVATE meos_app)

# Testing
enable_testing()
add_subdirectory(tests)
