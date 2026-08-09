#include "core/CapabilityTable.hpp"

#include "core/Concepts.hpp"
#include "core/FormatId.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QIODevice>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QImageWriter>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace katachi::core {

// 契約の明文化（docs/spec-core.md §3、docs/cpp-conventions.md §2.3）。
// ヘッダに置くと Concepts.hpp との include が循環するためここに置く。
static_assert(CapabilitySource<CapabilityTable>);

namespace {

// 名前付き定数にしているのは clang-tidy の readability-magic-numbers /
// cppcoreguidelines-avoid-magic-numbers による。抑制コメントは使えない。
constexpr int channelMax = 255;
constexpr int alphaOpaque = 255;
constexpr int alphaTransparent = 0;
constexpr int alphaHalf = 128;
constexpr int alphaQuarter = 64;
constexpr int alphaProbeEdge = 2;
constexpr int losslessProbeEdge = 8;

// アルファ判定用の探針画像。完全透明の画素を 1 つ持つ。
// 固定パターンであり、乱数も時刻も使わない（core の禁止事項）。
[[nodiscard]] QImage alphaProbeImage() {
    QImage probe(alphaProbeEdge, alphaProbeEdge, QImage::Format_ARGB32);
    probe.setPixelColor(0, 0, QColor(channelMax, 0, 0, alphaHalf));
    probe.setPixelColor(1, 0, QColor(0, channelMax, 0, alphaTransparent));
    probe.setPixelColor(0, 1, QColor(0, 0, channelMax, alphaOpaque));
    probe.setPixelColor(1, 1, QColor(channelMax, channelMax, 0, alphaQuarter));
    return probe;
}

// 可逆性判定用の探針画像。1 画素ごとに白黒が入れ替わる高周波パターン。
// 非可逆コーデックの周波数変換では原理的に完全一致しない。
[[nodiscard]] QImage losslessProbeImage() {
    QImage probe(losslessProbeEdge, losslessProbeEdge, QImage::Format_RGB32);
    const QColor white(channelMax, channelMax, channelMax);
    const QColor black(0, 0, 0);

    for (int row = 0; row < probe.height(); ++row) {
        for (int column = 0; column < probe.width(); ++column) {
            probe.setPixelColor(column, row, ((column + row) % 2) == 0 ? white : black);
        }
    }
    return probe;
}

// メモリ上だけで書き出し→読み戻しを行う。ファイルには触れない。
[[nodiscard]] bool roundTrip(const QImage& source, const QByteArray& format, QImage* decoded) {
    QByteArray encoded;
    QBuffer sink(&encoded);
    if (!sink.open(QIODevice::WriteOnly)) {
        return false;
    }
    QImageWriter writer(&sink, format);
    if (!writer.canWrite() || !writer.write(source)) {
        return false;
    }
    sink.close();

    QBuffer origin(&encoded);
    if (!origin.open(QIODevice::ReadOnly)) {
        return false;
    }
    QImageReader reader(&origin, format);
    *decoded = reader.read();
    return !decoded->isNull();
}

[[nodiscard]] bool probeSupportsAlpha(const QByteArray& format) {
    QImage decoded;
    if (!roundTrip(alphaProbeImage(), format, &decoded)) {
        return false;
    }
    // 完全透明にした画素が透明のまま戻れば、その形式はアルファを保持できる。
    return decoded.convertToFormat(QImage::Format_ARGB32).pixelColor(1, 0).alpha() ==
           alphaTransparent;
}

[[nodiscard]] bool probeIsLossless(const QByteArray& format) {
    QImage decoded;
    if (!roundTrip(losslessProbeImage(), format, &decoded)) {
        return false;
    }
    return decoded.convertToFormat(QImage::Format_RGB32) ==
           losslessProbeImage().convertToFormat(QImage::Format_RGB32);
}

[[nodiscard]] bool probeSupportsQuality(const QByteArray& format) {
    QImageWriter writer;
    writer.setFormat(format);
    return writer.supportsOption(QImageIOHandler::Quality);
}

// 同一 FormatId の項目を 1 件へ畳む。
// 真偽値は論理和、extensions は和集合を取る（ADR-0006）。
// 「1 つの FormatId に対して項目はちょうど 1 件」を保証しないと
// find() がどれを返すか不定になる。
[[nodiscard]] std::vector<FormatCapability> mergeById(std::vector<FormatCapability> source) {
    QMap<QString, FormatCapability> merged;

    // 値渡しの引数を実際に消費する（move する）。const 参照で受けないのは
    // 呼び出し側が一時オブジェクトを渡すため、ここで所有権を引き取る方が写しが減るから。
    for (FormatCapability& entry : source) {
        const FormatId formatId = formatIdFromString(entry.id.v);
        auto found = merged.find(formatId.v);
        if (found == merged.end()) {
            FormatCapability normalized = std::move(entry);
            normalized.id = formatId;
            normalized.extensions.removeDuplicates();
            normalized.extensions.sort();
            // QMap::insert は値を const 参照で受けるため std::move しても写しは消えない。
            merged.insert(formatId.v, normalized);
            continue;
        }

        FormatCapability& target = found.value();
        target.canDecode = target.canDecode || entry.canDecode;
        target.canEncode = target.canEncode || entry.canEncode;
        target.supportsAlpha = target.supportsAlpha || entry.supportsAlpha;
        target.supportsQuality = target.supportsQuality || entry.supportsQuality;
        target.isLossless = target.isLossless || entry.isLossless;
        target.extensions.append(entry.extensions);
        target.extensions.removeDuplicates();
        target.extensions.sort();
    }

    // QMap は鍵の昇順に走査されるため、結果は id 昇順で決定的になる。
    return {merged.cbegin(), merged.cend()};
}

} // namespace

