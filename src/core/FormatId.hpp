#pragma once

// 画像フォーマットの識別子。強い型付き文字列（docs/spec-core.md §2.1）。
//
// 能力表内のインデックスにはしない。能力表は実行時生成のため、インデックスは生成順に
// 依存し、fromCapabilities() のテストダブルと本番で同じ値が別の形式を指しうる。
// 無効値の排除は CapabilityTable::find() が std::optional を返すことで担保する。
//
// **このファイルは「フォーマット名の文字列リテラル禁止」の唯一の例外である**
// （docs/spec-core.md §3。不変条件スキャナ INV3A はこのファイルだけを除外する）。
// ただし現時点で例外を必要とする記述は無い。変換関数は名前を知らずに包み直すだけで、
// 特定のフォーマット名を書いていない。例外枠を使う場合はここに書く。

#include <QString>
#include <QStringView>

namespace katachi::core {

struct FormatId {
    QString v;

    friend bool operator==(const FormatId&, const FormatId&) = default;
};

// 任意の文字列から識別子を作る。
//
// 前後の空白を落とし、小文字へ畳む。Qt が返す形式名は小文字だが、
// 呼び出し側が拡張子や利用者入力から作る場合は大文字が混じりうる。
// ここで正規化しないと、同じ形式を指す FormatId が operator== で別物になり、
// CapabilityTable::find() が取りこぼす。
//
// noexcept と確保失敗の扱いは ADR-0002 に従う。
[[nodiscard]] inline FormatId formatIdFromString(QStringView name) noexcept {
    return FormatId{name.trimmed().toString().toLower()};
}

// 引数名を id にしないのは clang-tidy の readability-identifier-length
// （3 文字未満のパラメータ名を禁止）による。docs/spec-core.md §2.1 が定める
// メンバ名 v は同チェックの対象外なので、そのまま維持している。
[[nodiscard]] inline QString formatIdToString(const FormatId& format) noexcept { return format.v; }

} // namespace katachi::core
