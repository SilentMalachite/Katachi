#pragma once

// 出力先の衝突ポリシー（docs/spec-core.md §5 / ADR-0009）。
//
// **core には置かない。** 衝突判定には出力先の実在確認、すなわちファイルシステム参照が要り、
// core では禁止されている（ADR-0005）。
//
// 適用するのは FileSink::write()、すなわちワーカースレッドで、そのファイルを書く直前。
// main thread が事前に全件の実在確認をしない（1000 件で UI が止まるため）。

#include "core/Result.hpp"
#include "io/IoError.hpp"

#include <QSet>
#include <QString>

#include <cstdint>

namespace katachi::io {

// 基底型を明示するのは clang-tidy の performance-enum-size による（Phase 1 T2 の知見）。
enum class CollisionPolicy : std::uint8_t {
    Overwrite,
    Skip,
    Rename,
};

// **既定は Skip。破壊的操作を既定にしない**（ADR-0005 の明示的な指示）。
// 既定値を確認せずに Overwrite にしないこと。
inline constexpr CollisionPolicy defaultCollisionPolicy = CollisionPolicy::Skip;

// 改名の上限。無ければ名前が埋まった状況で無限ループする（ADR-0009）。
inline constexpr int defaultMaxRenameAttempts = 10000;

// 出力先とファイル名を強い型で分ける。どちらも中身は QString なので、素の引数で並べると
// 呼び出し側が取り違えられる（clang-tidy: bugprone-easily-swappable-parameters）。
// FormatId / NamePattern と同じ「強い型付き文字列」の考え方（docs/spec-core.md §2.1）。
// **取り違えると、意図しない場所へファイルを書く。**
struct OutputDirectory {
    QString v;
};

struct OutputFileName {
    QString v;
};

// 実際に書くパスを決める。ファイルシステムを見る。
//
//   Overwrite: 希望どおりのパスを返す
//   Skip     : 既にあれば IoError::DestinationExists（失敗ではない。app 層は
//              「スキップ（既存）」と表示し、失敗件数に数えない — ADR-0009）
//   Rename   : _1 から順に空きを探し、上限で打ち切って IoError::WriteFailed
//
// reserved は「このバッチで既に予約済みの出力パス」。**実在するものと同じに扱う**
// （ADR-0009 の追補）。並列実行では、まだ commit されていない出力を実在確認では
// 見つけられないため、これが無いと 2 つのワーカーが同じ名前を選ぶ。
// 呼び出し側（JobRunnerBridge）がロックの中で渡し、得たパスを予約集合へ入れる。
//
// maxRenameAttempts を引数で受けるのは、上限に達する経路をテストで通すため。
// 既定の 10000 個のファイルを作るテストは CI で重すぎる（FileSource の上限と同じ考え方）。
[[nodiscard]] core::Result<QString, IoError>
resolveCollision(const OutputDirectory& directory, const OutputFileName& fileName,
                 CollisionPolicy policy, const QSet<QString>& reserved = {},
                 int maxRenameAttempts = defaultMaxRenameAttempts);

} // namespace katachi::io
