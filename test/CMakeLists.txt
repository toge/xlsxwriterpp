find_package(Catch2 REQUIRED CONFIG)

file(GLOB test_src test_*.cpp)

add_executable(all_test ${test_src})

target_link_libraries(all_test PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)
target_include_directories(all_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include)
target_compile_features(all_test PRIVATE cxx_std_23)

add_test(NAME all_test COMMAND all_test -r junit)

# ベンチマーク（Catch2 BENCHMARK 有効）
add_executable(benchmark_performance benchmark_performance.cpp)
target_compile_definitions(benchmark_performance PRIVATE CATCH_CONFIG_ENABLE_BENCHMARKING)
target_link_libraries(benchmark_performance PRIVATE xlsxwriterpp Catch2::Catch2WithMain)
target_include_directories(benchmark_performance PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include)
target_compile_features(benchmark_performance PRIVATE cxx_std_23)
