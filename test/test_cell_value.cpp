#include "catch2/catch_all.hpp"
#include <xlsxwriterpp/cell_value.hpp>

using namespace xlsxwriterpp;
using Catch::Approx;

TEST_CASE("to_stored converts CellValue to StoredCellValue") {
  SECTION("monostate") {
    CellValue v{std::monostate{}};
    auto stored = detail::to_stored(v);
    REQUIRE(std::holds_alternative<std::monostate>(stored));
  }

  SECTION("int") {
    CellValue v{42};
    auto stored = detail::to_stored(v);
    REQUIRE(std::holds_alternative<int>(stored));
    REQUIRE(std::get<int>(stored) == 42);
  }

  SECTION("double") {
    CellValue v{3.14};
    auto stored = detail::to_stored(v);
    REQUIRE(std::holds_alternative<double>(stored));
    REQUIRE(std::get<double>(stored) == Approx(3.14));
  }

  SECTION("string_view") {
    std::string s = "hello";
    CellValue v{std::string_view{s}};
    auto stored = detail::to_stored(v);
    REQUIRE(std::holds_alternative<std::string>(stored));
    REQUIRE(std::get<std::string>(stored) == "hello");
  }

  SECTION("bool") {
    CellValue v{true};
    auto stored = detail::to_stored(v);
    REQUIRE(std::holds_alternative<bool>(stored));
    REQUIRE(std::get<bool>(stored) == true);
  }
}

TEST_CASE("to_cell_value converts StoredCellValue to CellValue") {
  SECTION("monostate") {
    detail::StoredCellValue sv{std::monostate{}};
    auto cv = detail::to_cell_value(sv);
    REQUIRE(std::holds_alternative<std::monostate>(cv));
  }

  SECTION("int") {
    detail::StoredCellValue sv{100};
    auto cv = detail::to_cell_value(sv);
    REQUIRE(std::holds_alternative<int>(cv));
    REQUIRE(std::get<int>(cv) == 100);
  }

  SECTION("double") {
    detail::StoredCellValue sv{2.718};
    auto cv = detail::to_cell_value(sv);
    REQUIRE(std::holds_alternative<double>(cv));
    REQUIRE(std::get<double>(cv) == Approx(2.718));
  }

  SECTION("string") {
    detail::StoredCellValue sv{std::string{"world"}};
    auto cv = detail::to_cell_value(sv);
    REQUIRE(std::holds_alternative<std::string_view>(cv));
    REQUIRE(std::get<std::string_view>(cv) == "world");
  }

  SECTION("bool") {
    detail::StoredCellValue sv{false};
    auto cv = detail::to_cell_value(sv);
    REQUIRE(std::holds_alternative<bool>(cv));
    REQUIRE(std::get<bool>(cv) == false);
  }
}

TEST_CASE("roundtrip to_stored then to_cell_value preserves value") {
  SECTION("int") {
    CellValue original{999};
    auto cv = detail::to_cell_value(detail::to_stored(original));
    REQUIRE(std::get<int>(cv) == 999);
  }

  SECTION("double") {
    CellValue original{1.23};
    auto cv = detail::to_cell_value(detail::to_stored(original));
    REQUIRE(std::get<double>(cv) == Approx(1.23));
  }

  SECTION("string") {
    std::string s = "test string";
    CellValue original{std::string_view{s}};
    auto stored = detail::to_stored(original);
    auto cv = detail::to_cell_value(stored);
    REQUIRE(std::get<std::string_view>(cv) == "test string");
  }

  SECTION("bool") {
    CellValue original{true};
    auto cv = detail::to_cell_value(detail::to_stored(original));
    REQUIRE(std::get<bool>(cv) == true);
  }
}
