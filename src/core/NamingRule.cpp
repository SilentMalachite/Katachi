#include "core/NamingRule.hpp"

#include "core/Result.hpp"

#include <QChar>
#include <QString>
#include <QStringView>
#include <QtTypes>

namespace katachi::core {
namespace {

using NamingResult = Result<QString, NamingError>;

// 桁指定の上限。上限を設けないと、巨大な指定で確保が走り std::terminate しうる
// （ADR-0002）。int の十進表現は最大 11 文字なので、32 あれば実用上足りる。
constexpr qsizetype maxIndexWidth = 32;
constexpr int decimalBase = 10;

// 差し込む値をひとまとめにする。個別の引数で渡すと同じ型が隣り合い、
// 呼び出し側が取り違えやすくなる（clang-tidy: bugprone-easily-swappable-parameters）。
struct NamingInputs {
    QString baseName;
    QString extension;
    int index = 0;
};

// 桁数を裸の整数で渡すと index と隣り合って取り違えやすい。型で分ける。
struct IndexWidth {
    qsizetype value = 0;
};

// {index:N} の N を読む。数字のみを受け付ける。戻り値が負なら不正。
[[nodiscard]] qsizetype parseIndexWidth(QStringView spec) noexcept {
    if (spec.isEmpty()) {
        return -1;
    }
    qsizetype width = 0;
    for (const QChar character : spec) {
        if (!character.isDigit()) {
            return -1;
        }
        width = (width * decimalBase) + (character.unicode() - u'0');
        if (width > maxIndexWidth) {
            return -1;
        }
    }
    return width;
}

// 符号を保ったまま 0 詰めする。-7 を幅 3 で詰めると "-007"。
// 幅より長い数はそのまま返す（切り詰めない）。
[[nodiscard]] QString padIndex(int index, IndexWidth width) noexcept {
    const bool negative = index < 0;
    QString digits = QString::number(negative ? -static_cast<qint64>(index) : index);

    while (digits.size() < width.value) {
        digits.prepend(u'0');
    }
    if (negative) {
        digits.prepend(u'-');
    }
    return digits;
}

// 波括弧の中身をひとつ展開する。body は括弧を含まない。
[[nodiscard]] NamingResult expandPlaceholder(QStringView body,
                                             const NamingInputs& inputs) noexcept {
    const qsizetype colon = body.indexOf(u':');
    const QStringView key = colon < 0 ? body : body.first(colon);
    const bool hasSpec = colon >= 0;

    if (key == QStringView(u"name")) {
        // {name} と {ext} は桁指定を取らない。付いていたら書式の誤り。
        return hasSpec ? NamingResult::err(NamingError::UnknownPlaceholder)
                       : NamingResult::ok(inputs.baseName);
    }
    if (key == QStringView(u"ext")) {
        return hasSpec ? NamingResult::err(NamingError::UnknownPlaceholder)
                       : NamingResult::ok(inputs.extension);
    }
    if (key == QStringView(u"index")) {
        if (!hasSpec) {
            return NamingResult::ok(padIndex(inputs.index, IndexWidth{}));
        }
        const qsizetype width = parseIndexWidth(body.sliced(colon + 1));
        if (width < 0) {
            return NamingResult::err(NamingError::InvalidIndexSpec);
        }
        return NamingResult::ok(padIndex(inputs.index, IndexWidth{.value = width}));
    }

    return NamingResult::err(NamingError::UnknownPlaceholder);
}

} // namespace

Result<QString, NamingError> resolveOutputName(const QString& sourceBaseName, int index,
                                               const NamePattern& pattern,
                                               const NameExtension& extension) noexcept {
    if (pattern.v.isEmpty()) {
        return NamingResult::err(NamingError::EmptyPattern);
    }

    const NamingInputs inputs{.baseName = sourceBaseName, .extension = extension.v, .index = index};
    QString resolved;

    for (qsizetype position = 0; position < pattern.v.size(); ++position) {
        const QChar character = pattern.v.at(position);
        if (character != u'{') {
            resolved.append(character);
            continue;
        }

        const qsizetype close = pattern.v.indexOf(u'}', position + 1);
        if (close < 0) {
            // 閉じ括弧が無いものを literal 扱いにしない。打ち間違いを見逃さないため。
            return NamingResult::err(NamingError::UnknownPlaceholder);
        }

        const NamingResult expanded = expandPlaceholder(
            QStringView(pattern.v).sliced(position + 1, close - position - 1), inputs);
        if (!expanded.isOk()) {
            return expanded;
        }
        resolved.append(expanded.value());
        position = close;
    }

    if (resolved.isEmpty()) {
        return NamingResult::err(NamingError::EmptyResult);
    }

    return NamingResult::ok(resolved);
}

} // namespace katachi::core
