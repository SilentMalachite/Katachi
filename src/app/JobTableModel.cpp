#include "app/JobTableModel.hpp"

#include "core/ConvertError.hpp"
#include "core/NamingRule.hpp"
#include "io/IoError.hpp"
#include "io/JobRunner.hpp"

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>
// emit マクロの提供元。Qt 6 では qtmetamacros.h にある（T6 で実測して確定させた）。
#include <QtCore/qtmetamacros.h>

#include <optional>
#include <type_traits>
#include <variant>

namespace katachi::app {
namespace {

// 列の並び。docs/spec-core.md §1 の「入力 / 出力 / 状態 / 理由」。
//
// enum ではなく定数にするのは、QModelIndex::column() が int を返すため。
// enum class にすると比較のたびに変換が要り、素の enum は
// cpp-conventions.md §1 と clang-tidy の cppcoreguidelines-use-enum-class が禁じる。
constexpr int sourceColumn = 0;
constexpr int outputColumn = 1;
constexpr int statusColumn = 2;
constexpr int reasonColumn = 3;
constexpr int columnCountValue = 4;

// **表示文言はこの層にしか置かない**（ADR-0010）。
// フォーマット名を含む文言は書かない（不変条件 INV3B）。形式名は能力表から来る。

[[nodiscard]] QString statusText(const std::optional<io::JobOutcome>& outcome) {
    if (!outcome.has_value()) {
        return QObject::tr("待機中");
    }

    switch (outcome->status) {
    case io::JobStatus::Succeeded:
        return QObject::tr("成功");
    case io::JobStatus::Skipped:
        return QObject::tr("スキップ");
    case io::JobStatus::Cancelled:
        return QObject::tr("キャンセル");
    case io::JobStatus::Failed:
        return QObject::tr("失敗");
    }

    return QObject::tr("失敗");
}

[[nodiscard]] QString convertErrorText(core::ConvertError error) {
    switch (error) {
    case core::ConvertError::EmptyInput:
        return QObject::tr("入力が空です");
    case core::ConvertError::DecodeFailed:
        return QObject::tr("画像として読み取れません");
    case core::ConvertError::UnsupportedTarget:
        return QObject::tr("この出力形式には書き出せません");
    case core::ConvertError::EncodeFailed:
        return QObject::tr("書き出しに失敗しました");
    case core::ConvertError::AlphaLossNotAllowed:
        return QObject::tr("透明部分を失うため中止しました");
    case core::ConvertError::ImageTooLarge:
        return QObject::tr("画素数の上限を超えています");
    }

    return {};
}

[[nodiscard]] QString namingErrorText(core::NamingError error) {
    switch (error) {
    case core::NamingError::EmptyPattern:
        return QObject::tr("命名規則が空です");
    case core::NamingError::UnknownPlaceholder:
        return QObject::tr("命名規則に未知の項目があります");
    case core::NamingError::InvalidIndexSpec:
        return QObject::tr("命名規則の桁指定が不正です");
    case core::NamingError::EmptyResult:
        return QObject::tr("出力ファイル名が空になります");
    }

    return {};
}

[[nodiscard]] QString ioErrorText(io::IoError error) {
    switch (error) {
    case io::IoError::NotFound:
        return QObject::tr("入力が見つかりません");
    case io::IoError::OpenFailed:
        return QObject::tr("入力を開けません");
    case io::IoError::WriteFailed:
        return QObject::tr("出力先に書き出せません");
    case io::IoError::TooLarge:
        return QObject::tr("入力が大きすぎます");
    case io::IoError::DestinationExists:
        return QObject::tr("同名のファイルが既にあります");
    }

    return {};
}

[[nodiscard]] QString warningText(core::ConvertWarning warning) {
    switch (warning) {
    case core::ConvertWarning::AlphaFlattenedFallback:
        return QObject::tr("透明部分を背景色で合成しました");
    }

    return {};
}

// 理由の列。失敗とスキップは理由を、成功は警告があればそれを出す。
// **成功しても「指定どおりには処理できなかった」ことは伝える**（ADR-0004）。
[[nodiscard]] QString reasonText(const std::optional<io::JobOutcome>& outcome) {
    if (!outcome.has_value()) {
        return {};
    }

    if (outcome->failure.has_value()) {
        return std::visit(
            [](auto&& error) -> QString {
                using Error = std::decay_t<decltype(error)>;
                if constexpr (std::is_same_v<Error, core::ConvertError>) {
                    return convertErrorText(error);
                } else if constexpr (std::is_same_v<Error, core::NamingError>) {
                    return namingErrorText(error);
                } else {
                    return ioErrorText(error);
                }
            },
            *outcome->failure);
    }

    QString warnings;
    for (const core::ConvertWarning warning : outcome->warnings) {
        if (!warnings.isEmpty()) {
            warnings += QObject::tr(" / ");
        }
        warnings += warningText(warning);
    }

    return warnings;
}

[[nodiscard]] QString fileNameOf(const QString& path) {
    return path.isEmpty() ? QString() : QFileInfo(path).fileName();
}

} // namespace

JobTableModel::JobTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void JobTableModel::setJobs(const QList<QString>& sourcePaths) {
    beginResetModel();

    rows_.clear();
    rowBySourcePath_.clear();
    rows_.reserve(sourcePaths.size());

    for (const QString& path : sourcePaths) {
        // 同じ入力が二度来た場合は 1 行にまとめる。行と結果の対応が 1 対 1 でなくなるため。
        if (rowBySourcePath_.contains(path)) {
            continue;
        }
        rowBySourcePath_.insert(path, static_cast<int>(rows_.size()));
        rows_.append(Row{.sourcePath = path, .outcome = std::nullopt});
    }

    endResetModel();
}

void JobTableModel::applyOutcomes(const QList<io::JobOutcome>& outcomes) {
    for (const io::JobOutcome& outcome : outcomes) {
        const auto found = rowBySourcePath_.constFind(outcome.sourcePath);
        if (found == rowBySourcePath_.constEnd()) {
            // 表に無い入力の結果。黙って捨てる。行を増やすと並びが崩れる。
            continue;
        }

        const int row = found.value();
        rows_[row].outcome = outcome;

        // **その行だけを更新する。** 全体を reset すると選択もスクロール位置も飛ぶ。
        emit dataChanged(index(row, 0), index(row, columnCountValue - 1));
    }
}

void JobTableModel::clear() { setJobs({}); }

int JobTableModel::failedCount() const noexcept {
    int count = 0;
    for (const Row& row : rows_) {
        if (row.outcome.has_value() && row.outcome->status == io::JobStatus::Failed) {
            ++count;
        }
    }
    return count;
}

int JobTableModel::skippedCount() const noexcept {
    int count = 0;
    for (const Row& row : rows_) {
        if (row.outcome.has_value() && row.outcome->status == io::JobStatus::Skipped) {
            ++count;
        }
    }
    return count;
}

int JobTableModel::succeededCount() const noexcept {
    int count = 0;
    for (const Row& row : rows_) {
        if (row.outcome.has_value() && row.outcome->status == io::JobStatus::Succeeded) {
            ++count;
        }
    }
    return count;
}

int JobTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int JobTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : columnCountValue;
}

QVariant JobTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }
    if (index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }

    const Row& row = rows_.at(index.row());

    switch (index.column()) {
    case sourceColumn:
        return fileNameOf(row.sourcePath);
    case outputColumn:
        return row.outcome.has_value() ? fileNameOf(row.outcome->outputPath) : QString();
    case statusColumn:
        return statusText(row.outcome);
    case reasonColumn:
        return reasonText(row.outcome);
    default:
        return {};
    }
}

QVariant JobTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    switch (section) {
    case sourceColumn:
        return tr("入力");
    case outputColumn:
        return tr("出力");
    case statusColumn:
        return tr("状態");
    case reasonColumn:
        return tr("理由");
    default:
        return {};
    }
}

} // namespace katachi::app
