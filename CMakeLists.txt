cmake_minimum_required(VERSION 3.20)
project(deepnestcpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
find_package(Threads REQUIRED)

set(CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIPPER2_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  clipper2
  GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
  GIT_TAG Clipper2_1.5.4
  SOURCE_SUBDIR CPP
)
FetchContent_MakeAvailable(clipper2)

add_library(deepnestcpp
  src/demo_cli.cpp
  src/demo_setup.cpp
  src/dxf_export.cpp
  src/geometry.cpp
  src/nfp_cache.cpp
  src/nfp.cpp
  src/placement.cpp
  src/orchestrator.cpp
)

target_include_directories(deepnestcpp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(deepnestcpp PUBLIC Clipper2 Threads::Threads)

add_executable(deepnestcpp_demo demo/main.cpp)
target_link_libraries(deepnestcpp_demo PRIVATE deepnestcpp)

include(CTest)
if(BUILD_TESTING)
  FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
  )
  FetchContent_MakeAvailable(catch2)

  add_executable(deepnestcpp_tests
    tests/test_demo_cli.cpp
    tests/test_demo_setup.cpp
    tests/test_dxf_export.cpp
    tests/test_geometry.cpp
    tests/test_nfp.cpp
    tests/test_placement.cpp
  )
  target_link_libraries(deepnestcpp_tests PRIVATE deepnestcpp Catch2::Catch2WithMain)

  include(Catch)
  catch_discover_tests(deepnestcpp_tests)
endif()
