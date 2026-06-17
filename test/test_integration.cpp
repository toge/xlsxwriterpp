#include "catch2/catch_all.hpp"
#include <xlsxwriterpp/workbook.hpp>
#include <xlsxwriterpp/mock_backend.hpp>

using namespace xlsxwriterpp;

TEST_CASE("Integration: full workbook lifecycle") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};

  SECTION("create workbook, add sheets, write data, flush, save") {
    auto* ws1 = *wb.add_sheet("Sales");
    auto* ws2 = *wb.add_sheet("Expenses");

    REQUIRE(ws1->write(0, 0, "Item").has_value());
    REQUIRE(ws1->write(0, 1, "Amount").has_value());
    REQUIRE(ws1->write(1, 0, "Revenue").has_value());
    REQUIRE(ws1->write(1, 1, 1000.50).has_value());

    REQUIRE(ws2->write(0, 0, "Category").has_value());
    REQUIRE(ws2->write(0, 1, "Cost").has_value());
    REQUIRE(ws2->write(1, 0, "Rent").has_value());
    REQUIRE(ws2->write(1, 1, 500.00).has_value());

    auto flush_res = wb.flush_all_sheets();
    REQUIRE(flush_res.has_value());

    auto save_res = wb.save();
    REQUIRE(save_res.has_value());
  }
}

TEST_CASE("Integration: formatted cells") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};
  auto* ws = *wb.add_sheet("Formatted");

  FormatProperties header_fmt;
  header_fmt.bold = true;
  header_fmt.font_name = "Arial";
  header_fmt.font_size = 12.0;
  header_fmt.h_align = HorizontalAlign::Center;

  FormatProperties currency_fmt;
  currency_fmt.font_name = "Courier New";
  currency_fmt.font_size = 10.0;

  REQUIRE(ws->write(0, 0, "Product", header_fmt).has_value());
  REQUIRE(ws->write(0, 1, "Price", header_fmt).has_value());
  REQUIRE(ws->write(1, 0, "Widget").has_value());
  REQUIRE(ws->write(1, 1, 29.99, currency_fmt).has_value());

  auto flush_res = wb.flush_all_sheets();
  REQUIRE(flush_res.has_value());

  auto save_res = wb.save();
  REQUIRE(save_res.has_value());
}

TEST_CASE("Integration: format after write") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};
  auto* ws = *wb.add_sheet("FormatAfter");

  REQUIRE(ws->write(0, 0, "Data").has_value());

  FormatProperties fmt;
  fmt.italic = true;
  REQUIRE(ws->format(0, 0, fmt).has_value());

  auto flush_res = wb.flush_all_sheets();
  REQUIRE(flush_res.has_value());
}

TEST_CASE("Integration: row order in flush_row") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};
  auto* ws = *wb.add_sheet("RowOrder");

  REQUIRE(ws->write(0, 0, "Row0").has_value());
  REQUIRE(ws->write(1, 0, "Row1").has_value());
  REQUIRE(ws->write(2, 0, "Row2").has_value());

  REQUIRE(ws->flush_row(0).has_value());
  REQUIRE(ws->flush_row(1).has_value());
  REQUIRE(ws->flush_row(2).has_value());
}

TEST_CASE("Integration: multiple sheets with independent data") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};

  auto* ws1 = *wb.add_sheet("Sheet1");
  auto* ws2 = *wb.add_sheet("Sheet2");
  auto* ws3 = *wb.add_sheet("Sheet3");

  REQUIRE(ws1->write(0, 0, "A1").has_value());
  REQUIRE(ws2->write(0, 0, "B1").has_value());
  REQUIRE(ws3->write(0, 0, "C1").has_value());

  auto flush_res = wb.flush_all_sheets();
  REQUIRE(flush_res.has_value());

  auto save_res = wb.save();
  REQUIRE(save_res.has_value());
}
