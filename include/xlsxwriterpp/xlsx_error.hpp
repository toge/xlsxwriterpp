#pragma once

#include <cstdint>

namespace xlsxwriterpp {

/**
 * @brief ライブラリが返すエラーコードの列挙型。
 *
 * 全公開 API の戻り値 std::expected<T, XlsxError> で使用する。
 * 各エラーの意味はメンバの Doxygen コメントを参照。
 */
enum class XlsxError : uint8_t {
  RowOrderViolation,  ///< flush_row に昇順違反の行番号が渡された
  BackendWriteError,  ///< バックエンドの write_cell が失敗
  BackendSaveError,   ///< バックエンドの save が失敗
  SheetNotFound,      ///< 存在しないシートへのアクセス
  DuplicateSheetName, ///< 同名シートの重複追加
};

} // namespace xlsxwriterpp