CapabilityTable::CapabilityTable(std::vector<FormatCapability> capabilities) noexcept
    : capabilities_(std::move(capabilities)) {}

CapabilityTable CapabilityTable::buildFromQt() {
    std::vector<FormatCapability> collected;

    const QList<QByteArray> readable = QImageReader::supportedImageFormats();
    const QList<QByteArray> writable = QImageWriter::supportedImageFormats();

    for (const QByteArray& name : readable) {
        FormatCapability entry;
        entry.id = formatIdFromString(QString::fromUtf8(name));
        entry.extensions = QStringList{QString::fromUtf8(name)};
        entry.canDecode = true;
        collected.push_back(entry);
    }

    for (const QByteArray& name : writable) {
        FormatCapability entry;
        entry.id = formatIdFromString(QString::fromUtf8(name));
        entry.extensions = QStringList{QString::fromUtf8(name)};
        entry.canEncode = true;
        // 書き出せる形式だけ実測できる。読み込み専用の形式は往復できないため
        // 判定不能であり、false のままとする（ADR-0007）。
        entry.supportsAlpha = probeSupportsAlpha(name);
        entry.supportsQuality = probeSupportsQuality(name);
        entry.isLossless = probeIsLossless(name);
        collected.push_back(entry);
    }

    return CapabilityTable(mergeById(std::move(collected)));
}

CapabilityTable CapabilityTable::fromCapabilities(std::vector<FormatCapability> capabilities) {
    return CapabilityTable(mergeById(std::move(capabilities)));
}

std::optional<FormatCapability> CapabilityTable::find(const FormatId& format) const noexcept {
    const auto found =
        std::ranges::find_if(capabilities_, [&format](const FormatCapability& candidate) {
            return candidate.id == format;
        });

    if (found == capabilities_.cend()) {
        return std::nullopt;
    }
    return *found;
}

std::vector<FormatCapability> CapabilityTable::encodable() const {
    std::vector<FormatCapability> result;
    std::ranges::copy_if(capabilities_, std::back_inserter(result),
                         [](const FormatCapability& candidate) { return candidate.canEncode; });
    return result;
}

} // namespace katachi::core
