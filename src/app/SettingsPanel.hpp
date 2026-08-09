#pragma once

// 変換設定の入力欄（docs/spec-core.md §1 / ADR-0010）。
//
// **選択肢はすべて能力表から作る。** フォーマット名の文字列リテラルを書かない
// （docs/spec-core.md §3 / 不変条件 INV3B）。
//
// 値の読み出しはウィジェットを外へ晒さず、意味のある単位で返す。
// テストも MainWindow も、この API 越しに扱う。

#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/FormatId.hpp"
#include "core/NamingRule.hpp"
#include "io/CollisionPolicy.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QList>
#include <QObject>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include <optional>

namespace katachi::app {

class SettingsPanel : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPanel(const core::CapabilityTable& caps, QWidget* parent = nullptr);

    // --- 出力形式と拡張子 ---

    [[nodiscard]] QList<core::FormatId> availableFormats() const;
    void selectFormat(const core::FormatId& format);

    // 選択中の形式が持つ拡張子（能力表の extensions。別名の和集合）。
    [[nodiscard]] QList<QString> availableExtensions() const;
    void selectExtension(const QString& extension);

    // --- 各設定 ---

    [[nodiscard]] bool isQualityEnabled() const;
    void setQuality(int quality);

    // std::nullopt でリサイズ無し。
    void setResizeBound(const std::optional<QSize>& bound);

    void setAlphaPolicy(core::AlphaPolicy policy);
    void setMetadataPolicy(core::MetadataPolicy policy);
    void setIccPolicy(core::IccPolicy policy);
    void setNamePattern(const core::NamePattern& pattern);
    void setCollisionPolicy(io::CollisionPolicy policy);

    // --- 画面の値から作る ---

    [[nodiscard]] core::ConversionSpec spec() const;
    [[nodiscard]] core::NamePattern namePattern() const;
    [[nodiscard]] core::NameExtension extension() const;
    [[nodiscard]] io::CollisionPolicy collisionPolicy() const;

private:
    void rebuildExtensions();

    [[nodiscard]] core::FormatId currentFormat() const;

    const core::CapabilityTable* caps_;

    QComboBox* format_;
    QComboBox* extension_;
    QSpinBox* quality_;
    QComboBox* alpha_;
    QComboBox* metadata_;
    QComboBox* icc_;
    QComboBox* collision_;
    QLineEdit* pattern_;
    QSpinBox* resizeWidth_;
    QSpinBox* resizeHeight_;
    QCheckBox* resizeEnabled_;

    // レイアウトをメンバに持つのは clang-tidy の cppcoreguidelines-owning-memory による。
    // 局所変数や引数として new を渡すと「非所有ポインタに所有権を入れている」と指摘される
    // （メンバ初期化子では指摘されない）。docs/cpp-conventions.md §1 は app 層での
    // Qt の親付き new を明示的に許しており、寿命は親ウィジェットが持つ。
    QFormLayout* form_;
};

} // namespace katachi::app
