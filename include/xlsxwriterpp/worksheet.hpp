#pragma once

#include "cell_value.hpp"
#include "format_properties.hpp"
#include "xlsx_backend_concept.hpp"
#include "xlsx_error.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <expected>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace xlsxwriterpp {

namespace detail {

/**
 * @brief pending セルの内部表現。
 *
 * 行、列、値、および書式を保持する。
 * 文字列は StoredCellValue 内の std::string で所有権を管理する。
 */
struct CellRecord {
  int             row;
  int             col;
  StoredCellValue value;
  FormatProperties format;
};

} // namespace detail

/**
 * @brief スパース遅延バッファ方式のワークシートラッパー。
 *
 * セルへの値書き込みと書式設定を別タイミングで行い、
 * flush_row() 呼び出し時にのみバックエンドへ転送する。
 * ストライプロック方式で異なる行への並列書き込みをサポートする。
 *
 * ## ライフサイクル契約
 * - flush_row(row) は昇順でのみ受け付ける（RowOrderViolation）
 * - デストラクタは未フラッシュ行を自動フラッシュする（失敗時は std::terminate）
 * - デストラクタ前に flush_all() を呼ぶことで std::terminate を回避できる
 *
 * ## スレッド安全性
 * - 異なる行への並列 write/format/flush_row: 安全
 * - 同一行への並列アクセス: ストライプロックで直列化
 * - flush_all(): 全ストライプロックを昇順取得して実行
 *
 * @tparam Backend XlsxBackend コンセプトを満たすバックエンド型
 */
template <XlsxBackend Backend>
class Worksheet {
public:
  /// ストライプロック数（2の累乗推奨、異なる行の並列性を制御）
  static constexpr std::size_t STRIPE_COUNT = 64;

  /**
   * @brief コンストラクタ。
   *
   * @param backend このシートに対応するバックエンドインスタンス
   * @param sheet_name シート名
   */
  explicit Worksheet(Backend backend, std::string sheet_name)
    : backend_(std::move(backend))
    , sheet_name_(std::move(sheet_name)) {
  }

  // コピー禁止（mutex はコピー不可）
  Worksheet(Worksheet const&)                    = delete;
  auto operator=(Worksheet const&) -> Worksheet& = delete;

  // ムーブ禁止（mutex はムーブ不可）
  Worksheet(Worksheet&&)                    = delete;
  auto operator=(Worksheet&&) -> Worksheet& = delete;

  /**
   * @brief デストラクタ。未フラッシュの行を昇順で自動フラッシュする。
   *
   * フラッシュに失敗した場合は std::terminate を呼び出す。
   * デストラクタ前に明示的に flush_all() を呼ぶことを強く推奨する。
   */
  ~Worksheet() noexcept {
    if (auto res = flush_all_impl(); !res) {
      std::terminate();
    }
  }

  /**
   * @brief セルに値のみを書き込む（書式は後から format() で設定可能）。
   *
   * @param row 行インデックス（0始まり）
   * @param col 列インデックス（0始まり）
   * @param val セル値
   * @return 成功時 void、失敗時 XlsxError
   */
  [[nodiscard]] auto write(int row, int col, CellValue val)
    -> std::expected<void, XlsxError> {
    auto const stripe = stripe_index(row);
    auto _            = std::lock_guard{stripe_mutexes_[stripe]};
    auto& cell        = find_or_create_cell(row, col);
    cell.value        = detail::to_stored(val);
    return {};
  }

  /**
   * @brief セルに値と書式を同時に書き込む。
   *
   * すでに書式が設定されている場合は fmt がマージされる。
   *
   * @param row 行インデックス（0始まり）
   * @param col 列インデックス（0始まり）
   * @param val セル値
   * @param fmt 書式プロパティ（既存書式にマージ）
   * @return 成功時 void、失敗時 XlsxError
   */
  [[nodiscard]] auto write(int row, int col, CellValue val, FormatProperties fmt)
    -> std::expected<void, XlsxError> {
    auto const stripe = stripe_index(row);
    auto _            = std::lock_guard{stripe_mutexes_[stripe]};
    auto& cell        = find_or_create_cell(row, col);
    cell.value        = detail::to_stored(val);
    cell.format.merge(fmt);
    return {};
  }

  /**
   * @brief セルに書式のみを設定する（値は pending または未設定のまま）。
   *
   * @param row 行インデックス（0始まり）
   * @param col 列インデックス（0始まり）
   * @param fmt 書式プロパティ（既存書式にマージ）
   * @return 成功時 void、失敗時 XlsxError
   */
  [[nodiscard]] auto format(int row, int col, FormatProperties fmt)
    -> std::expected<void, XlsxError> {
    auto const stripe = stripe_index(row);
    auto _            = std::lock_guard{stripe_mutexes_[stripe]};
    auto& cell        = find_or_create_cell(row, col);
    cell.format.merge(fmt);
    return {};
  }

  /**
   * @brief 指定行の全 pending セルをバックエンドへ書き込む。
   *
   * 書き込み後、該当行の pending エントリを erase してメモリを解放する。
   *
   * @param row フラッシュする行インデックス
   * @return 成功時 void、RowOrderViolation / BackendWriteError 時はエラー
   */
  [[nodiscard]] auto flush_row(int row) -> std::expected<void, XlsxError> {
    // 行順序チェック（最後にフラッシュした行より前の行は拒否）
    if (auto const last = last_flushed_row_.load(std::memory_order_acquire); last > row) {
      return std::unexpected(XlsxError::RowOrderViolation);
    }

    auto const stripe = stripe_index(row);
    auto _            = std::lock_guard{stripe_mutexes_[stripe]};

    if (auto res = flush_row_locked(row); !res) {
      return res;
    }

    // last_flushed_row_ を max(current, row) に更新（CAS ループ）
    update_last_flushed(row);
    return {};
  }

