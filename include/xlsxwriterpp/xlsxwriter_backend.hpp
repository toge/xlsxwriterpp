#pragma once

// このヘッダーは libxlsxwriter をリンクするビルドでのみ利用可能。
// CMakeLists.txt で XLSXWRITERPP_ENABLE_XLSXWRITER オプションを有効にすること。

#include "cell_value.hpp"
#include "format_properties.hpp"
#include "xlsx_error.hpp"

#include <cstring>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

extern "C" {
#include <xlsxwriter.h>
}

namespace xlsxwriterpp {

/**
 * @brief ワークブックレベルの共有状態（libxlsxwriter ラッパー）。
 *
 * 複数の XlsxWriterBackend インスタンス間で shared_ptr を通じて共有する。
 * lxw_workbook は close されるまで有効であり、所有権はこの構造体が管理する。
 */
struct XlsxWriterWorkbookState {
  lxw_workbook* wb{nullptr}; ///< libxlsxwriter のワークブックポインタ

  /**
   * @brief コンストラクタ。
   * @param filename 出力 Excel ファイルのパス
   */
  explicit XlsxWriterWorkbookState(char const* filename)
    : wb(workbook_new(filename)) {
  }

  // コピー禁止
  XlsxWriterWorkbookState(XlsxWriterWorkbookState const&)                    = delete;
  auto operator=(XlsxWriterWorkbookState const&) -> XlsxWriterWorkbookState& = delete;

  // ムーブ禁止
  XlsxWriterWorkbookState(XlsxWriterWorkbookState&&)                    = delete;
  auto operator=(XlsxWriterWorkbookState&&) -> XlsxWriterWorkbookState& = delete;

  ~XlsxWriterWorkbookState() = default; // workbook_close は save() で呼ぶ
};

/**
 * @brief libxlsxwriter ベースのバックエンド実装。
 *
 * XlsxBackend コンセプトを満たし、実際の Excel ファイルへ書き込む。
 * ワークブック状態は shared_ptr で共有し、シートごとに独立した
 * lxw_worksheet* を保持する。
 *
 * ## スレッド安全性
 * write_cell は並列呼び出し時に状態共有部（workbook_add_format）で
 * データ競合が起きるため、内部 mutex で直列化している。
 *
 * ## 使用例
 * ```cpp
 * auto wb  = SmartWorkbook{XlsxWriterBackend{"output.xlsx"}};
 * auto& ws = *wb.add_sheet("Sheet1");
 * ws.write(0, 0, 42);
 * ws.flush_all();
 * wb.save();
 * ```
 */
class XlsxWriterBackend {
public:
  /**
   * @brief コンストラクタ（ワークブックルートとして使用）。
   *
   * @param filename 出力 Excel ファイルのパス
   */
  explicit XlsxWriterBackend(std::string_view filename)
    : state_(std::make_shared<XlsxWriterWorkbookState>(std::string(filename).c_str())) {
  }

  /**
   * @brief 指定セルに値と書式を書き込む。
   *
   * CellValue の型に応じて適切な worksheet_write_* 関数を呼び出す。
   * std::monostate（値なし）の場合は worksheet_write_blank を使用する。
   *
   * @param row 行インデックス
   * @param col 列インデックス
   * @param val セル値
   * @param fmt 書式プロパティ
   * @return 成功時 void、失敗時 BackendWriteError
   */
  auto write_cell(int row, int col, CellValue const& val, FormatProperties const& fmt)
    -> std::expected<void, XlsxError> {
    if (!worksheet_) {
      return std::unexpected(XlsxError::BackendWriteError);
    }

    auto _  = std::lock_guard{state_mutex_};
    auto* f = fmt.has_any() ? apply_format(fmt) : nullptr;

    auto const lrow = static_cast<lxw_row_t>(row);
    auto const lcol = static_cast<lxw_col_t>(col);

    // CellValue の型でディスパッチして書き込み
    auto const err = std::visit(
      [&]<typename V>(V const& v) -> lxw_error {
        if constexpr (std::is_same_v<V, std::monostate>) {
          return worksheet_write_blank(worksheet_, lrow, lcol, f);
        } else if constexpr (std::is_same_v<V, int>) {
          return worksheet_write_number(worksheet_, lrow, lcol, static_cast<double>(v), f);
        } else if constexpr (std::is_same_v<V, double>) {
          return worksheet_write_number(worksheet_, lrow, lcol, v, f);
        } else if constexpr (std::is_same_v<V, std::string_view>) {
          return worksheet_write_string(worksheet_, lrow, lcol, std::string(v).c_str(), f);
        } else if constexpr (std::is_same_v<V, bool>) {
          return worksheet_write_boolean(worksheet_, lrow, lcol, v ? 1 : 0, f);
        }
      },
      val);

    return err == LXW_NO_ERROR
              ? std::expected<void, XlsxError>{}
              : std::unexpected(XlsxError::BackendWriteError);
  }

