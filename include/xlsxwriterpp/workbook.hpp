#pragma once

#include "worksheet.hpp"
#include "xlsx_backend_concept.hpp"
#include "xlsx_error.hpp"

#include <expected>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xlsxwriterpp {

/**
 * @brief 複数シートを管理するワークブッククラス。
 *
 * Worksheet のコレクションを保持し、シートの追加と保存を担う。
 *
 * ## 使用例
 * ```cpp
 * auto wb = SmartWorkbook{MockBackend{}};
 * auto& ws = *wb.add_sheet("Sheet1");
 * ws.write(0, 0, 42);
 * ws.flush_all();
 * wb.save();
 * ```
 *
 * ## スレッド安全性
 * - add_sheet() はシングルスレッドから呼ぶこと
 * - 各 Worksheet への並列アクセスはシート間で安全
 *
 * @tparam Backend XlsxBackend コンセプトを満たすバックエンド型
 */
template <XlsxBackend Backend>
class Workbook {
public:
  /**
   * @brief コンストラクタ。
   *
   * ワークブックレベルのバックエンドを受け取る。
   * シート追加時は root_backend_ をコピーして per-sheet バックエンドを作成する。
   *
   * @param backend ワークブックバックエンドインスタンス
   */
  explicit Workbook(Backend backend)
    : root_backend_(std::move(backend)) {
  }

  // コピー禁止
  Workbook(Workbook const&)                    = delete;
  auto operator=(Workbook const&) -> Workbook& = delete;

  // ムーブ許可
  Workbook(Workbook&&)                    = default;
  auto operator=(Workbook&&) -> Workbook& = default;

  /**
   * @brief デストラクタ。
   *
   * 各 Worksheet のデストラクタが自動フラッシュを行う。
   * ワークブック保存（save()）はデストラクタでは行わないため、
   * 明示的に save() を呼ぶこと。
   */
  ~Workbook() = default;

  /**
   * @brief 新しいシートをワークブックに追加する。
   *
   * 内部では root_backend_ をコピーし、add_sheet(name) を呼んで
   * シート固有のバックエンドを生成する。
   *
   * 戻り値の Worksheet<Backend>* はこのワークブックが所有し、
   * ワークブックが生存している間は有効なポインタである。
   *
   * @note C++26 P2505 で std::expected<T&, E> が標準化されたが、
   *       現行コンパイラ実装が未対応のため、ポインタ型で返す。
   *
   * @param name シート名（重複不可）
   * @return 成功時 Worksheet<Backend>* 、失敗時 XlsxError
   *         - DuplicateSheetName: 同名シートがすでに存在する
   *         - BackendWriteError: バックエンドのシート追加が失敗した
   */
  [[nodiscard]] auto add_sheet(std::string_view name)
    -> std::expected<Worksheet<Backend>*, XlsxError> {
    // 重複チェック
    for (auto const& [sheet_name, _ws] : sheets_) {
      if (sheet_name == name) {
        return std::unexpected(XlsxError::DuplicateSheetName);
      }
    }

    // root_backend_ をコピーしてシート固有バックエンドを作成
    auto sheet_backend = root_backend_;
    if (auto res = sheet_backend.add_sheet(name); !res) {
      return std::unexpected(res.error());
    }

    // Worksheet を生成して格納
    auto  sheet = std::make_unique<Worksheet<Backend>>(
      std::move(sheet_backend), std::string(name));
    auto* ptr = sheet.get();
    sheets_.emplace_back(std::string(name), std::move(sheet));
    return ptr;
  }

  /**
   * @brief シート名で Worksheet ポインタを取得する。
   *
   * @param name 取得するシート名
   * @return 成功時 Worksheet<Backend>* 、SheetNotFound 時はエラー
   */
  [[nodiscard]] auto get_sheet(std::string_view name)
    -> std::expected<Worksheet<Backend>*, XlsxError> {
    for (auto& [sheet_name, ws] : sheets_) {
      if (sheet_name == name) {
        return ws.get();
      }
    }
    return std::unexpected(XlsxError::SheetNotFound);
  }

  /**
   * @brief インデックスで Worksheet ポインタを取得する。
   *
   * @param index シートインデックス（0始まり）
   * @return 成功時 Worksheet<Backend>* 、SheetNotFound 時はエラー
   */
  [[nodiscard]] auto get_sheet(std::size_t index)
    -> std::expected<Worksheet<Backend>*, XlsxError> {
    if (index >= sheets_.size()) {
      return std::unexpected(XlsxError::SheetNotFound);
    }
    return sheets_[index].second.get();
  }

  /**
   * @brief シート数を返す。
   * @return 登録済みシート数
   */
  [[nodiscard]] auto sheet_count() const noexcept -> std::size_t {
    return sheets_.size();
  }

  /**
   * @brief 全シートの pending セルを昇順にバックエンドへ書き込む。
   *
   * 各シートの flush_all() を順に呼び出す。
   *
   * @return 成功時 void、BackendWriteError 時はエラー
   */
  [[nodiscard]] auto flush_all_sheets() -> std::expected<void, XlsxError> {
    for (auto& [_name, ws] : sheets_) {
      if (auto res = ws->flush_all(); !res) {
        return res;
      }
    }
    return {};
  }

  /**
   * @brief ワークブックをバックエンドへ保存する。
   *
   * save() 前に各シートの flush_all() を呼ぶか、flush_all_sheets() を使うこと。
   *
   * @return 成功時 void、BackendSaveError 時はエラー
   */
  [[nodiscard]] auto save() -> std::expected<void, XlsxError> {
    return root_backend_.save();
  }

private:
  /// ワークブックレベルのバックエンド（save/add_sheet 用）
  Backend root_backend_;

  /// シートのリスト（挿入順を保持）: (シート名, Worksheet のユニークポインタ)
  std::vector<std::pair<std::string, std::unique_ptr<Worksheet<Backend>>>> sheets_;
};

} // namespace xlsxwriterpp
