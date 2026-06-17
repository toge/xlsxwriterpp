/**
 * @file main.cpp
 * @brief xlsxwriterpp ライブラリの動作確認サンプル。
 *
 * 以下の項目を実証する:
 *   1. 複数シートへの書き込み
 *   2. 行単位の並列 flush_row（std::jthread を使用）
 *   3. 部分的な書式マージ（太字設定後に文字色だけを赤に変更）
 *   4. flush_all() による一括フラッシュ
 *   5. std::expected によるエラーハンドリング（RowOrderViolation の発生と補足）
 */

#include <xlsxwriterpp/cell_value.hpp>
#include <xlsxwriterpp/format_properties.hpp>
#include <xlsxwriterpp/mock_backend.hpp>
#include <xlsxwriterpp/workbook.hpp>
#include <xlsxwriterpp/worksheet.hpp>
#include <xlsxwriterpp/xlsx_error.hpp>

#include <print>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

// ──────────────────────────────────────────────────────────────
// ユーティリティ：エラーを文字列に変換
// ──────────────────────────────────────────────────────────────
auto error_str(xlsxwriterpp::XlsxError e) -> std::string {
  using E = xlsxwriterpp::XlsxError;
  switch (e) {
  case E::RowOrderViolation:  return "RowOrderViolation";
  case E::BackendWriteError:  return "BackendWriteError";
  case E::BackendSaveError:   return "BackendSaveError";
  case E::SheetNotFound:      return "SheetNotFound";
  case E::DuplicateSheetName: return "DuplicateSheetName";
  }
  return "Unknown";
}

// ──────────────────────────────────────────────────────────────
// デモ 1: 複数シートへの基本書き込みと flush_all
// ──────────────────────────────────────────────────────────────
auto demo_multi_sheet() -> void {
  std::println("\n━━━ Demo 1: 複数シートへの基本書き込み ━━━");

  auto wb = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};

  // add_sheet は std::expected<SmartWorksheet<Backend>*, XlsxError> を返す
  auto* ws1 = wb.add_sheet("売上データ").value();
  auto* ws2 = wb.add_sheet("顧客リスト").value();

  // Sheet1 への書き込み
  std::ignore = ws1->write(0, 0, std::string_view{"商品名"});
  std::ignore = ws1->write(0, 1, std::string_view{"数量"});
  std::ignore = ws1->write(0, 2, std::string_view{"単価"});
  std::ignore = ws1->write(1, 0, std::string_view{"りんご"});
  std::ignore = ws1->write(1, 1, 100);
  std::ignore = ws1->write(1, 2, 150.0);

  // Sheet2 への書き込み
  std::ignore = ws2->write(0, 0, std::string_view{"顧客名"});
  std::ignore = ws2->write(0, 1, std::string_view{"メールアドレス"});
  std::ignore = ws2->write(1, 0, std::string_view{"山田太郎"});
  std::ignore = ws2->write(1, 1, std::string_view{"yamada@example.com"});

  // 全シートを一括フラッシュ
  std::println("\n--- flush_all_sheets ---");
  if (auto res = wb.flush_all_sheets(); !res) {
    std::println("エラー: {}", error_str(res.error()));
  }

  std::println("\n--- save ---");
  std::ignore = wb.save();
}

// ──────────────────────────────────────────────────────────────
// デモ 2: 書式マージ（太字→文字色赤への部分変更）
// ──────────────────────────────────────────────────────────────
auto demo_format_merge() -> void {
  std::println("\n━━━ Demo 2: 書式マージ（太字 → 後から文字色赤を追加） ━━━");

  auto wb  = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};
  auto* ws = wb.add_sheet("書式マージ検証").value();

  using xlsxwriterpp::FormatProperties;
  using xlsxwriterpp::HorizontalAlign;

  // ステップ1: 値を書き込み、太字・中央揃えを設定
  std::ignore = ws->write(0, 0, std::string_view{"重要データ"});
  std::ignore = ws->format(
    0, 0,
    FormatProperties{.bold = true, .h_align = HorizontalAlign::Center});

  std::println("  ▶ ステップ1: 太字 + 中央揃えを設定");

  // ステップ2: 後から文字色だけを赤に変更（既存書式にマージ）
  std::ignore = ws->format(0, 0, FormatProperties{.font_color = 0xFF0000u});

  std::println("  ▶ ステップ2: 文字色赤を追加（太字・中央揃えは維持されるはず）");

  // ステップ3: さらにフォントサイズだけ変更
  std::ignore = ws->format(0, 0, FormatProperties{.font_size = 14.0});

  std::println("  ▶ ステップ3: フォントサイズ14を追加");

  std::println("\n--- flush_row(0) の結果（3回のマージが反映されているか確認） ---");
  std::ignore = ws->flush_row(0);

  // 別行でも検証：背景色と文字色のマージ
  std::ignore = ws->write(1, 0, 42, FormatProperties{.bg_color = 0xD3D3D3u}); // 灰色背景
  std::ignore = ws->format(1, 0, FormatProperties{.italic = true, .font_color = 0x0000FFu}); // 斜体+青文字

  std::println("\n--- flush_row(1) の結果（灰色背景+斜体+青文字） ---");
  std::ignore = ws->flush_row(1);

  std::ignore = wb.save();
}

