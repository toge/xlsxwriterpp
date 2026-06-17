#pragma once

#include <string>
#include <string_view>
#include <variant>

namespace xlsxwriterpp {

/**
 * @brief セル値の公開型エイリアス。
 *
 * string_view を使用するため、参照元文字列の寿命はライブラリ側で
 * コピーして管理する（内部では StoredCellValue を使用）。
 * std::monostate は「書式のみ、値なし」を意味する。
 */
using CellValue = std::variant<std::monostate, int, double, std::string_view, bool>;

namespace detail {

/**
 * @brief セル値の内部保存型。
 *
 * string_view の代わりに std::string を使用して文字列の所有権を持つ。
 * 外部公開はせず、ライブラリ内部でのみ使用する。
 */
using StoredCellValue = std::variant<std::monostate, int, double, std::string, bool>;

/**
 * @brief CellValue を StoredCellValue に変換する。
 *
 * string_view は std::string にコピーし、その他の型はそのまま格納する。
 *
 * @param val 変換元の CellValue
 * @return 文字列を所有する StoredCellValue
 */
[[nodiscard]] inline auto to_stored(CellValue const& val) noexcept -> StoredCellValue {
  return std::visit(
    []<typename V>(V const& v) -> StoredCellValue {
      if constexpr (std::is_same_v<V, std::string_view>) {
        return std::string{v};
      } else {
        return v;
      }
    },
    val);
}

/**
 * @brief StoredCellValue を CellValue に変換する。
 *
 * std::string は std::string_view として参照する（寿命は呼び出し元が管理）。
 *
 * @param val 変換元の StoredCellValue
 * @return バックエンド呼び出し用の CellValue
 */
[[nodiscard]] inline auto to_cell_value(StoredCellValue const& val) noexcept -> CellValue {
  return std::visit(
    []<typename V>(V const& v) -> CellValue {
      if constexpr (std::is_same_v<V, std::string>) {
        return std::string_view{v};
      } else {
        return v;
      }
    },
    val);
}

} // namespace detail
} // namespace xlsxwriterpp
