#pragma once

#include "cell_value.hpp"
#include "format_properties.hpp"
#include "xlsx_error.hpp"

#include <expected>
#include <string_view>

namespace xlsxwriterpp {

/**
 * @brief バックエンドが満たすべき Concept。
 *
 * 静的ポリモーフィズム（コンパイル時ディスパッチ）を実現するため、
 * virtual 関数の代わりにこの Concept を使用する。
 * 各メソッドの意味は以下の通り:
 *
 * - write_cell: 指定セルに値と書式を書き込む（バックエンドが「現在のシート」を管理）
 * - add_sheet:  新しいシートを追加し、以降の write_cell はそのシートに書き込む
 * - save:       ワークブックをファイルへ保存し閉じる
 *
 * @tparam T バックエンド型
 */
template <typename T>
concept XlsxBackend = requires(
  T            backend,
  int          row,
  int          col,
  CellValue    val,
  FormatProperties fmt,
  std::string_view name) {
  { backend.write_cell(row, col, val, fmt) } -> std::same_as<std::expected<void, XlsxError>>;
  { backend.add_sheet(name) }                -> std::same_as<std::expected<void, XlsxError>>;
  { backend.save() }                         -> std::same_as<std::expected<void, XlsxError>>;
};

} // namespace xlsxwriterpp
