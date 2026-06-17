#pragma once

#include "cell_value.hpp"
#include "format_properties.hpp"
#include "xlsx_error.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xlsxwriterpp {

/**
 * @brief ワークブック全体の共有状態（スレッドセーフ）。
 *
 * 複数の MockBackend インスタンス間で shared_ptr を通じて共有し、
 * ワークブックレベルの状態（シート一覧等）を管理する。
 */
struct MockWorkbookState {
  std::vector<std::string> sheet_names; ///< 追加済みシート名のリスト
  mutable std::mutex       mtx;         ///< 共有状態の排他制御用 Mutex
};

/**
 * @brief デバッグ出力付きモックバックエンド。
 *
 * 実際の Excel ファイルを生成せず、全操作を std::print で標準出力に表示する。
 * 書式マージの正確性や write/flush の動作を視覚的に検証するために使用する。
 *
 * ## 設計
 * - 複数インスタンスで shared_ptr<MockWorkbookState> を共有し、ワークブック状態を同期
 * - current_sheet_name_ は per-instance 状態（シート固有）
 *
 * ## 使用例
 * ```cpp
 * auto wb  = SmartWorkbook{MockBackend{}};
 * auto& ws = *wb.add_sheet("Sheet1");
 * ws.write(0, 0, 42);
 * ws.flush_all();
 * wb.save();
 * ```
 */
class MockBackend {
public:
  /**
   * @brief デフォルトコンストラクタ（ワークブックルートとして使用）。
   *
   * 新しい MockWorkbookState を作成する。
   */
  MockBackend()
    : state_(std::make_shared<MockWorkbookState>()) {
    std::println("[MockBackend] ワークブック作成");
  }

  /**
   * @brief シートに値と書式を書き込む。
   *
   * FormatProperties の内容をパースして std::print で出力する。
   *
   * @param row 行インデックス
   * @param col 列インデックス
   * @param val セル値
   * @param fmt 書式プロパティ
   * @return 常に成功
   */
  auto write_cell(int row, int col, CellValue val, FormatProperties const& fmt)
    -> std::expected<void, XlsxError> {
    auto const val_str  = format_value(val);
    auto const fmt_str  = format_props(fmt);
    auto _ = std::lock_guard{state_->mtx};
    std::println(
      "[MockBackend][{}] write_cell({:3},{:3}) val={:<20} fmt={}",
      current_sheet_name_, row, col, val_str, fmt_str);
    return {};
  }

  auto write_cell_stored(int row, int col, detail::StoredCellValue const& val, FormatProperties const& fmt)
    -> std::expected<void, XlsxError> {
    return write_cell(row, col, detail::to_cell_value(val), fmt);
  }

  /**
   * @brief シートを追加し、以降の write_cell をそのシートへ向ける。
   *
   * @param name シート名
   * @return 常に成功
   */
  auto add_sheet(std::string_view name) -> std::expected<void, XlsxError> {
    current_sheet_name_ = std::string(name);
    {
      auto _ = std::lock_guard{state_->mtx};
      state_->sheet_names.emplace_back(name);
    }
    std::println("[MockBackend] シート追加: '{}'", name);
    return {};
  }

  /**
   * @brief ワークブックを保存（モックなので内容を出力のみ）。
   *
   * @return 常に成功
   */
  auto save() -> std::expected<void, XlsxError> {
    auto _ = std::lock_guard{state_->mtx};
    std::println("[MockBackend] save() — シート数: {}", state_->sheet_names.size());
    for (auto const& name : state_->sheet_names) {
      std::println("  - '{}'", name);
    }
    return {};
  }

private:
  std::shared_ptr<MockWorkbookState> state_;              ///< ワークブック共有状態
  std::string                        current_sheet_name_; ///< このインスタンスのシート名

  /**
   * @brief CellValue を表示用文字列に変換する。
   *
   * @param val 変換対象の CellValue
   * @return 表示用文字列
   */
  [[nodiscard]] static auto format_value(CellValue const& val) -> std::string {
    return std::visit(
      []<typename V>(V const& v) -> std::string {
        if constexpr (std::is_same_v<V, std::monostate>) {
          return "(blank)";
        } else if constexpr (std::is_same_v<V, int>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<V, double>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<V, std::string_view>) {
          return std::string{'"'} + std::string(v) + '"';
        } else if constexpr (std::is_same_v<V, bool>) {
          return v ? "true" : "false";
        }
      },
      val);
  }

  /**
   * @brief FormatProperties を表示用文字列に変換する。
   *
   * 設定済みのプロパティのみを出力して視認性を向上させる。
   *
   * @param fmt 変換対象の FormatProperties
   * @return 表示用文字列
   */
  [[nodiscard]] static auto format_props(FormatProperties const& fmt) -> std::string {
    auto out = std::string{};
    if (!fmt.font_name.empty()) {
      out += "font=" + fmt.font_name + " ";
    }
    if (fmt.font_size != 0.0) {
      out += "size=" + std::to_string(fmt.font_size) + " ";
    }
    if (fmt.bold) {
      out += "bold ";
    }
    if (fmt.italic) {
      out += "italic ";
    }
    if (fmt.font_color.has_value()) {
      char buf[16];
      std::snprintf(buf, sizeof(buf), "fc=#%06X ", *fmt.font_color);
      out += buf;
    }
    if (fmt.bg_color.has_value()) {
      char buf[16];
      std::snprintf(buf, sizeof(buf), "bg=#%06X ", *fmt.bg_color);
      out += buf;
    }
    if (fmt.h_align != HorizontalAlign::Default) {
      out += "h=" + halign_str(fmt.h_align) + " ";
    }
    if (fmt.v_align != VerticalAlign::Default) {
      out += "v=" + valign_str(fmt.v_align) + " ";
    }
    return out.empty() ? "(no format)" : out;
  }

  /// @brief HorizontalAlign を文字列に変換する。
  [[nodiscard]] static auto halign_str(HorizontalAlign a) -> std::string {
    switch (a) {
    case HorizontalAlign::Left:    return "Left";
    case HorizontalAlign::Center:  return "Center";
    case HorizontalAlign::Right:   return "Right";
    case HorizontalAlign::Justify: return "Justify";
    default:                       return "Default";
    }
  }

  /// @brief VerticalAlign を文字列に変換する。
  [[nodiscard]] static auto valign_str(VerticalAlign a) -> std::string {
    switch (a) {
    case VerticalAlign::Top:      return "Top";
    case VerticalAlign::Center:   return "Center";
    case VerticalAlign::Bottom:   return "Bottom";
    case VerticalAlign::VJustify: return "VJustify";
    default:                      return "Default";
    }
  }
};

} // namespace xlsxwriterpp
