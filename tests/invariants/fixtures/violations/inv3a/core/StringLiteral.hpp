// INV3A の検出確認用フィクスチャ。故意に違反している。ビルド対象ではない。
#pragma once

#include <QString>

inline QString badLabel() { return QStringLiteral("core layer must not contain string literals"); }
