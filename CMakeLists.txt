cmake_minimum_required(VERSION 3.25)
project(xlsxwriterpp
  VERSION 0.0.1
  DESCRIPTION "型安全・スパース遅延バッファ方式の libxlsxwriter C++ ラッパーライブラリ"
  LANGUAGES CXX)

# ──────────────────────────────────────────────────────────────
# オプション
# ──────────────────────────────────────────────────────────────
option(BUILD_TEST      "テストをビルドする"             ON)
option(BUILD_EXAMPLE   "examples/main.cpp をビルドする" OFF)

# ──────────────────────────────────────────────────────────────
# コンパイラ設定
# ──────────────────────────────────────────────────────────────
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
endif()

if (MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
endif()
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
endif()

option(STATIC "static binary" OFF)
if (CMAKE_COMPILER_IS_GNUCXX)
    if(STATIC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc")
    endif()
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ──────────────────────────────────────────────────────────────
# 依存ライブラリの検索
# ──────────────────────────────────────────────────────────────
find_package(xlsxwriter CONFIG QUIET)
if(NOT xlsxwriter_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XLSXWRITER IMPORTED_TARGET xlsxwriter)
    add_library(xlsxwriter::xlsxwriter ALIAS PkgConfig::XLSXWRITER)
endif()

# ──────────────────────────────────────────────────────────────
# INTERFACE ライブラリターゲットの定義
# ──────────────────────────────────────────────────────────────
add_library(xlsxwriterpp INTERFACE)
add_library(xlsxwriterpp::xlsxwriterpp ALIAS xlsxwriterpp)

target_compile_features(xlsxwriterpp INTERFACE cxx_std_26)

target_include_directories(xlsxwriterpp
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(xlsxwriterpp INTERFACE xlsxwriter::xlsxwriter)
target_compile_definitions(xlsxwriterpp INTERFACE XLSXWRITERPP_ENABLE_XLSXWRITER)

# ──────────────────────────────────────────────────────────────
# examples
# ──────────────────────────────────────────────────────────────
if(BUILD_EXAMPLE)
  add_executable(xlsxwriterpp_example examples/main.cpp)
  target_link_libraries(xlsxwriterpp_example PRIVATE xlsxwriterpp::xlsxwriterpp)
  if(MSVC)
    target_compile_options(xlsxwriterpp_example PRIVATE /W4 /utf-8)
  else()
    target_compile_options(xlsxwriterpp_example PRIVATE
      -Wall -Wextra -pedantic -Wno-missing-field-initializers)
  endif()
endif()

# ──────────────────────────────────────────────────────────────
# テスト
# ──────────────────────────────────────────────────────────────
if(BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

# ──────────────────────────────────────────────────────────────
# インストール設定
# ──────────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  TARGETS xlsxwriterpp
  EXPORT xlsxwriterppTargets)

install(
  EXPORT xlsxwriterppTargets
  FILE xlsxwriterppTargets.cmake
  NAMESPACE xlsxwriterpp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xlsxwriterpp)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/xlsxwriterppConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/xlsxwriterppConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xlsxwriterpp)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/xlsxwriterppConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/xlsxwriterppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/xlsxwriterppConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xlsxwriterpp)
