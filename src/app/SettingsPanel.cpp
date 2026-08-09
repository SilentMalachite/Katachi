#include "app/SettingsPanel.hpp"

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
#include <QVariant>
#include <QWidget>

#include <optional>

namespace katachi::app {
namespace {

// docs/spec-core.md §2 の範囲。
constexpr int minQualityValue = 0;
constexpr int maxQualityValue = 100;
constexpr int defaultQualityValue = 90;

// リサイズの上限。maxPixels の 1 辺（16384）に合わせる。
constexpr int maxResizeEdge = 16384;
constexpr int defaultResizeEdge = 1920;

// 表示文言はこの層にしか置かない（ADR-0010）。**フォーマット名は書かない。**
// 形式と拡張子の選択肢は能力表から作る。

void addPolicyItem(QComboBox* combo, const QString& text, int value) {
    combo->addItem(text, value);
}

[[nodiscard]] int currentValue(const QComboBox* combo) { return combo->currentData().toInt(); }

void selectValue(QComboBox* combo, int value) {
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

// 形式と拡張子は文字列を userData に入れているため、こちらで引く。
void selectValueByString(QComboBox* combo, const QString& value) {
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

} // namespace

SettingsPanel::SettingsPanel(const core::CapabilityTable& caps, QWidget* parent)
    : QWidget(parent), caps_(&caps), format_(new QComboBox(this)), extension_(new QComboBox(this)),
      quality_(new QSpinBox(this)), alpha_(new QComboBox(this)), metadata_(new QComboBox(this)),
      icc_(new QComboBox(this)), collision_(new QComboBox(this)), pattern_(new QLineEdit(this)),
      resizeWidth_(new QSpinBox(this)), resizeHeight_(new QSpinBox(this)),
      resizeEnabled_(new QCheckBox(this)), form_(new QFormLayout(this)) {
    // 出力形式。**能力表から作る。**
    for (const core::FormatCapability& capability : caps_->encodable()) {
        format_->addItem(core::formatIdToString(capability.id),
                         core::formatIdToString(capability.id));
    }

    quality_->setRange(minQualityValue, maxQualityValue);
    quality_->setValue(defaultQualityValue);

    resizeWidth_->setRange(1, maxResizeEdge);
    resizeWidth_->setValue(defaultResizeEdge);
    resizeHeight_->setRange(1, maxResizeEdge);
    resizeHeight_->setValue(defaultResizeEdge);
    resizeEnabled_->setChecked(false);
    resizeWidth_->setEnabled(false);
    resizeHeight_->setEnabled(false);

    addPolicyItem(alpha_, tr("そのまま保つ"), static_cast<int>(core::AlphaPolicy::Preserve));
    addPolicyItem(alpha_, tr("背景色で合成する"), static_cast<int>(core::AlphaPolicy::Flatten));
    addPolicyItem(alpha_, tr("失うなら中止する"), static_cast<int>(core::AlphaPolicy::Reject));

    addPolicyItem(metadata_, tr("扱える範囲で残す"),
                  static_cast<int>(core::MetadataPolicy::PreserveSupported));
    addPolicyItem(metadata_, tr("すべて削除する"),
                  static_cast<int>(core::MetadataPolicy::StripAll));

    addPolicyItem(icc_, tr("埋め込む"), static_cast<int>(core::IccPolicy::Embed));
    addPolicyItem(icc_, tr("削除する"), static_cast<int>(core::IccPolicy::Strip));

    // **既定は Skip。破壊的操作を既定にしない**（ADR-0005 / ADR-0009）。
    addPolicyItem(collision_, tr("スキップする"), static_cast<int>(io::CollisionPolicy::Skip));
    addPolicyItem(collision_, tr("別名で保存する"), static_cast<int>(io::CollisionPolicy::Rename));
    addPolicyItem(collision_, tr("上書きする"), static_cast<int>(io::CollisionPolicy::Overwrite));

    pattern_->setText(tr("{name}.{ext}"));

    form_->addRow(tr("出力形式"), format_);
    form_->addRow(tr("拡張子"), extension_);
    form_->addRow(tr("品質"), quality_);
    form_->addRow(tr("大きさを変える"), resizeEnabled_);
    form_->addRow(tr("幅の上限"), resizeWidth_);
    form_->addRow(tr("高さの上限"), resizeHeight_);
    form_->addRow(tr("透明部分"), alpha_);
    form_->addRow(tr("メタデータ"), metadata_);
    form_->addRow(tr("カラープロファイル"), icc_);
    form_->addRow(tr("名前の付け方"), pattern_);
    form_->addRow(tr("同名のファイルがある場合"), collision_);

    // docs/spec-core.md §7: タブ順を明示する。並べた順にたどれるようにする。
    setTabOrder(format_, extension_);
    setTabOrder(extension_, quality_);
    setTabOrder(quality_, resizeEnabled_);
    setTabOrder(resizeEnabled_, resizeWidth_);
    setTabOrder(resizeWidth_, resizeHeight_);
    setTabOrder(resizeHeight_, alpha_);
    setTabOrder(alpha_, metadata_);
    setTabOrder(metadata_, icc_);
    setTabOrder(icc_, pattern_);
    setTabOrder(pattern_, collision_);

    connect(format_, &QComboBox::currentIndexChanged, this, [this](int) { rebuildExtensions(); });
    connect(resizeEnabled_, &QCheckBox::toggled, this, [this](bool enabled) {
        resizeWidth_->setEnabled(enabled);
        resizeHeight_->setEnabled(enabled);
    });

    rebuildExtensions();
}

void SettingsPanel::rebuildExtensions() {
    extension_->clear();

    const auto capability = caps_->find(currentFormat());
    if (!capability.has_value()) {
        return;
    }

    for (const QString& value : capability->extensions) {
        extension_->addItem(value, value);
    }

    // 既定は代表名（ADR-0006）。選ばなければ従来どおりの名前になる。
    const QString representative = core::formatIdToString(capability->id);
    const int index = extension_->findData(representative);
    extension_->setCurrentIndex(index >= 0 ? index : 0);

    // 品質を扱えない形式では欄を触れなくする。値だけ残しても意味が無い。
    quality_->setEnabled(capability->supportsQuality);
}

core::FormatId SettingsPanel::currentFormat() const {
    return core::formatIdFromString(format_->currentData().toString());
}

QList<core::FormatId> SettingsPanel::availableFormats() const {
    QList<core::FormatId> formats;
    formats.reserve(format_->count());
    for (int index = 0; index < format_->count(); ++index) {
        formats.append(core::formatIdFromString(format_->itemData(index).toString()));
    }
    return formats;
}

void SettingsPanel::selectFormat(const core::FormatId& format) {
    selectValueByString(format_, core::formatIdToString(format));
}

QList<QString> SettingsPanel::availableExtensions() const {
    QList<QString> extensions;
    extensions.reserve(extension_->count());
    for (int index = 0; index < extension_->count(); ++index) {
        extensions.append(extension_->itemData(index).toString());
    }
    return extensions;
}

void SettingsPanel::selectExtension(const QString& extension) {
    selectValueByString(extension_, extension);
}

bool SettingsPanel::isQualityEnabled() const { return quality_->isEnabled(); }

void SettingsPanel::setQuality(int quality) { quality_->setValue(quality); }

void SettingsPanel::setResizeBound(const std::optional<QSize>& bound) {
    resizeEnabled_->setChecked(bound.has_value());
    if (bound.has_value()) {
        resizeWidth_->setValue(bound->width());
        resizeHeight_->setValue(bound->height());
    }
}

void SettingsPanel::setAlphaPolicy(core::AlphaPolicy policy) {
    selectValue(alpha_, static_cast<int>(policy));
}

void SettingsPanel::setMetadataPolicy(core::MetadataPolicy policy) {
    selectValue(metadata_, static_cast<int>(policy));
}

void SettingsPanel::setIccPolicy(core::IccPolicy policy) {
    selectValue(icc_, static_cast<int>(policy));
}

void SettingsPanel::setNamePattern(const core::NamePattern& pattern) {
    pattern_->setText(pattern.v);
}

void SettingsPanel::setCollisionPolicy(io::CollisionPolicy policy) {
    selectValue(collision_, static_cast<int>(policy));
}

core::ConversionSpec SettingsPanel::spec() const {
    core::ConversionSpec spec;
    spec.target = currentFormat();
    spec.quality = quality_->value();
    spec.resize = resizeEnabled_->isChecked()
                      ? std::optional<QSize>(QSize(resizeWidth_->value(), resizeHeight_->value()))
                      : std::nullopt;
    spec.alpha = static_cast<core::AlphaPolicy>(currentValue(alpha_));
    spec.metadata = static_cast<core::MetadataPolicy>(currentValue(metadata_));
    spec.icc = static_cast<core::IccPolicy>(currentValue(icc_));
    return spec;
}

core::NamePattern SettingsPanel::namePattern() const { return core::NamePattern{pattern_->text()}; }

core::NameExtension SettingsPanel::extension() const {
    return core::NameExtension{extension_->currentData().toString()};
}

io::CollisionPolicy SettingsPanel::collisionPolicy() const {
    return static_cast<io::CollisionPolicy>(currentValue(collision_));
}

} // namespace katachi::app
