# C++20 規約

---

## 1. 基本

**必須**

- `[[nodiscard]]` を、値を返す全ての関数に付ける
- `enum class` のみ。素の `enum` 禁止
- `std::span` / `std::string_view` / `QStringView` で非所有ビューを渡す
- 所有は `std::unique_ptr`。Qt の親子オーナーシップは `app/` 層でのみ使う
- 初期化は必ず行う。未初期化メンバを作らない
- コアの関数は `noexcept`。エラーは `Result` で返す（確保失敗については ADR-0002）
- **テンプレート引数は必ず制約する**（§2）

**禁止**

- `new` / `delete` の直書き（Qt の親付き `new` は `app/` 層のみ許可）
- C 形式キャスト、`const_cast`、`reinterpret_cast`
- マクロによるロジック（`Q_OBJECT` 等は除く）
- シングルトン、グローバル可変状態、`static` な可変変数
- **無制約テンプレート**（`template <typename T>` を制約なしで書く）。例外は concept 定義そのもの（`template <typename T> concept X = ...`）のみ
- **`std::enable_if` / SFINAE / `std::void_t`**。制約は `requires` 節と concept で書く
- **素の `auto` 仮引数**（`void f(auto x)`）。制約付き（`CapabilitySource auto`）のみ許可
- コア層での例外の送出 / ファイルアクセス / 時刻取得 / ネットワーク
- Qt5 由来の非推奨 API（`QRegExp`、`QLinkedList`、旧 `SIGNAL/SLOT` マクロ接続）。接続は必ず関数ポインタ形式
- Qt Quick / QML の混入
- **アプリ実行時の**ネットワーク通信全般（`QNetworkAccessManager` を include した時点で違反）。
  ビルド時の依存取得（`FetchContent` 等）はこの禁止の対象外（`phases.md` §1.5）

---

## 2. concept と型制約

C++20 の concept は本設計と相性が良い。ただし**無条件に増やさない**。

### 2.1 定義してよい条件（すべて満たすとき）

1. その位置に、本番実装とテストダブルなど **2 つ以上の型**が入る必然性がある
2. 実行時多態（仮想関数）を使う理由がない
3. 要求する操作を、構文として書き下せる

**定義しない場合**

- 実装が 1 つしかない → 具象型で書く。「将来差し替えるかもしれない」は理由にならない
- 差し替えたいのが「型」ではなく「中身の値」 → ファクトリ関数を足せば済む
- 表明したいのが純粋性・決定性・スレッド安全性 → §2.5

### 2.2 定義する concept

**concept は層ごとにファイルを分ける。** `ByteSource` などは `IoError` を参照するため、
`src/core/` に置くと依存方向（`core → io → app`）が逆流する。

#### `src/core/Concepts.hpp`（core 層のみを参照）

```cpp
// --- Result の型引数 ---
template <typename T>
concept ResultValue = std::destructible<T>
                   && std::is_nothrow_move_constructible_v<T>;  // ok() が noexcept のため

template <typename E>
concept ResultError = std::destructible<E>
                   && std::copy_constructible<E>
                   && std::equality_comparable<E>;   // テストで比較するため

// --- 能力表の抽象 ---
template <typename T>
concept CapabilitySource = requires(const T& t, FormatId id) {
    { t.find(id) }    -> std::same_as<std::optional<FormatCapability>>;
    { t.encodable() } -> std::same_as<std::vector<FormatCapability>>;
};
```

#### `src/io/IoConcepts.hpp`（Phase 2。core を参照してよい）

```cpp
// IoError は src/io/IoError.hpp で定義する（core には置かない）
template <typename T>
concept ByteSource = requires(T& t) {
    { t.read() } -> std::same_as<Result<QByteArray, IoError>>;
};

template <typename T>
concept ByteSink = requires(T& t, const QByteArray& bytes) {
    { t.write(bytes) } -> std::same_as<Result<std::monostate, IoError>>;
};

// --- 進捗とキャンセル（Qt に依存せず JobRunner をテストするため） ---
template <typename T>
concept ProgressSink = requires(T& t, int done, int total) {
    { t.onProgress(done, total) };                    // noexcept を要求しない（§2.4）
    { t.isCancelled() } noexcept -> std::same_as<bool>;
};
```

