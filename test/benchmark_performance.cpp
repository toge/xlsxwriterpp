#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <xlsxwriterpp.hpp>
#include <xlsxwriterpp/mock_backend.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace xlsxwriterpp;

namespace {

auto make_int_row(int row, int cols) -> void {
  auto backend = MockBackend{};
  auto ws      = Worksheet<MockBackend>{std::move(backend), "test"};
  for (auto c = 0; c < cols; ++c) {
    ws.write(row, c, CellValue{row * cols + c});
  }
  ws.flush_row(row);
}

auto make_string_row(int row, int cols) -> void {
  auto backend = MockBackend{};
  auto ws      = Worksheet<MockBackend>{std::move(backend), "test"};
  auto const s = std::string{"hello_xlsxwriterpp_"};
  for (auto c = 0; c < cols; ++c) {
    ws.write(row, c, CellValue{std::string_view{s}});
  }
  ws.flush_row(row);
}

auto make_formatted_int_row(int row, int cols) -> void {
  auto backend = MockBackend{};
  auto ws      = Worksheet<MockBackend>{std::move(backend), "test"};
  auto fmt     = FormatProperties{};
  fmt.bold     = true;
  for (auto c = 0; c < cols; ++c) {
    ws.write(row, c, CellValue{row * cols + c}, fmt);
  }
  ws.flush_row(row);
}

auto write_all_then_flush(int rows, int cols) -> void {
  auto backend = MockBackend{};
  auto ws      = Worksheet<MockBackend>{std::move(backend), "test"};
  for (auto r = 0; r < rows; ++r) {
    for (auto c = 0; c < cols; ++c) {
      ws.write(r, c, CellValue{r * cols + c});
    }
  }
  ws.flush_all();
}

} // namespace

// 行数パラメータ
constexpr auto COLS = 100;

TEST_CASE("Benchmark int cell writes", "[benchmark][int]") {
  for (auto rows : {1'000, 10'000, 100'000}) {
    BENCHMARK("int " + std::to_string(rows) + " rows") {
      return make_int_row(0, rows);
    };
  }
}

TEST_CASE("Benchmark string cell writes", "[benchmark][string]") {
  for (auto rows : {1'000, 10'000, 100'000}) {
    BENCHMARK("string " + std::to_string(rows) + " rows") {
      return make_string_row(0, rows);
    };
  }
}

TEST_CASE("Benchmark formatted int cell writes", "[benchmark][formatted]") {
  for (auto rows : {1'000, 10'000, 100'000}) {
    BENCHMARK("formatted int " + std::to_string(rows) + " rows") {
      return make_formatted_int_row(0, rows);
    };
  }
}

TEST_CASE("Benchmark write_all + flush_all", "[benchmark][flush]") {
  for (auto rows : {1'000, 10'000}) {
    BENCHMARK("flush_all " + std::to_string(rows) + "x" + std::to_string(COLS)) {
      return write_all_then_flush(rows, COLS);
    };
  }
}
