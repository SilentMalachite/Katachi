#pragma once

// UI2 の違反フィクスチャ。
// docs/spec-core.md §7: ダークモードはシステム設定に追随。独自テーマを作らない。
#include <QWidget>

inline void paintIt(QWidget* widget) {
    widget->setStyleSheet(QStringLiteral("background: black;"));
}
