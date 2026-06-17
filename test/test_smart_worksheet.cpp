#include "catch2/catch_all.hpp"
#include <xlsxwriterpp/mock_backend.hpp>
#include <xlsxwriterpp/worksheet.hpp>

using namespace xlsxwriterpp;

TEST_CASE("Worksheet write") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "TestSheet"};

  SECTION("write int value") {
    auto res = ws.write(0, 0, 42);
    REQUIRE(res.has_value());
  }

  SECTION("write double value") {
    auto res = ws.write(1, 2, 3.14);
    REQUIRE(res.has_value());
  }

  SECTION("write string value") {
    std::string s = "hello";
    auto res = ws.write(2, 3, std::string_view{s});
    REQUIRE(res.has_value());
  }

  SECTION("write bool value") {
    auto res = ws.write(3, 4, true);
    REQUIRE(res.has_value());
  }

  SECTION("write with format") {
    FormatProperties fmt;
    fmt.bold = true;
    auto res = ws.write(5, 6, 100, fmt);
    REQUIRE(res.has_value());
  }
}

TEST_CASE("Worksheet format") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "TestSheet"};

  SECTION("format only") {
    FormatProperties fmt;
    fmt.italic = true;
    auto res = ws.format(0, 0, fmt);
    REQUIRE(res.has_value());
  }
}

TEST_CASE("Worksheet flush_row") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "TestSheet"};

  SECTION("flush empty row succeeds") {
    auto res = ws.flush_row(0);
    REQUIRE(res.has_value());
  }

  SECTION("flush row with data") {
    REQUIRE(ws.write(0, 0, 42).has_value());
    REQUIRE(ws.write(0, 1, 100).has_value());
    auto res = ws.flush_row(0);
    REQUIRE(res.has_value());
  }

  SECTION("row order violation") {
    REQUIRE(ws.flush_row(5).has_value());
    auto res = ws.flush_row(3);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == XlsxError::RowOrderViolation);
  }
}

TEST_CASE("Worksheet flush_all") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "TestSheet"};

  SECTION("flush_all with multiple rows") {
    REQUIRE(ws.write(0, 0, 10).has_value());
    REQUIRE(ws.write(1, 0, 20).has_value());
    REQUIRE(ws.write(2, 0, 30).has_value());
    auto res = ws.flush_all();
    REQUIRE(res.has_value());
  }

  SECTION("flush_all on empty worksheet") {
    auto res = ws.flush_all();
    REQUIRE(res.has_value());
  }
}

TEST_CASE("Worksheet sheet_name") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "MySheet"};
  REQUIRE(ws.sheet_name() == "MySheet");
}

TEST_CASE("Worksheet multiple columns in row") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "MultiCol"};

  REQUIRE(ws.write(0, 0, "A").has_value());
  REQUIRE(ws.write(0, 1, "B").has_value());
  REQUIRE(ws.write(0, 2, "C").has_value());
  REQUIRE(ws.write(0, 3, "D").has_value());

  auto res = ws.flush_row(0);
  REQUIRE(res.has_value());
}

TEST_CASE("Worksheet cell overwrite", "[worksheet]") {
  MockBackend backend;
  Worksheet<MockBackend> ws{std::move(backend), "OverwriteSheet"};

  SECTION("overwrite value") {
    REQUIRE(ws.write(0, 0, "first").has_value());
    REQUIRE(ws.write(0, 0, "second").has_value());
    auto res = ws.flush_row(0);
    REQUIRE(res.has_value());
  }

  SECTION("format then write") {
    FormatProperties fmt;
    fmt.bold = true;
    REQUIRE(ws.format(0, 0, fmt).has_value());
    REQUIRE(ws.write(0, 0, 42).has_value());
    auto res = ws.flush_row(0);
    REQUIRE(res.has_value());
  }

  SECTION("write then format") {
    REQUIRE(ws.write(0, 0, 42).has_value());
    FormatProperties fmt;
    fmt.italic = true;
    REQUIRE(ws.format(0, 0, fmt).has_value());
    auto res = ws.flush_row(0);
    REQUIRE(res.has_value());
  }
}
