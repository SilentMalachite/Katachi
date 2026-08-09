#pragma once

// UI3 の違反フィクスチャ。
// docs/spec-core.md §7: ウィンドウは単一。フローティングパネルを作らない。
#include <QDockWidget>

class Floating {
    QDockWidget* panel = nullptr;
};
