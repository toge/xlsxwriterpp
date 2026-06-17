/**
 * @file consumer_main.cpp
 * @brief find_package(xlsxwriterpp) 経由での利用サンプル。
 *
 * xlsxwriterpp::xlsxwriterpp ターゲットをリンクするだけで
 * 全依存が解決されることを示す最小サンプル。
 */

#include <xlsxwriterpp/mock_backend.hpp>
#include <xlsxwriterpp/workbook.hpp>
#include <print>

auto main() -> int {
  auto wb  = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};
  auto& ws = *wb.add_sheet("Sample");

  std::ignore = ws.write(0, 0, std::string_view{"Hello"});
  std::ignore = ws.write(0, 1, 42);
  std::ignore = ws.flush_all();
  std::ignore = wb.save();

  std::println("consumer_app: 正常終了");
  return 0;
}
