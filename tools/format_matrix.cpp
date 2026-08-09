// docs/format-matrix.md の生成器。
//
// docs/phases.md §4 Phase 1: 「docs/format-matrix.md がビルド時に自動生成される」
//
// 能力表は実行環境の Qt プラグイン構成で決まるため、この文書は環境ごとに内容が変わる。
// リポジトリにはコミットしない（.gitignore 済み）。
//
// 使い方: katachi_format_matrix <出力ファイル>
//
// 表が空、あるいは PNG が見つからない場合は非ゼロ終了する。
// **中身の無い一覧を黙って出力しない。**
#include "core/CapabilityTable.hpp"
#include "core/FormatId.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QImageReader>
#include <QImageWriter>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QtGlobal>

#include <algorithm>
#include <vector>

namespace {

QTextStream& err() {
    static QTextStream stream(stderr);
    return stream;
}

QString mark(bool value) { return value ? QStringLiteral("o") : QStringLiteral("-"); }

// CapabilityTable は全項目を返す取得子を持たない（docs/spec-core.md §3 のとおり
// find() と encodable() のみ）。読み込み専用の形式も一覧に載せたいので、
// Qt が報告する名前を正規化して重複を除き、それぞれ find() で引く。
std::vector<katachi::core::FormatCapability>
collectAll(const katachi::core::CapabilityTable& caps) {
    QSet<QString> seen;
    std::vector<katachi::core::FormatCapability> rows;

    QList<QByteArray> names = QImageReader::supportedImageFormats();
    names.append(QImageWriter::supportedImageFormats());

    for (const QByteArray& name : names) {
        const katachi::core::FormatId id =
            katachi::core::formatIdFromString(QString::fromUtf8(name));
        if (seen.contains(id.v)) {
            continue;
        }
        seen.insert(id.v);

        const auto found = caps.find(id);
        if (found.has_value()) {
            rows.push_back(*found);
        }
    }

    std::ranges::sort(
        rows, [](const katachi::core::FormatCapability& lhs,
                 const katachi::core::FormatCapability& rhs) { return lhs.id.v < rhs.id.v; });
    return rows;
}

} // namespace

int main(int argc, char* argv[]) {
    const QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() != 2) {
        err() << "usage: katachi_format_matrix <output-file>\n";
        return 2;
    }

    const katachi::core::CapabilityTable caps = katachi::core::CapabilityTable::buildFromQt();
    const std::vector<katachi::core::FormatCapability> rows = collectAll(caps);

    if (rows.empty()) {
        err() << "能力表が空。Qt の画像プラグインが読み込まれていない可能性がある\n";
        return 1;
    }
    if (!caps.find(katachi::core::formatIdFromString(QStringLiteral("png"))).has_value()) {
        err() << "PNG が能力表に無い。環境が壊れている\n";
        return 1;
    }

    QFile out(args.at(1));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        err() << "出力ファイルを開けない: " << args.at(1) << "\n";
        return 1;
    }

    QTextStream text(&out);
    text << "# 対応フォーマット一覧\n\n";
    text << "**このファイルはビルド時に自動生成される。手で編集しない。**\n";
    text << "生成元: `tools/format_matrix.cpp`\n\n";
    text << "対応フォーマットはハードコードせず、実行時に `QImageReader` / `QImageWriter` から\n";
    text << "能力表を生成している（`CLAUDE.md`）。**内容は実行環境の Qt "
            "プラグイン構成で決まる。**\n\n";
    text << "- Qt バージョン: " << QString::fromUtf8(qVersion()) << "\n\n";
    text << "「アルファ」「品質指定」「可逆」はメモリ上の往復で実測している（ADR-0007）。\n";
    text << "**書き出せない形式は往復できないため判定できず、いずれも `-` になる。**\n";
    text << "この `-` は「その性質が無い」ではなく「判定していない」を意味する。\n\n";

    text << "| フォーマット | 拡張子 | 読み込み | 書き出し | アルファ | 品質指定 | 可逆 |\n";
    text << "|---|---|---|---|---|---|---|\n";

    for (const katachi::core::FormatCapability& row : rows) {
        text << "| `" << row.id.v << "` | " << row.extensions.join(QStringLiteral(", ")) << " | "
             << mark(row.canDecode) << " | " << mark(row.canEncode) << " | "
             << mark(row.supportsAlpha) << " | " << mark(row.supportsQuality) << " | "
             << mark(row.isLossless) << " |\n";
    }

    text.flush();
    out.close();
    return 0;
}
