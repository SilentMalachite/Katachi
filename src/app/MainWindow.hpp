#pragma once

// 単一のメインウィンドウ（ADR-0010）。
//
// 縦 3 段。上からジョブ一覧 / 設定 / 実行と進捗。
// **フローティングパネルを作らない。ウィンドウは 1 枚**（docs/spec-core.md §7）。
//
// モーダルを出してよいのは 2 つだけ（ADR-0010）。
//   1. 出力先フォルダの選択
//   2. CollisionPolicy::Overwrite で実行を開始するときの確認
// **エラーはステータス行と結果一覧の理由列に出す。**

#include "app/JobTableModel.hpp"
#include "app/SettingsPanel.hpp"
#include "core/CapabilityTable.hpp"
#include "io/JobRunnerBridge.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QTableView>
#include <QUrl>
#include <QWidget>

namespace katachi::app {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const core::CapabilityTable& caps, QWidget* parent = nullptr);

    // D&D と同じ経路。フォルダは再帰的にたどり、能力表が読める拡張子だけを拾う。
    void addSources(const QList<QUrl>& urls);

    void setOutputDirectory(const QString& path);

    void startConversion();
    void cancelConversion();

    [[nodiscard]] int jobCount() const;
    [[nodiscard]] int succeededCount() const;
    [[nodiscard]] int failedCount() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] bool isConverting() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    // QStringList と綴らないのは clang-tidy の misc-include-cleaner による
    // （Qt 6 の QStringList は QList<QString> の別名。T1.5 と同じ事情）。
    void collectFrom(const QString& path, QList<QString>& into) const;
    [[nodiscard]] bool isReadableImage(const QString& path) const;
    void setStatus(const QString& text);
    void updateButtons();
    void handleFinished();

    const core::CapabilityTable* caps_;

    JobTableModel* model_;
    QTableView* table_;
    SettingsPanel* settings_;
    QLineEdit* outputDirectory_;
    QPushButton* browse_;
    QPushButton* start_;
    QPushButton* cancel_;
    QProgressBar* progress_;
    QLabel* status_;
    io::JobRunnerBridge* bridge_;

    QList<QString> sources_;
};

} // namespace katachi::app
