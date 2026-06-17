#include "catch2/catch_all.hpp"
#include <xlsxwriterpp/workbook.hpp>
#include <xlsxwriterpp/mock_backend.hpp>

using namespace xlsxwriterpp;

TEST_CASE("Workbook add_sheet") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};

  SECTION("add single sheet") {
    auto res = wb.add_sheet("Sheet1");
    REQUIRE(res.has_value());
    REQUIRE(*res != nullptr);
  }

  SECTION("add multiple sheets") {
    auto r1 = wb.add_sheet("Sheet1");
    auto r2 = wb.add_sheet("Sheet2");
    auto r3 = wb.add_sheet("Sheet3");
    REQUIRE(r1.has_value());
    REQUIRE(r2.has_value());
    REQUIRE(r3.has_value());
    REQUIRE(wb.sheet_count() == 3);
  }

  SECTION("duplicate sheet name") {
    wb.add_sheet("Sheet1");
    auto res = wb.add_sheet("Sheet1");
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == XlsxError::DuplicateSheetName);
  }
}

TEST_CASE("Workbook get_sheet by name") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};
  wb.add_sheet("Alpha");
  wb.add_sheet("Beta");

  SECTION("existing sheet") {
    auto res = wb.get_sheet("Alpha");
    REQUIRE(res.has_value());
    REQUIRE(*res != nullptr);
    REQUIRE((*res)->sheet_name() == "Alpha");
  }

  SECTION("non-existing sheet") {
    auto res = wb.get_sheet("Gamma");
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == XlsxError::SheetNotFound);
  }
}

TEST_CASE("Workbook get_sheet by index") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};
  wb.add_sheet("First");
  wb.add_sheet("Second");

  SECTION("valid index") {
    auto res = wb.get_sheet(0);
    REQUIRE(res.has_value());
    REQUIRE((*res)->sheet_name() == "First");
  }

  SECTION("second sheet") {
    auto res = wb.get_sheet(1);
    REQUIRE(res.has_value());
    REQUIRE((*res)->sheet_name() == "Second");
  }

  SECTION("out of range") {
    auto res = wb.get_sheet(5);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == XlsxError::SheetNotFound);
  }
}

TEST_CASE("Workbook sheet_count") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};

  REQUIRE(wb.sheet_count() == 0);
  wb.add_sheet("A");
  REQUIRE(wb.sheet_count() == 1);
  wb.add_sheet("B");
  REQUIRE(wb.sheet_count() == 2);
}

TEST_CASE("Workbook flush_all_sheets") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};

  auto* ws1 = *wb.add_sheet("Sheet1");
  auto* ws2 = *wb.add_sheet("Sheet2");

  ws1->write(0, 0, 10);
  ws2->write(0, 0, 20);

  auto res = wb.flush_all_sheets();
  REQUIRE(res.has_value());
}

TEST_CASE("Workbook save") {
  MockBackend backend;
  Workbook<MockBackend> wb{std::move(backend)};
  wb.add_sheet("Test");

  auto res = wb.save();
  REQUIRE(res.has_value());
}
