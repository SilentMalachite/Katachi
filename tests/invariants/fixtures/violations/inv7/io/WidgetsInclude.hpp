#pragma once

// INV7 の違反フィクスチャ。スキャナが空振りしていないことを確認するために置く。
// io はワーカースレッドで動く層であり、QWidget に触れてはならない（ADR-0010）。
#include <QtWidgets/QWidget>

class WidgetsInclude {};
