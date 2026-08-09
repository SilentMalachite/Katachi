#pragma once

// 画像フォーマットの識別子。強い型付き文字列（docs/spec-core.md §2.1）。
//
// 能力表内のインデックスにはしない。能力表は実行時生成のため、インデックスは生成順に
// 依存し、fromCapabilities() のテストダブルと本番で同じ値が別の形式を指しうる。
// 無効値の排除は CapabilityTable::find() が std::optional を返すことで担保する。
//
// **このファイルは「フォーマット名の文字列リテラル禁止」の唯一の例外である**
// （docs/spec-core.md §3。不変条件スキャナ INV3A はこのファイルだけを除外する）。
// 下の別名表がその例外枠を使う唯一の箇所であり、ここ以外に増やさない。

#include <QString>
#include <QStringView>

namespace katachi::core {

struct FormatId {
    QString v;

    friend bool operator==(const FormatId&, const FormatId&) = default;
};

// 任意の文字列から識別子を作る。正規化の内容は ADR-0006。
//
// 1. 前後の空白を落とす
// 2. 小文字へ畳む — Qt が返す形式名は小文字だが、呼び出し側が拡張子や
//    利用者入力から作る場合は大文字が混じりうる
// 3. 別名を代表名へ畳む — 下記
//
// 正規化しないと、同じ形式を指す FormatId が operator== で別物になり、
// CapabilityTable::find() が取りこぼす。
//
// 別名を畳む基準は「Qt が同一の MIME タイプを報告すること」。
// 実測（Qt 6.11.1 / macOS）では jpeg・jpg・jfif がいずれも image/jpeg、
// tif・tiff がいずれも image/tiff を返す。
// heic と heif は image/heic と image/heif で別の MIME のため畳まない。
//
// 代表名の側（jpeg / tiff）は Qt の対応形式一覧に必ず含まれることを実測で確認済み。
// また CapabilityTable も同じこの関数で正規化するため、表の鍵と問い合わせの鍵が
// 常に一致する（片側だけ畳まれて引けなくなることはない）。
//
// noexcept と確保失敗の扱いは ADR-0002 に従う。
[[nodiscard]] inline FormatId formatIdFromString(QStringView name) noexcept {
    const QString folded = name.trimmed().toString().toLower();

    if (folded == QStringView(u"jpg") || folded == QStringView(u"jfif")) {
        return FormatId{QStringView(u"jpeg").toString()};
    }
    if (folded == QStringView(u"tif")) {
        return FormatId{QStringView(u"tiff").toString()};
    }

    return FormatId{folded};
}

// 引数名を id にしないのは clang-tidy の readability-identifier-length
// （3 文字未満のパラメータ名を禁止）による。docs/spec-core.md §2.1 が定める
// メンバ名 v は同チェックの対象外なので、そのまま維持している。
[[nodiscard]] inline QString formatIdToString(const FormatId& format) noexcept { return format.v; }

} // namespace katachi::core
