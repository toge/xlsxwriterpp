#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace xlsxwriterpp {

/**
 * @brief 水平方向の配置を表す列挙型。
 *
 * Default は Excel のデータ型ごとのデフォルト配置を使用する。
 */
enum class HorizontalAlign : uint8_t {
  Default, ///< デフォルト（Excel 既定）
  Left,    ///< 左揃え
  Center,  ///< 中央揃え
  Right,   ///< 右揃え
  Justify, ///< 均等割り付け
};

/**
 * @brief 垂直方向の配置を表す列挙型。
 *
 * Default は Excel の既定（通常は下揃え）を使用する。
 */
enum class VerticalAlign : uint8_t {
  Default,  ///< デフォルト（Excel 既定）
  Top,      ///< 上揃え
  Center,   ///< 中央揃え
  Bottom,   ///< 下揃え
  VJustify, ///< 縦方向均等割り付け
};

/**
 * @brief セルの書式設定プロパティ構造体。
 *
 * 各フィールドは「未設定」状態を持つ（デフォルト値を参照）。
 * merge() によって既存書式に差分書式を重ね合わせる運用を想定する。
 *
 * - std::optional フィールド: has_value() が true の場合のみ上書き
 * - std::string フィールド: 空文字列でない場合のみ上書き
 * - double フィールド: 0.0 でない場合のみ上書き
 * - bool フィールド: true の場合のみ上書き（解除操作は本ライブラリのスコープ外）
 * - enum フィールド: Default でない場合のみ上書き
 */
struct FormatProperties {
  std::string             font_name;                          ///< フォント名（空文字 = 未設定）
  double                  font_size{0.0};                     ///< フォントサイズ pt（0.0 = 未設定）
  bool                    bold{false};                        ///< 太字（false = 未設定）
  bool                    italic{false};                      ///< 斜体（false = 未設定）
  std::optional<uint32_t> font_color;                        ///< 文字色 RGB（nullopt = 未設定）
  std::optional<uint32_t> bg_color;                          ///< 背景色 RGB（nullopt = 未設定）
  HorizontalAlign         h_align{HorizontalAlign::Default}; ///< 水平配置
  VerticalAlign           v_align{VerticalAlign::Default};   ///< 垂直配置

  /**
   * @brief 別の書式プロパティをこのオブジェクトにマージ（重ね合わせ）する。
   *
   * other の各フィールドが「設定済み」（デフォルト値でない）場合のみ上書きする。
   * これにより、後から一部のプロパティだけを変更する操作が可能になる。
   *
   * @param other 上書きに使う書式プロパティ
   */
  auto merge(FormatProperties const& other) noexcept -> void {
    if (!other.font_name.empty()) {
      font_name = other.font_name;
    }
    if (other.font_size != 0.0) {
      font_size = other.font_size;
    }
    if (other.bold) {
      bold = true;
    }
    if (other.italic) {
      italic = true;
    }
    if (other.font_color.has_value()) {
      font_color = other.font_color;
    }
    if (other.bg_color.has_value()) {
      bg_color = other.bg_color;
    }
    if (other.h_align != HorizontalAlign::Default) {
      h_align = other.h_align;
    }
    if (other.v_align != VerticalAlign::Default) {
      v_align = other.v_align;
    }
  }

  [[nodiscard]] auto has_any() const noexcept -> bool {
    return !font_name.empty()
        || font_size != 0.0
        || bold
        || italic
        || font_color.has_value()
        || bg_color.has_value()
        || h_align != HorizontalAlign::Default
        || v_align != VerticalAlign::Default;
  }
};

} // namespace xlsxwriterpp
