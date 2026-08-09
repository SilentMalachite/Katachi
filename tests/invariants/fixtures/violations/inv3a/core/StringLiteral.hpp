// INV3A の検出確認用フィクスチャ。故意に違反している。ビルド対象ではない。
#pragma once

#include <QString>

// フォーマット名のリテラルは core では禁止（FormatId.hpp を除く）。
inline QString badFilter() { return QStringLiteral("Images (*.png)"); }
