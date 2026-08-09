# コア仕様

型・能力表・アルファ処理・命名規則・並行実行・UI 非機能要件。
C++ の書き方は `docs/cpp-conventions.md`、Phase 分割と受け入れ基準は `docs/phases.md`。

---

## 1. ディレクトリ構成

```
katachi/
├── CMakeLists.txt / CMakePresets.json / .clang-format / .clang-tidy
├── cmake/QualityGates.cmake
├── cmake/ExtraCodecs.cmake / CollectExtraCodecs.cmake  # 追加コーデック（ADR-0013）。既定 OFF
├── src/
│   ├── core/          # 依存: Qt6::Core, Qt6::Gui のみ（Widgets 禁止）
│   │   ├── Concepts.hpp            # core 層の concept のみ
│   │   ├── Result.hpp / ConvertError.hpp
│   │   ├── FormatId.hpp            # 強い型付き文字列。文字列リテラル例外はここだけ
│   │   ├── CapabilityTable.hpp/.cpp
│   │   ├── ConversionSpec.hpp
│   │   ├── Converter.hpp/.cpp      # 純粋関数（非テンプレート）
│   │   └── NamingRule.hpp/.cpp     # 純粋関数
│   ├── io/            # 副作用の境界。core に依存してよいが逆は不可
│   │   │                # Qt6::Widgets はリンクしない（ワーカー側の層のため）
│   │   ├── IoError.hpp             # core には置かない
│   │   ├── IoConcepts.hpp          # ByteSource / ByteSink / ProgressSink
│   │   ├── FileSource.hpp/.cpp / FileSink.hpp/.cpp
│   │   ├── CollisionPolicy.hpp/.cpp  # Overwrite / Skip / Rename。既定 Skip（ADR-0009）
│   │   ├── MemoryBudget.hpp/.cpp     # バッチ実行時のメモリ上限（ADR-0008）
│   │   ├── JobRunner.hpp           # テンプレート。ヘッダのみ（.cpp を作らない）
│   │   └── JobRunnerBridge.hpp/.cpp  # 非テンプレートの QObject アダプタ
│   └── app/           # 依存: Qt6::Widgets
│       ├── MainWindow / JobTableModel / SettingsPanel / main.cpp
├── tests/{core,io,app,fixtures}/    # app のみ QApplication が要るため実行ファイルを分ける
├── tools/format_matrix.cpp          # docs/format-matrix.md の生成器（ビルド時に実行）
├── CLAUDE.md                       # リポジトリ直下（常時読み込み）
├── docs/                           # 参照文書は全てここ
│   ├── spec-core.md / cpp-conventions.md / phases.md / agent-protocol.md
│   ├── licenses.md / format-matrix.md
│   └── progress/ + adr/
└── .github/workflows/ci.yml
```

**依存方向は一方向 `core → io → app`。逆向きの `#include` はテストで検出して落とす。**

---

## 2. コア型

```cpp
// Result.hpp — 型引数は必ず制約する（cpp-conventions.md §2）
template <ResultValue T, ResultError E>
    requires (!std::same_as<T, E>)   // variant<T,E> の曖昧さを型で排除
class Result {
public:
    static Result ok(T value) noexcept;
    static Result err(E error) noexcept;
    [[nodiscard]] bool isOk() const noexcept;
    [[nodiscard]] const T& value() const&;   // !isOk() での呼び出しは契約違反
    [[nodiscard]] const E& error() const&;
private:
    std::variant<T, E> data_;
};
```

> `!std::same_as<T,E>` は飾りではない。`Result<QString, QString>` は構築が曖昧になり、
> 成功と失敗を取り違える。型で塞ぐ。

### 2.1 `FormatId`（決定済み）

```cpp
// 強い型付き文字列。能力表内のインデックスにはしない。
// 理由: 能力表は実行時生成のため、インデックスは生成順に依存する。
//       fromCapabilities() のテストダブルと本番で、同じ値が別の形式を指しうる。
//       無効値の排除は find() が std::optional を返すことで担保する。
struct FormatId {
    QString v;
    friend bool operator==(const FormatId&, const FormatId&) = default;
};
```

`QString` ⇄ `FormatId` の変換関数は `FormatId.hpp` にのみ置く。
**ここが「フォーマット名の文字列リテラル禁止」の唯一の例外である。**

`formatIdFromString()` は **①前後の空白除去 ②小文字化 ③別名の代表名への吸収**（`jpg` / `jfif` → `jpeg`、`tif` → `tiff`）を行う（**ADR-0006**）。
別名を畳む基準は「Qt が同一の MIME タイプを報告すること」で、`heic` / `heif` のように MIME が異なるものは畳まない。
**`CapabilityTable` も同じ関数で正規化する。** 表の鍵と問い合わせの鍵が必ず同じ経路を通るため、片側だけ畳まれて引けなくなることはない。
`buildFromQt()` は正規化後に同一 `FormatId` となった項目を 1 件へ統合し、`extensions` は和集合を取る。