### 2.3 テンプレート化する箇所・しない箇所（決定済み）

| 対象 | 判断 | 理由 |
|---|---|---|
| `convert()` | **しない**。引数は具象 `const CapabilityTable&` | 差し替えたいのは能力表の**中身**であって型ではない。`fromCapabilities()` で足りる。ヘッダ実装化を避け、心臓部を 1 箇所に閉じる |
| `Result<T,E>` | する。`ResultValue` / `ResultError` で制約 | 型引数が実際に多様 |
| `JobRunner` | **する**。`ByteSink` / `ProgressSink` で制約 | テストでファイルシステムと Qt のイベントループを避けたい。そこには型として別物が入る |
| `NamingRule` | しない | 入力は `QString` 固定 |

`CapabilitySource` は定義するが、用途は `static_assert(CapabilitySource<CapabilityTable>)` による**契約の明文化**に留める。

### 2.4 Qt と併用するときの落とし穴

- **`Q_OBJECT` を持つクラスはテンプレートにできない**（moc がテンプレートを処理しない）。
  `JobRunner<Sink, Progress>` はヘッダのみの純粋テンプレート（`src/io/JobRunner.hpp`）とし、
  シグナル発行は**非テンプレートの薄い `QObject` アダプタ** `src/io/JobRunnerBridge.hpp/.cpp` へ分離する。
  **テンプレートクラスの実装を `.cpp` に置かない。**
- **`ProgressSink::onProgress` に `noexcept` を要求しない。** 本番の `JobRunnerBridge` はここで Qt の
  シグナルを emit するが、`emit` は接続先スロットを同期呼び出しするため `noexcept` にできない。
  要求すると本番型が concept を満たせなくなる。`isCancelled()` は単なるフラグ読み出しなので `noexcept` を課す
- 返り値制約に `std::same_as<...>` を使うと、Qt の暗黙変換（`QString` ⇄ `QStringView` 等）を弾く。意図した弾き方かを確認する。緩めるなら `std::convertible_to` を使い、**緩めた理由をコメントに残す**
- Qt コンテナを `std::ranges` の制約で受ける場合、range 要件だけでなく `std::same_as<std::ranges::range_value_t<R>, QString>` のように要素型も制約する

### 2.5 concept で表現**できない**もの

concept は構文の制約であって、意味の制約ではない。以下はテストで担保する。

| 性質 | 担保する手段 |
|---|---|
| `convert()` の純粋性 | 依存方向テスト + 不変条件テスト |
| 同一入力 → 同一バイト列 | 決定性テスト |
| スレッド安全性 | ASan / UBSan + コードレビュー |
| `isOk()` のときだけ `value()` を呼ぶ | 実行時アサート（契約） |

**「concept を付けたので安全です」と報告に書かない。**

### 2.6 書き方

- 短い制約は `template <CapabilitySource Caps>`、複合条件は `requires` 節に分ける
- concept 名は名詞または形容詞。`~Concept` という接尾辞を付けない
- 定義は**層ごとに 1 ファイル**（`src/core/Concepts.hpp` / `src/io/IoConcepts.hpp`）。各ヘッダに散らさない。層をまたぐ concept を作らない
- 1 つの concept が要求する操作は原則 4 個以下。超えるなら分割を検討する

---

## 3. API の確認

- Qt の API は、**公式ドキュメントで存在を確認できたものだけ**使う
- Qt5 と Qt6 で挙動が変わった API（`QImage::pixel()` 周辺、`QTextCodec` 廃止、`QVariant` の比較等）に注意する
- 確信が持てない API に遭遇したら、**推測で書かず、その場で停止して報告する**
- 調査はサブエージェントに投げてよいが、結果には根拠 URL を付けさせる（`agent-protocol.md` §3）
