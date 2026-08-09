#pragma once

// UI1 の違反フィクスチャ。スキャナが空振りしていないことを確認するために置く。
// docs/spec-core.md §7: アニメーション・フェード・スライドを一切使わない。
#include <QPropertyAnimation>

class Animated {
    QPropertyAnimation* fade = nullptr;
};