  /**
   * @brief 保存済みセル値に値と書式を書き込む（コピー削減版）。
   *
   * std::string 等の型を直接扱うことで、CellValue 経由の再コピーを避ける。
   *
   * @param row 行インデックス
   * @param col 列インデックス
   * @param val 保存済みセル値
   * @param fmt 書式プロパティ
   * @return 成功時 void、失敗時 BackendWriteError
   */
  auto write_cell_stored(int row, int col, detail::StoredCellValue const& val, FormatProperties const& fmt)
    -> std::expected<void, XlsxError> {
    if (!worksheet_) {
      return std::unexpected(XlsxError::BackendWriteError);
    }

    auto _  = std::lock_guard{state_mutex_};
    auto* f = fmt.has_any() ? apply_format(fmt) : nullptr;

    auto const lrow = static_cast<lxw_row_t>(row);
    auto const lcol = static_cast<lxw_col_t>(col);

    auto const err = std::visit(
      [&]<typename V>(V const& v) -> lxw_error {
        if constexpr (std::is_same_v<V, std::monostate>) {
          return worksheet_write_blank(worksheet_, lrow, lcol, f);
        } else if constexpr (std::is_same_v<V, int>) {
          return worksheet_write_number(worksheet_, lrow, lcol, static_cast<double>(v), f);
        } else if constexpr (std::is_same_v<V, double>) {
          return worksheet_write_number(worksheet_, lrow, lcol, v, f);
        } else if constexpr (std::is_same_v<V, std::string>) {
          return worksheet_write_string(worksheet_, lrow, lcol, v.c_str(), f);
        } else if constexpr (std::is_same_v<V, bool>) {
          return worksheet_write_boolean(worksheet_, lrow, lcol, v ? 1 : 0, f);
        }
      },
      val);

    return err == LXW_NO_ERROR
              ? std::expected<void, XlsxError>{}
              : std::unexpected(XlsxError::BackendWriteError);
  }

  /**
   * @brief 新しいシートを追加する。
   *
   * workbook_add_worksheet を呼び、以降の write_cell がそのシートに書き込まれる。
   *
   * @param name シート名
   * @return 成功時 void、失敗時 BackendWriteError
   */
  auto add_sheet(std::string_view name) -> std::expected<void, XlsxError> {
    auto sheet_name = std::string(name);
    worksheet_      = workbook_add_worksheet(state_->wb, sheet_name.c_str());
    return worksheet_ ? std::expected<void, XlsxError>{}
                      : std::unexpected(XlsxError::BackendWriteError);
  }

  /**
   * @brief ワークブックをファイルに保存して閉じる。
   *
   * workbook_close を呼び出す。一度 save() を呼ぶと以降の write_cell は無効になる。
   *
   * @return 成功時 void、失敗時 BackendSaveError
   */
  auto save() -> std::expected<void, XlsxError> {
    auto const err = workbook_close(state_->wb);
    return err == LXW_NO_ERROR ? std::expected<void, XlsxError>{}
                               : std::unexpected(XlsxError::BackendSaveError);
  }

private:
  std::shared_ptr<XlsxWriterWorkbookState> state_;       ///< ワークブック共有状態
  lxw_worksheet*                           worksheet_{nullptr}; ///< このシートのポインタ
  mutable std::mutex                       state_mutex_; ///< workbook 操作の排他制御

  /**
   * @brief FormatProperties を lxw_format* に変換して適用する。
   *
   * workbook_add_format で新規フォーマットを生成し、FormatProperties の内容を設定する。
   * フォーマットオブジェクトは workbook_close 時に自動解放される。
   *
   * @param fmt 適用する書式プロパティ
   * @return 設定済みの lxw_format*（設定がない場合は nullptr）
   */
  [[nodiscard]] auto apply_format(FormatProperties const& fmt) const -> lxw_format* {
    if (!fmt.has_any()) {
      return nullptr;
    }

    auto* f = workbook_add_format(state_->wb);

    if (!fmt.font_name.empty()) {
      format_set_font_name(f, fmt.font_name.c_str());
    }
    if (fmt.font_size != 0.0) {
      format_set_font_size(f, fmt.font_size);
    }
    if (fmt.bold) {
      format_set_bold(f);
    }
    if (fmt.italic) {
      format_set_italic(f);
    }
    if (fmt.font_color.has_value()) {
      format_set_font_color(f, static_cast<lxw_color_t>(*fmt.font_color));
    }
    if (fmt.bg_color.has_value()) {
      // 背景色にはパターン設定が必要
      format_set_pattern(f, LXW_PATTERN_SOLID);
      format_set_bg_color(f, static_cast<lxw_color_t>(*fmt.bg_color));
    }

    // 水平配置
    switch (fmt.h_align) {
    case HorizontalAlign::Left:    format_set_align(f, LXW_ALIGN_LEFT);    break;
    case HorizontalAlign::Center:  format_set_align(f, LXW_ALIGN_CENTER);  break;
    case HorizontalAlign::Right:   format_set_align(f, LXW_ALIGN_RIGHT);   break;
    case HorizontalAlign::Justify: format_set_align(f, LXW_ALIGN_JUSTIFY); break;
    default: break;
    }

    // 垂直配置
    switch (fmt.v_align) {
    case VerticalAlign::Top:      format_set_align(f, LXW_ALIGN_VERTICAL_TOP);      break;
    case VerticalAlign::Center:   format_set_align(f, LXW_ALIGN_VERTICAL_CENTER);   break;
    case VerticalAlign::Bottom:   format_set_align(f, LXW_ALIGN_VERTICAL_BOTTOM);   break;
    case VerticalAlign::VJustify: format_set_align(f, LXW_ALIGN_VERTICAL_JUSTIFY);  break;
    default: break;
    }

    return f;
  }
};

} // namespace xlsxwriterpp