// ──────────────────────────────────────────────────────────────
// デモ 3: 行単位の並列 flush_row（std::jthread 使用）
// ──────────────────────────────────────────────────────────────
auto demo_parallel_flush() -> void {
  std::println("\n━━━ Demo 3: 行単位の並列 flush_row ━━━");

  auto wb  = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};
  auto* ws = wb.add_sheet("並列フラッシュ").value();

  // 行 0–7 にデータを書き込む
  for (auto const row : std::views::iota(0, 8)) {
    for (auto const col : std::views::iota(0, 3)) {
      std::ignore = ws->write(row, col, row * 10 + col);
    }
  }

  std::println("  ▶ 行0–3 を並列でフラッシュ（各行は独立したストライプロック）");

  // 行 0–3 を並列フラッシュ（各行は異なるストライプロックで競合しない）
  auto threads = std::vector<std::jthread>{};
  threads.reserve(4);
  for (auto const row : std::views::iota(0, 4)) {
    threads.emplace_back([ws, row]() {
      if (auto res = ws->flush_row(row); !res) {
        std::println("  flush_row({}) エラー: {}", row, error_str(res.error()));
      }
    });
  }
  // jthread はスコープ抜けで自動 join
  threads.clear();

  std::println("  ▶ 行4–7 を flush_all で一括フラッシュ");
  if (auto res = ws->flush_all(); !res) {
    std::println("  flush_all エラー: {}", error_str(res.error()));
  }

  std::ignore = wb.save();
}

// ──────────────────────────────────────────────────────────────
// デモ 4: RowOrderViolation エラーハンドリング
// ──────────────────────────────────────────────────────────────
auto demo_row_order_violation() -> void {
  std::println("\n━━━ Demo 4: RowOrderViolation のエラーハンドリング ━━━");

  auto wb  = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};
  auto* ws = wb.add_sheet("エラー検証").value();

  std::ignore = ws->write(0, 0, std::string_view{"行0"});
  std::ignore = ws->write(1, 0, std::string_view{"行1"});
  std::ignore = ws->write(2, 0, std::string_view{"行2"});

  // 正常なフラッシュ順序（昇順）
  std::println("  ▶ flush_row(0): 正常");
  if (auto res = ws->flush_row(0); !res) {
    std::println("  エラー: {}", error_str(res.error()));
  }

  std::println("  ▶ flush_row(2): 正常（行1をスキップして行2をフラッシュ可能）");
  if (auto res = ws->flush_row(2); !res) {
    std::println("  エラー: {}", error_str(res.error()));
  }

  // 行順序違反（行2の後に行1をフラッシュしようとする）
  std::println("  ▶ flush_row(1): RowOrderViolation を期待（2の後に1は不可）");
  if (auto res = ws->flush_row(1); !res) {
    std::println("  ✓ 期待通りエラーを捕捉: {}", error_str(res.error()));
  } else {
    std::println("  ✗ エラーが発生しなかった（予期しない動作）");
  }

  // flush_all は order violation チェックをスキップして残りを書き込む
  std::println("  ▶ flush_all(): 残りの行1を強制フラッシュ（違反チェックなし）");
  if (auto res = ws->flush_all(); !res) {
    std::println("  エラー: {}", error_str(res.error()));
  }

  std::ignore = wb.save();
}

// ──────────────────────────────────────────────────────────────
// デモ 5: DuplicateSheetName エラーハンドリング
// ──────────────────────────────────────────────────────────────
auto demo_duplicate_sheet() -> void {
  std::println("\n━━━ Demo 5: DuplicateSheetName エラーハンドリング ━━━");

  auto wb = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};

  std::println("  ▶ add_sheet(\"データ\"): 正常");
  std::ignore = wb.add_sheet("データ");

  std::println("  ▶ add_sheet(\"データ\"): DuplicateSheetName を期待");
  if (auto res = wb.add_sheet("データ"); !res) {
    std::println("  ✓ 期待通りエラーを捕捉: {}", error_str(res.error()));
  } else {
    std::println("  ✗ エラーが発生しなかった（予期しない動作）");
  }

  std::ignore = wb.save();
}

// ──────────────────────────────────────────────────────────────
// デモ 6: flush 後の再書き込みと再フラッシュ
// ──────────────────────────────────────────────────────────────
auto demo_rewrite_after_flush() -> void {
  std::println("\n━━━ Demo 6: flush 後の再書き込みと再フラッシュ ━━━");

  auto wb  = xlsxwriterpp::Workbook{xlsxwriterpp::MockBackend{}};
  auto* ws = wb.add_sheet("再書き込み検証").value();

  std::ignore = ws->write(0, 0, std::string_view{"初回書き込み"});
  std::println("  ▶ 行0 初回書き込み → flush_row(0)");
  std::ignore = ws->flush_row(0);

  // flush 後に同じ行へ再度書き込み
  std::ignore = ws->write(0, 0, std::string_view{"再書き込み（flush後）"});
  std::println("  ▶ 行0 再書き込み（flush後）→ flush_all()");
  if (auto res = ws->flush_all(); !res) {
    std::println("  エラー: {}", error_str(res.error()));
  }

  std::ignore = wb.save();
}

// ──────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────
auto main() -> int {
  std::println("╔══════════════════════════════════════╗");
  std::println("║  xlsxwriterpp ライブラリ 動作確認      ║");
  std::println("╚══════════════════════════════════════╝");

  demo_multi_sheet();
  demo_format_merge();
  demo_parallel_flush();
  demo_row_order_violation();
  demo_duplicate_sheet();
  demo_rewrite_after_flush();

  std::println("\n✓ 全デモ完了");
  return 0;
}
