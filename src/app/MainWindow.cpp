#include "app/MainWindow.hpp"

#include "app/JobTableModel.hpp"
#include "app/SettingsPanel.hpp"
#include "core/CapabilityTable.hpp"
#include "core/FormatId.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/JobRunner.hpp"
#include "io/JobRunnerBridge.hpp"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <optional>

namespace katachi::app {
namespace {

// docs/spec-core.md §7: ウィンドウは単一。起動時の大きさのみ決めておく。
constexpr int initialWindowWidth = 960;
constexpr int initialWindowHeight = 720;

} // namespace

MainWindow::MainWindow(const core::CapabilityTable& caps, QWidget* parent)
    : QMainWindow(parent), caps_(&caps), model_(new JobTableModel(this)),
      table_(new QTableView(this)), settings_(new SettingsPanel(caps, this)),
      outputDirectory_(new QLineEdit(this)), browse_(new QPushButton(this)),
      start_(new QPushButton(this)), cancel_(new QPushButton(this)),
      progress_(new QProgressBar(this)), status_(new QLabel(this)),
      bridge_(new io::JobRunnerBridge(caps, this)) {
    setWindowTitle(QCoreApplication::applicationName());
    resize(initialWindowWidth, initialWindowHeight);
    setAcceptDrops(true);

    table_->setModel(model_);
    // docs/spec-core.md §7: 自動スクロール禁止。完了しても表が勝手に動かない。
    table_->setAutoScroll(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);

    browse_->setText(tr("出力先を選ぶ(&B)..."));
    start_->setText(tr("開始(&S)"));
    cancel_->setText(tr("キャンセル(&C)"));
    start_->setObjectName(QStringLiteral("startButton"));
    cancel_->setObjectName(QStringLiteral("cancelButton"));
    browse_->setObjectName(QStringLiteral("browseButton"));

    outputDirectory_->setPlaceholderText(tr("出力先フォルダ"));
    progress_->setRange(0, 1);
    progress_->setValue(0);
    // アニメーションを避けるため、不定状態（往復するバー）は使わない。
    progress_->setTextVisible(true);

    setStatus(tr("画像またはフォルダをドラッグ&ドロップしてください。"));

    auto* central = new QWidget(this);
    auto* column = new QVBoxLayout(central);
    column->addWidget(table_, 1);
    column->addWidget(settings_);

    auto* destination = new QWidget(central);
    auto* destinationRow = new QHBoxLayout(destination);
    destinationRow->setContentsMargins(0, 0, 0, 0);
    destinationRow->addWidget(outputDirectory_, 1);
    destinationRow->addWidget(browse_);
    column->addWidget(destination);

    auto* controls = new QWidget(central);
    auto* controlRow = new QHBoxLayout(controls);
    controlRow->setContentsMargins(0, 0, 0, 0);
    controlRow->addWidget(start_);
    controlRow->addWidget(cancel_);
    controlRow->addWidget(progress_, 1);
    column->addWidget(controls);

    column->addWidget(status_);
    setCentralWidget(central);

    // docs/spec-core.md §7: タブ順を明示する。表 -> 設定 -> 出力先 -> 実行の順にたどる。
    setTabOrder(table_, settings_);
    setTabOrder(settings_, outputDirectory_);
    setTabOrder(outputDirectory_, browse_);
    setTabOrder(browse_, start_);
    setTabOrder(start_, cancel_);

    connect(browse_, &QPushButton::clicked, this, [this] {
        // ADR-0010 が認めるモーダルの 1 つ目。利用者が自ら起動する選択。
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("出力先フォルダを選ぶ"),
                                                                 outputDirectory_->text());
        if (!chosen.isEmpty()) {
            setOutputDirectory(chosen);
        }
    });
    connect(start_, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(cancel_, &QPushButton::clicked, this, &MainWindow::cancelConversion);

    connect(bridge_, &io::JobRunnerBridge::progressChanged, this, [this](int done, int total) {
        progress_->setRange(0, total > 0 ? total : 1);
        progress_->setValue(done);
    });
    connect(bridge_, &io::JobRunnerBridge::resultsReady, this,
            [this](const QList<io::JobOutcome>& outcomes) { model_->applyOutcomes(outcomes); });
    connect(bridge_, &io::JobRunnerBridge::finished, this, &MainWindow::handleFinished);

    updateButtons();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    addSources(event->mimeData()->urls());
    event->acceptProposedAction();
}

bool MainWindow::isReadableImage(const QString& path) const {
    // 拡張子の判定は能力表から。**フォーマット名を書かない**（不変条件 INV3B）。
    const core::FormatId format = core::formatIdFromString(QFileInfo(path).suffix());
    const std::optional<core::FormatCapability> capability = caps_->find(format);

    return capability.has_value() && capability->canDecode;
}

void MainWindow::collectFrom(const QString& path, QList<QString>& into) const {
    const QFileInfo info(path);

    if (info.isDir()) {
        // フォルダは再帰的にたどる。
        QDirIterator walker(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (walker.hasNext()) {
            const QString found = walker.next();
            if (isReadableImage(found)) {
                into.append(found);
            }
        }
        return;
    }

    if (info.isFile() && isReadableImage(path)) {
        into.append(path);
    }
}

void MainWindow::addSources(const QList<QUrl>& urls) {
    if (isConverting()) {
        // 実行中に一覧を変えない。表と結果の対応が崩れる。
        return;
    }

    QList<QString> found;
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        collectFrom(url.toLocalFile(), found);
    }

    // 重複は入れない。同じ入力が 2 行あると、結果の行が一意に決まらない。
    QSet<QString> known(sources_.begin(), sources_.end());
    for (const QString& path : found) {
        if (!known.contains(path)) {
            known.insert(path);
            sources_.append(path);
        }
    }

    model_->setJobs(sources_);
    setStatus(tr("%1 件を読み込みました。").arg(sources_.size()));
    updateButtons();
}

void MainWindow::setOutputDirectory(const QString& path) { outputDirectory_->setText(path); }

void MainWindow::startConversion() {
    if (isConverting()) {
        return;
    }
    if (sources_.isEmpty()) {
        setStatus(tr("変換するファイルがありません。"));
        return;
    }

    const QString destination = outputDirectory_->text();
    if (destination.isEmpty() || !QFileInfo(destination).isDir()) {
        // **モーダルは出さない。**エラーはステータス行に出す（docs/spec-core.md §7）。
        setStatus(tr("出力先フォルダを選んでください。"));
        return;
    }

    const io::CollisionPolicy collision = settings_->collisionPolicy();
    if (collision == io::CollisionPolicy::Overwrite) {
        // ADR-0010 が認めるモーダルの 2 つ目。破壊的操作の確認。
        const auto answer = QMessageBox::question(
            this, tr("上書きの確認"),
            tr("同名のファイルを上書きします。元のファイルは戻せません。続けますか。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            setStatus(tr("上書きを取りやめました。"));
            return;
        }
    }

    io::BatchRequest request;
    request.sourcePaths = sources_;
    request.outputDirectory = io::OutputDirectory{destination};
    request.pattern = settings_->namePattern();
    request.extension = settings_->extension();
    request.spec = settings_->spec();
    request.collision = collision;

    model_->setJobs(sources_);
    progress_->setRange(0, static_cast<int>(sources_.size()));
    progress_->setValue(0);
    setStatus(tr("変換しています..."));

    bridge_->start(request);
    updateButtons();
}

void MainWindow::cancelConversion() {
    if (!isConverting()) {
        return;
    }
    bridge_->cancel();
    setStatus(tr("キャンセルしています..."));
}

void MainWindow::handleFinished() {
    setStatus(tr("完了しました。成功 %1 件 / スキップ %2 件 / 失敗 %3 件")
                  .arg(model_->succeededCount())
                  .arg(model_->skippedCount())
                  .arg(model_->failedCount()));
    updateButtons();
}

void MainWindow::updateButtons() {
    const bool running = isConverting();
    start_->setEnabled(!running && !sources_.isEmpty());
    cancel_->setEnabled(running);
    settings_->setEnabled(!running);
    browse_->setEnabled(!running);
}

int MainWindow::jobCount() const { return model_->rowCount(); }

int MainWindow::succeededCount() const { return model_->succeededCount(); }

int MainWindow::failedCount() const { return model_->failedCount(); }

QString MainWindow::statusText() const { return status_->text(); }

bool MainWindow::isConverting() const { return bridge_->isRunning(); }

void MainWindow::setStatus(const QString& text) { status_->setText(text); }

} // namespace katachi::app