  /**
   * @brief 全 pending セルを昇順にバックエンドへ書き込む。
   *
   * デストラクタ前に呼ぶことで std::terminate を回避できる。
   * flush_all は RowOrderViolation チェックをスキップして全行を書き込む。
   *
   * @return 成功時 void、BackendWriteError 時はエラー
   */
  [[nodiscard]] auto flush_all() -> std::expected<void, XlsxError> {
    return flush_all_impl();
  }

  /**
   * @brief シート名を返す。
   * @return シート名（string_view）
   */
  [[nodiscard]] auto sheet_name() const noexcept -> std::string_view {
    return sheet_name_;
  }

private:
  Backend     backend_;    ///< このシートのバックエンドインスタンス
  std::string sheet_name_; ///< シート名

  /// pending セルのベクトル（(row, col) 昇順ソート済み）
  std::vector<detail::CellRecord> pending_;

  /// ストライプロック配列（行インデックス % STRIPE_COUNT でインデックス決定）
  mutable std::array<std::mutex, STRIPE_COUNT> stripe_mutexes_;

  /// 最後にフラッシュされた行番号（-1 = まだ未フラッシュ）
  std::atomic<int> last_flushed_row_{-1};

  /**
   * @brief 行番号からストライプインデックスを計算する。
   *
   * @param row 行インデックス（非負整数を想定）
   * @return ストライプインデックス [0, STRIPE_COUNT)
   */
  [[nodiscard]] static constexpr auto stripe_index(int row) noexcept -> std::size_t {
    return static_cast<std::size_t>(row) % STRIPE_COUNT;
  }

  /**
   * @brief 指定セルの CellRecord を取得または生成する（ストライプロック取得済みが前提）。
   *
   * @param row 行インデックス
   * @param col 列インデックス
   * @return CellRecord への参照
   */
  auto find_or_create_cell(int row, int col) -> detail::CellRecord& {
    // Append-at-end fast path（連続書き込みで O(1)）
    if (pending_.empty()
        || std::tie(row, col) > std::tie(pending_.back().row, pending_.back().col)) {
      return pending_.emplace_back(row, col, std::monostate{}, FormatProperties{});
    }

    auto it = std::ranges::lower_bound(
      pending_,
      detail::CellRecord{row, col, std::monostate{}, FormatProperties{}},
      [](detail::CellRecord const& a, detail::CellRecord const& b) {
        return std::tie(a.row, a.col) < std::tie(b.row, b.col);
      });

    if (it != pending_.end() && it->row == row && it->col == col) {
      return *it;
    }

    return *pending_.emplace(it, row, col, std::monostate{}, FormatProperties{});
  }

  /**
   * @brief last_flushed_row_ を max(current, row) に CAS ループで更新する。
   *
   * @param row 新しい候補値
   */
  auto update_last_flushed(int row) noexcept -> void {
    auto old = last_flushed_row_.load(std::memory_order_relaxed);
    while (old < row
           && !last_flushed_row_.compare_exchange_weak(
             old, row, std::memory_order_release, std::memory_order_relaxed)) {
    }
  }

  /**
   * @brief 指定行の pending セルをバックエンドへ書き込む（ロック取得済みが前提）。
   *
   * 書き込み完了後に pending エントリを erase してメモリを解放する。
   *
   * @param row フラッシュする行インデックス
   * @return 成功時 void、BackendWriteError 時はエラー
   */
  auto flush_row_locked(int row) -> std::expected<void, XlsxError> {
    // binary search で row の先頭を特定
    auto it = std::lower_bound(
      pending_.begin(), pending_.end(), row,
      [](detail::CellRecord const& c, int r) { return c.row < r; });

    if (it == pending_.end() || it->row != row) {
      return {}; // この行に pending セルなし
    }

    // 同じ行のセルを連続で flush
    auto first = it;
    while (it != pending_.end() && it->row == row) {
      if (it->value.index() != 0  // monostate 以外
          || it->format.has_any()) {
        if (auto res = backend_.write_cell_stored(it->row, it->col, it->value, it->format); !res) {
          return std::unexpected(XlsxError::BackendWriteError);
        }
      }
      ++it;
    }

    // flush 済み行を erase（メモリ解放 + 次回の binary search 範囲縮小）
    pending_.erase(first, it);
    return {};
  }

  /**
   * @brief 全 pending 行を昇順フラッシュする内部実装。
   *
   * 全ストライプロックを昇順で取得してから実行することで、
   * 他スレッドの write/format 操作を一時停止させた状態で安全にフラッシュする。
   *
   * @return 成功時 void、BackendWriteError 時はエラー
   */
  auto flush_all_impl() -> std::expected<void, XlsxError> {
    // 全ストライプのロックを昇順で取得（デッドロック防止）
    auto locks = std::vector<std::unique_lock<std::mutex>>{};
    locks.reserve(STRIPE_COUNT);
    for (auto const i : std::views::iota(0UZ, STRIPE_COUNT)) {
      locks.emplace_back(stripe_mutexes_[i]);
    }

    // 全セルを逐次 flush
    auto current_row = -1;
    for (auto const& cell : pending_) {
      if (cell.row != current_row) {
        update_last_flushed(cell.row);
        current_row = cell.row;
      }
      if (cell.value.index() != 0 || cell.format.has_any()) {
        if (auto res = backend_.write_cell_stored(cell.row, cell.col, cell.value, cell.format); !res) {
          return std::unexpected(XlsxError::BackendWriteError);
        }
      }
    }

    pending_.clear();
    return {};
  }
};

} // namespace xlsxwriterpp