```cpp
enum class ConvertError {
    EmptyInput, DecodeFailed, UnsupportedTarget, EncodeFailed,
    AlphaLossNotAllowed,   // アルファ画像を非対応形式へ Reject 指定で変換
    ImageTooLarge,         // 上限は ConversionSpec::maxPixels
};

// 変換は成功したが、指定どおりには処理できなかったことの通知（ADR-0004）。
// エラーではないため Result の E ではなく、成功値の側に載せる。
enum class ConvertWarning {
    AlphaFlattenedFallback,   // §4 3 行目: Preserve 指定だが出力形式が非対応のため Flatten した
};
```

```cpp
// 値型。メンバは非 const（集成体初期化とコピー代入を壊さないため）だが、
// 「生成後に書き換えない」ことを規約とする。受け渡しは常に const 参照。
enum class AlphaPolicy    { Preserve, Flatten, Reject };
// PreserveSupported は「Qt が扱える範囲」＝ 向き / テキスト / ICC を保持する。
// EXIF 全体の保持は Qt 単体では不可能（ADR-0003）。
enum class MetadataPolicy { PreserveSupported, StripAll };
enum class IccPolicy      { Embed, Strip };

struct ConversionSpec {
    FormatId             target;                        // 文字列でなく型で持つ（§2.1）
    int                  quality      = 90;             // 0..100
    std::optional<QSize> resize       = std::nullopt;   // アスペクト比は常に保持
    AlphaPolicy          alpha        = AlphaPolicy::Preserve;
    QColor               flattenColor = Qt::white;
    MetadataPolicy       metadata     = MetadataPolicy::PreserveSupported;
    IccPolicy            icc          = IccPolicy::Embed;
    qint64               maxPixels    = 268'435'456;    // 16384 x 16384
};
```

```cpp
// Converter.hpp — 本アプリの心臓部。
// 純粋関数：ファイルシステム・時刻・グローバル状態・乱数に触れない。
// 同一入力に対して常に同一出力（バイト列）を返す。

// 成功値。警告はエラーではないため E 側ではなくここに載せる（ADR-0004）。
// 両メンバとも nothrow move 構築可能なので ResultValue を満たす。
struct ConversionOutput {
    QByteArray                  bytes;
    std::vector<ConvertWarning> warnings;
};

[[nodiscard]] Result<ConversionOutput, ConvertError>
convert(const QByteArray& source,
        const ConversionSpec& spec,
        const CapabilityTable& caps) noexcept;
```

> **`noexcept` と確保失敗（ADR-0002 に記録すること）**
> 「コアの関数は `noexcept`」という規約と、「確保は `std::bad_alloc` を投げうる」ことは
> そのままでは両立しない。**コア層全体の方針として次を採る。**
>
> - コアの関数は `noexcept` を維持する
> - 確保失敗時に `std::terminate` することを**意図的に受け入れる**。変換中の確保失敗は回復不能であり、
>   壊れた出力を書くより即座に落ちる方が安全
> - `maxPixels` の事前チェックで、現実的な入力では到達しないようにする
>
> この方針は `convert()` だけでなく、`CapabilityTable::find()` など**確保を伴う全ての `noexcept` なコア関数**に適用する。
> **暗黙に変更しない。**変えるなら ADR を書き直す。

---

## 3. 能力表（CapabilityTable）

```cpp
struct FormatCapability {
    FormatId id; QStringList extensions;
    bool canDecode, canEncode, supportsAlpha, supportsQuality, isLossless;
};

class CapabilityTable {
public:
    // QImageReader/QImageWriter::supportedImageFormats() から実行時に構築。
    // static でもシングルトンでもなく、明示的に生成して注入する。
    static CapabilityTable buildFromQt();
    // テスト用。任意の能力表を組み立てる。これがあるため convert() を
    // テンプレート化する必要はない（cpp-conventions.md §2.3）。
    static CapabilityTable fromCapabilities(std::vector<FormatCapability>);

    [[nodiscard]] std::optional<FormatCapability> find(FormatId) const noexcept;
    [[nodiscard]] std::vector<FormatCapability> encodable() const;
};

static_assert(CapabilitySource<CapabilityTable>);   // 契約の明文化
```

**不変条件（テストで強制）**

- `src/core/` `src/app/` にフォーマット名の文字列リテラルを書かない。すべて `CapabilityTable` 経由
- **唯一の例外は `src/core/FormatId.hpp` 内の変換関数**（`QString` ⇄ `FormatId`）とテストコード。スキャナはこの 2 箇所だけを除外する
- **禁止しているのは「フォーマット名の」文字列リテラルであって、文字列リテラル一般ではない。**
  `src/core/NamingRule.cpp` の `{name}` / `{index}` / `{ext}` のような、フォーマット名でない
  予約語のリテラルは許される（不変条件スキャナ INV3A / INV3B はフォーマット名の一覧と照合する）
- CI にハードコードスキャナのテストを置く

---

## 4. アルファ処理（取りこぼしやすい）

