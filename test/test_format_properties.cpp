#include "catch2/catch_all.hpp"
#include <xlsxwriterpp/format_properties.hpp>

using namespace xlsxwriterpp;
using Catch::Approx;

TEST_CASE("FormatProperties default values") {
  FormatProperties fmt;
  REQUIRE(fmt.font_name.empty());
  REQUIRE(fmt.font_size == 0.0);
  REQUIRE(fmt.bold == false);
  REQUIRE(fmt.italic == false);
  REQUIRE_FALSE(fmt.font_color.has_value());
  REQUIRE_FALSE(fmt.bg_color.has_value());
  REQUIRE(fmt.h_align == HorizontalAlign::Default);
  REQUIRE(fmt.v_align == VerticalAlign::Default);
}

TEST_CASE("FormatProperties merge overwrites non-default fields") {
  FormatProperties base;
  FormatProperties overlay;

  SECTION("font_name") {
    overlay.font_name = "Arial";
    base.merge(overlay);
    REQUIRE(base.font_name == "Arial");
  }

  SECTION("font_size") {
    overlay.font_size = 12.0;
    base.merge(overlay);
    REQUIRE(base.font_size == Approx(12.0));
  }

  SECTION("bold") {
    overlay.bold = true;
    base.merge(overlay);
    REQUIRE(base.bold == true);
  }

  SECTION("italic") {
    overlay.italic = true;
    base.merge(overlay);
    REQUIRE(base.italic == true);
  }

  SECTION("font_color") {
    overlay.font_color = 0xFF0000;
    base.merge(overlay);
    REQUIRE(base.font_color.has_value());
    REQUIRE(*base.font_color == 0xFF0000);
  }

  SECTION("bg_color") {
    overlay.bg_color = 0x00FF00;
    base.merge(overlay);
    REQUIRE(base.bg_color.has_value());
    REQUIRE(*base.bg_color == 0x00FF00);
  }

  SECTION("h_align") {
    overlay.h_align = HorizontalAlign::Center;
    base.merge(overlay);
    REQUIRE(base.h_align == HorizontalAlign::Center);
  }

  SECTION("v_align") {
    overlay.v_align = VerticalAlign::Top;
    base.merge(overlay);
    REQUIRE(base.v_align == VerticalAlign::Top);
  }
}

TEST_CASE("FormatProperties merge does not overwrite default values") {
  FormatProperties base;
  base.font_name = "Helvetica";
  base.font_size = 14.0;
  base.bold = true;
  base.font_color = 0x0000FF;

  FormatProperties overlay; // all defaults

  base.merge(overlay);

  REQUIRE(base.font_name == "Helvetica");
  REQUIRE(base.font_size == Approx(14.0));
  REQUIRE(base.bold == true);
  REQUIRE(base.font_color.has_value());
  REQUIRE(*base.font_color == 0x0000FF);
}

TEST_CASE("FormatProperties merge overwrites existing non-default values") {
  FormatProperties base;
  base.font_name = "Helvetica";
  base.font_size = 14.0;
  base.bold = false;

  FormatProperties overlay;
  overlay.font_name = "Arial";
  overlay.font_size = 12.0;
  overlay.bold = true;

  base.merge(overlay);

  REQUIRE(base.font_name == "Arial");
  REQUIRE(base.font_size == Approx(12.0));
  REQUIRE(base.bold == true);
}
