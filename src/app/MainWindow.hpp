#pragma once

#include <QMainWindow>

namespace katachi::app {

/// Phase 0 では空のウィンドウのみを提供する。
/// D&D・ジョブ一覧・設定パネル・進捗・キャンセルは Phase 2 で追加する。
///
/// docs/spec-core.md §7: アニメーション / フェード / スライドを一切使わない。
/// ウィンドウは単一。フローティングパネルを作らない。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace katachi::app