| 入力 | 出力形式 | AlphaPolicy | 挙動 |
|---|---|---|---|
| アルファあり | 対応 | 任意 | そのまま保持 |
| アルファあり | 非対応 | `Preserve` | **`Flatten` にフォールバックし、`ConversionOutput::warnings` に `ConvertWarning::AlphaFlattenedFallback` を 1 件積む**（ADR-0004） |
| アルファあり | 非対応 | `Flatten` | `flattenColor` で合成 |
| アルファあり | 非対応 | `Reject` | `ConvertError::AlphaLossNotAllowed` |
| アルファなし | 任意 | 任意 | そのまま |

合成は**プリマルチプライド前提で行わない**。`QImage::Format_ARGB32` に正規化してから、指定色の上に `CompositionMode_SourceOver` で描く。

**この表は全行テストする。**

---

## 5. 命名規則（純粋関数）

```cpp
enum class NamingError {
    EmptyPattern,        // pattern が空
    UnknownPlaceholder,  // {} 内が既知の名前でない
    InvalidIndexSpec,    // {index:...} の桁指定が不正
    EmptyResult,         // 展開結果が空になった
};

// 書式と拡張子は強い型で分ける。どちらも中身は QString なので、素の引数で並べると
// 呼び出し側が取り違えられる。FormatId と同じ「強い型付き文字列」の考え方（§2.1）で塞ぐ。
struct NamePattern   { QString v; };
struct NameExtension { QString v; };

// 差し込める名前は {name} / {index} / {ext} の 3 つ。
// {index} は :N を付けて最小桁数を指定できる（0 詰め、上限 32）。
// 例: resolveOutputName("photo", 1, {"{name}_{index:03}.{ext}"}, {"png"}) → "photo_001.png"
[[nodiscard]] Result<QString, NamingError>
resolveOutputName(const QString& sourceBaseName, int index,
                  const NamePattern& pattern, const NameExtension& extension) noexcept;
```

衝突ポリシー: `Overwrite` / `Skip` / `Rename`（`_1`, `_2` を付与）。
既定は **`Skip`**（破壊的操作を既定にしない）。

> **衝突ポリシーの適用は core に置かない（ADR-0005）。**
> 衝突判定には出力先の実在確認、すなわちファイルシステム参照が要る。
> core ではファイルアクセスが禁止されているため実装できない。
> `resolveOutputName()` は**名前の生成のみ**を担い、衝突の解決は
> **Phase 2 の `src/io` 層**で行う。

**適用する場所と、スキップの表し方（ADR-0009）。**

- `CollisionPolicy` の列挙は `src/io/CollisionPolicy.hpp` に置く。**core には置かない**
- 適用するのは `FileSink::write()`、すなわち**ワーカースレッドで、そのファイルを書く直前**。
  main thread が事前に全件の実在確認をしない（1000 件で UI が止まるため）
- `Skip` で書かなかったことは `IoError::DestinationExists` として返す。
  `ByteSink` の戻り値型（`cpp-conventions.md` §2.2）を変えないため。
  **app 層は「スキップ（既存）」と表示し、失敗件数に数えない**
- `Rename` は `_1` から順に試し、上限 10000 で打ち切って `IoError::WriteFailed` を返す
- `Overwrite` を選んだ状態での実行開始時のみ、確認のモーダルを出す（§7 が認める唯一の例外）

---

## 6. 並行実行（Phase 2）

- `QThreadPool` + `QtConcurrent::mapped`。既定並列度は `QThread::idealThreadCount() - 1`（最低 1）
- キャンセルは `QFuture::cancel()`
- **ワーカースレッドから `QWidget` に触れない。** UI 更新は必ずキュー接続シグナル経由
- コアが純粋関数なので `convert()` はロック不要でスレッドセーフ。**この性質を壊す変更（キャッシュ、静的変数の導入等）は禁止。**「性能のため」も理由にならない。必要なら停止して相談する
- `JobRunner<Sink, Progress>` はヘッダのみのテンプレート、シグナル発行は `JobRunnerBridge`（非テンプレート `QObject`）に分離する。理由は `cpp-conventions.md` §2.4

---

## 7. UI 非機能要件

**これらは好みではなく、視覚・前庭系の負荷を避けるための合理的要件。**
**実装時に落とさない。「モダンな UI にする」等の理由で覆さない。**

- アニメーション・フェード・スライドを一切使わない
- 進捗表示は 200ms 以下の間隔で更新しない（ちらつき防止）
- ダークモードはシステム設定に追随。独自テーマを作らない
- 全機能がキーボードのみで操作可能。タブ順を明示的に設定する
- 自動スクロール禁止。ジョブ完了時にリストが勝手に動かない
- モーダルダイアログは「破壊的操作の確認」のみ。エラーはステータス行と結果列に表示
- ウィンドウは単一。フローティングパネルを作らない

---

## 8. スコープ外（明示的な非スコープ）

- 画像編集（トリミング、補正、レタッチ）
- クラウド送信・テレメトリ・自動アップデータ（**ネットワーク通信は一切行わない**）
- 動画・アニメーション GIF の各フレーム編集（1 フレーム目のみ。多フレームは Phase 3 以降の検討事項）
- プラグイン機構の外部公開
