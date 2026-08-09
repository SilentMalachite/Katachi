# Phase 2 — GUI

**追記のみ。既存の記述は書き換えない。** 誤りを見つけた場合も遡って修正せず、新しい日付で訂正を追記する。

---

## 2026-08-09 — Phase 2 着手前の計画（承認待ち。コードは 1 行も書いていない）

### 実施内容

Phase 1 のクローズを受け、Phase 2（GUI）の計画を立てた。
`CLAUDE.md` セッション手順 2 に従い、**承認を得るまで実装へ進まない。**
ブランチもまだ作っていない（承認後に `phase2` を作る）。

### 読み込んだファイル

`CLAUDE.md` / `docs/phases.md` / `docs/spec-core.md` / `docs/cpp-conventions.md` /
`docs/agent-protocol.md` / `docs/progress/phase1.md`（全 1550 行） / `docs/adr/0005-naming-responsibility.md` /
`src/core/Converter.hpp` / `src/core/NamingRule.hpp` / `src/app/MainWindow.hpp` / `.cpp` / `src/app/main.cpp` /
`CMakeLists.txt` / `src/core/CMakeLists.txt` / `src/app/CMakeLists.txt` / `tests/CMakeLists.txt` /
`tests/main.cpp` / `tests/invariants/scan_invariants.cmake` / `cmake/QualityGates.cmake` /
`.clang-tidy` / `CMakePresets.json` / `.github/workflows/ci.yml`

---

### 着手前に確認した Qt API（根拠付き。推測で埋めない）

`cpp-conventions.md` §3 に従い、設計の前提になる API だけを先に確認した。

| 事項 | 確認結果 | 根拠 |
|---|---|---|
| `QFuture::cancel()` | 「キャンセルは**非同期**。すでに得られた結果は参照できるが、以後新しい結果は現れない。`QtConcurrent::run()` の future はキャンセルできないが `mappedReduced()` のものはできる」 | `doc.qt.io/qt-6.8/qfuture.html#cancel` |
| キャンセル後のシグナル | `QFutureWatcher` はキャンセル後 `progressValueChanged` / `progressRangeChanged` / `progressTextChanged` / `resultReadyAt` / `resultsReadyAt` を**配送しない** | `qfuturewatcher.html#cancel` |
| `QFutureWatcher::setFuture()` | **接続を済ませてから呼ぶ**（競合を避けるため、と明記） | `qfuturewatcher.html#setFuture` |
| `QtConcurrent::mapped(QThreadPool*, Sequence&&, MapFunctor&&)` | ローカル Qt 6.11.1 のヘッダに実在（`qtconcurrentmap.h:352`） | ローカルヘッダ実測 |
| `QMimeData::hasUrls()` / `urls()` | 実在。MIME タイプ `text/uri-list` に対応 | `qmimedata.html#hasUrls` |

**未確認として扱うもの（推測で進めない）**

1. **`QtConcurrent::mapped` の `QThreadPool*` 版が Qt 6.8 にも実在するか。**
   ローカルは 6.11.1 であり、6.8 のヘッダを手元に持っていない。**T6 の CI で確認する。**
2. **`Qt6::Concurrent` が CI の `qtbase` に含まれるか。** Phase 1 の `qtimageformats` の件があるため、
   「入っているはず」で進めない。**T6 で `find_package` に追加し、CI が通ることで確認する。**
3. **キャンセル後に `finished` シグナルが来るか。** `setFuture()` の説明では `canceled()` と `finished()` の
   両方が列挙されているが、`cancel()` の説明は「進捗と結果のシグナルを配送しない」としか書いていない。
   **T6 でテストとして実測し、来ない場合は別の復帰経路を設計して報告する。**
4. **実行中の項目がキャンセルで中断されるか。** 公式ドキュメントに明記が無い。
   **明記が無い以上、中断されない前提で設計する**（下記「キャンセルの二段構え」）。

---

### 受け入れ基準（`phases.md` §4 Phase 2）と、それを満たす手段

| # | 受け入れ基準 | 満たす手段 | 検証するタスク |
|---|---|---|---|
| 1 | ドラッグ&ドロップでファイル / フォルダを追加できる | `MainWindow::dropEvent` → `QMimeData::urls()`。フォルダは `QDirIterator` で再帰。対象拡張子は**能力表の `extensions` で判定**（リテラル禁止） | T9 |
| 2 | 変換中にキャンセルでき、キャンセル後にアプリが正常状態に戻る | `QFuture::cancel()`（待機中の項目）+ `ProgressSink::isCancelled()`（着手前の早期終了）の二段構え。復帰は `finished` を単一の出口にする | T6 / T9 |
| 3 | 1000 ファイルのバッチで UI が固まらない | 変換は必ずワーカースレッド。UI へはキュー接続シグナルのみ。進捗は main thread の 200ms タイマで間引く | T6 / T10 |
| 4 | `spec-core.md` §7 の UI 非機能要件を全て満たす | 下表 | T8 / T9 / T10 |
| 5 | ワーカースレッドから `QWidget` に触れていないことを確認済み | **`katachi_io` に `Qt6::Widgets` をリンクしない**（core と同じ構造的担保）+ 新スキャナ INV7（判断事項 4） | T1 / T10 |
| 6 | 失敗したジョブが結果一覧に理由付きで残る | `JobOutcome` に失敗理由を載せ、`JobTableModel` の「理由」列に出す。**行は消さない** | T7 / T9 |

**`spec-core.md` §7 の 7 項目**

| 要件 | 満たす手段 | 検証 |
|---|---|---|
| アニメーション・フェード・スライドを使わない | `QPropertyAnimation` / `QGraphicsEffect` を使わない。既定のウィジェットのみ | T10（`grep` で 0 件） |
| 進捗表示は 200ms 以下の間隔で更新しない | ワーカーは `std::atomic<int>` を増やすだけ。main thread の `QTimer(200ms)` が読み出して表示を更新する | T6（`interval() == 200` と、1000 件でシグナルが 1000 回飛ばないこと） |
| ダークモードはシステム設定に追随 | `setStyleSheet` / 独自パレットを**書かない**。Qt の既定に任せる | T10（`grep` で 0 件） |
| 全機能がキーボードのみで操作可能。タブ順を明示 | `setTabOrder()` を明示的に連結。ボタンにニーモニック | T9 |
| 自動スクロール禁止 | `QTableView::scrollTo()` / `scrollToBottom()` を呼ばない。`setAutoScroll(false)` | T7 / T9 |
| モーダルは破壊的操作の確認のみ | 上書き（`CollisionPolicy::Overwrite`）で実行するときのみ確認ダイアログ。エラーはステータス行と結果列 | T9（判断事項 2） |
| ウィンドウは単一。フローティングパネルを作らない | `QMainWindow` 1 枚。`QDockWidget` を使わない | T9 |

---

### 設計（層・スレッド境界）

```
[main thread]                              [worker threads (QThreadPool)]
 app/MainWindow                              io/JobRunner<Sink, Progress>
 app/JobTableModel   <--queued signal--        |-- io/FileSource   (QFile 読み出し)
 app/SettingsPanel                             |-- io/FileSink     (QSaveFile 書き出し + 衝突解決)
       |                                       |-- io/MemoryBudget (同時使用量の上限)
       v  直接呼び出し                          |-- core::convert() / resolveOutputName()
 io/JobRunnerBridge (QObject, 非テンプレート)
   - QtConcurrent::mapped(pool, jobs, fn) と QFutureWatcher を持つ
   - ProgressSink の本番実装（onProgress / isCancelled）
   - QTimer(200ms) で進捗をまとめて emit
```

**依存方向は `core → io → app`。`katachi_io` は `Qt6::Core` / `Qt6::Gui` / `katachi_core` のみをリンクし、
`Qt6::Widgets` はリンクしない。** ワーカーで動くコードは `QWidget` を参照できず、受け入れ基準 5 が
テキスト検査ではなくリンク構成で担保される（Phase 1 の `katachi_core` と同じ手口）。

**キャンセルの二段構え。** `QFuture::cancel()` は待機中の項目を止めるが、実行中の項目を止めるとは
公式ドキュメントに書かれていない。そこで `JobRunner::runOne()` は**着手直前に `progress.isCancelled()` を見て
早期終了する**。1 件分の変換が終わるまでは待つが、残り 999 件は着手しない。

**進捗の間引き。** ワーカーは `std::atomic<int>` を増やすだけで、シグナルを出さない。
main thread の `QTimer(200ms)` が値を読んで `progressChanged(done, total)` を emit する。
1000 件でも進捗シグナルは高々「所要時間 / 200ms」回で、`spec-core.md` §7 を構造的に満たす。

**ファイル構成（`spec-core.md` §1 の木に対する追加は判断事項 5 で申告する）**

| 層 | ファイル | 内容 |
|---|---|---|
| io | `IoError.hpp` | `IoError`（4〜5 値。下記 T1） |
| io | `IoConcepts.hpp` | `ByteSource` / `ByteSink` / `ProgressSink`（`cpp-conventions.md` §2.2 の定義どおり） |
| io | `FileSource.hpp/.cpp` | `QFile` 読み出し。上限バイト数を持つ |
| io | `FileSink.hpp/.cpp` | `QSaveFile` による原子的書き出し + 衝突ポリシーの適用 |
| io | `CollisionPolicy.hpp/.cpp` | **追加**（§1 の木に無い）。`Overwrite` / `Skip` / `Rename`、既定 `Skip` |
| io | `MemoryBudget.hpp/.cpp` | **追加**（§1 の木に無い）。`phases.md` §5.3 の上限（判断事項 1） |
| io | `JobRunner.hpp` | ヘッダのみのテンプレート。`.cpp` を作らない |
| io | `JobRunnerBridge.hpp/.cpp` | 非テンプレートの `QObject` アダプタ |
| app | `JobTableModel.hpp/.cpp` | `QAbstractTableModel`。列 = 入力 / 出力 / 状態 / 理由 |
| app | `SettingsPanel.hpp/.cpp` | `QFormLayout`。選択肢は**能力表から生成** |
| app | `MainWindow.hpp/.cpp` | D&D、ジョブ表、設定、進捗、開始 / キャンセル、ステータス行 |

**テストの実行ファイルを 2 つに分ける（追加の申告。判断事項 5）**

- `katachi_tests`（既存）: core と io。**`Qt6::Widgets` をリンクしない。** `QCoreApplication` で動く
- `katachi_app_tests`（新規）: app のみ。`QApplication` が要るため分ける

分けることで「io のテストが Widgets を引かない」ことが構成上で見える。1 つにまとめると
`katachi_io` が Widgets を引いていなくてもテストバイナリ経由で見えてしまい、担保が弱くなる。

---

### 停止して判断を仰ぐ事項（回答が無いと着手できない。停止条件 1・2・7）

| # | 事項 | 推奨案 | 代替案 |
|---|---|---|---|
| 1 | **`phases.md` §5.3 バッチ実行時のメモリ上限**（着手時に決めると明記されている） | **A: バイト予算セマフォ。** 総予算 **1 GiB** を `MemoryBudget` が管理する。1 件あたりの見積り = 入力ファイルサイズ + 4 × 幅 × 高さ（`QImageReader::size()` でヘッダのみ読む）+ 入力サイズ（出力見込み）。予算を超える単独ジョブは**予算全量を確保して 1 件ずつ**実行する。**1 GiB という値は `ConversionSpec::maxPixels` の既定 268,435,456 px を `ARGB32` で持ったときの 1 GiB と一致させたもの**で、「最大サイズの画像は必ず 1 枚ずつ処理される」という説明可能な性質になる | B: `maxPixels` の既定を 64 Mpx（256 MiB）へ下げ、並列度だけで抑える（実装は最小だが `spec-core.md` §2 の既定値変更）／ C: 見積りから並列度を動的に決める |
| 2 | **出力先フォルダの選択に `QFileDialog`（モーダル）を使ってよいか** | **使う。** §7 の「モーダルは破壊的操作の確認のみ」はエラー表示にモーダルを使うなという趣旨と読んだが、字義どおりだとフォルダ選択も禁じられる。**指示書内の曖昧さとして判断を仰ぐ** | 出力先はパスの入力欄 + フォルダの D&D のみで受ける（ダイアログを一切出さない） |
| 3 | **ICO の内部キー `_q_icoOrigDepth`**（Phase 1 の宿題） | **Phase 2 で core を直す。** `Converter.cpp` のメタデータ保持で `_q_` で始まるキーを除外し、テストを 1 本足す（差分 10 行程度）。ICO を読める以上、GUI で普通に踏む | Phase 3 送り（Phase 2 では core に触れない） |
| 4 | **不変条件スキャナ INV7 を追加してよいか**（`src/io` からの `QtWidgets` include 禁止） | **追加する。** 受け入れ基準 5 を機械化できる。違反フィクスチャも併せて置く | 追加しない（リンク構成のみで担保し、検査は人手） |
| 5 | **`spec-core.md` §1 の木への追加**（`CollisionPolicy` / `MemoryBudget` / `tests/app/` / `katachi_app_tests`） | **§1 に追記する。** Phase 1 の `tools/` と同じ扱い | 追加せず既存ファイルへ同居させる |
| 6 | **スキップと失敗の表し方** | `ByteSink::write()` の戻り値は `Result<std::monostate, IoError>` と決まっている（`cpp-conventions.md` §2.2）。**スキップを `IoError::DestinationExists` として返し、表示側で「スキップ（既存）」と読み替える。** 「宛先が既に存在し、ポリシーが Skip のため書かなかった」は io 層の事実であり、エラー列挙に置いても嘘ではない | `ByteSink` の戻り値型を変える（`cpp-conventions.md` §2.2 の変更を伴う） |

**1 は `phases.md` §5.3 が「Phase 2 着手時に決める」と明記した項目であり、
回答を得てから ADR-0008 に記録する。** 2〜6 も、回答を得るまで着手しない。

---

### 作業分割（1 タスク = 1 コミット）

| # | 内容 | 差分の見込み |
|---|---|---|
| T0 | 文書のみ。ADR-0008（メモリ上限）/ ADR-0009（衝突ポリシーの適用）/ ADR-0010（UI 構成と §7 の担保方法）を書き、`spec-core.md` §1 の木を更新 | 250 行 |
| T1 | `src/io` の土台。`IoError` / `IoConcepts` / `katachi_io`（**Widgets を引かない**）/ INV7 | 200 行 |
| T2 | `FileSource` / `FileSink`（`QSaveFile`） | 300 行 |
| T3 | `CollisionPolicy`（`Overwrite` / `Skip` / `Rename`、既定 `Skip`） | 250 行 |
| T4 | `JobRunner<Sink, Progress>`（ヘッダのみ。純粋な 1 件実行） | 350 行 |
| T5 | `MemoryBudget`（判断事項 1 の決定を実装） | 250 行 |
| T6 | `JobRunnerBridge`（`QtConcurrent::mapped` / `QFutureWatcher` / 200ms 間引き / キャンセル） | 400 行 |
| T7 | `JobTableModel` | 300 行 |
| T8 | `SettingsPanel`（選択肢は能力表から生成） | 350 行 |
| T9 | `MainWindow` 統合（D&D / 進捗 / キャンセル / タブ順 / ステータス行） | 450 行 |
| T10 | 受け入れ基準の検証（1000 件バッチ、UI 応答性、§7 の機械検査、実機確認、CI） | 300 行 |

判断事項 3 が「Phase 2 で直す」となった場合、**T2 の前に T1.5 として core の修正を単独コミットで入れる**
（Phase をまたぐ変更を他のコミットに混ぜないため）。

---

### 各タスクの詳細（触るファイル / テストの期待値 / 完了条件）

**テスト名は ASCII に限る**（Phase 0 の知見。`catch_discover_tests` がフィルタ引数に使うため）。
期待値は**実装前に確定させる**（`agent-protocol.md` §2）。

#### T0 — 文書と ADR（コードを書かない）

- 追加: `docs/adr/0008-batch-memory-budget.md` / `0009-collision-policy.md` / `0010-ui-composition.md`
- 変更: `docs/spec-core.md` §1（木）、§5（衝突ポリシーの適用形）、`docs/phases.md` §5.3（決着を §5.1 へ移す）
- テスト: なし
- 完了条件: 判断事項 1〜6 の回答が ADR に根拠付きで記録されている

#### T1 — `src/io` の土台

- 追加: `src/io/IoError.hpp` / `src/io/IoConcepts.hpp` / `src/io/CMakeLists.txt` / `tests/io/io_concepts_test.cpp`
- 変更: `CMakeLists.txt`（`add_subdirectory(src/io)`）/ `tests/CMakeLists.txt` /
  `tests/invariants/scan_invariants.cmake` + `cmake/QualityGates.cmake` + 違反フィクスチャ（INV7、判断事項 4 が承認された場合）
- `IoError` は**発生させられる値だけを定義する**（`phases.md` §2.2「全列挙値にテストがある」を守れる形にする）:
  `NotFound` / `OpenFailed` / `WriteFailed` / `TooLarge` / `DestinationExists`（判断事項 6）
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `IoError values are distinct and comparable` | 全列挙値が互いに区別され、同値比較が成立 |
| `static_assert`（肯定） | `ByteSource<MemorySource>` / `ByteSink<MemorySink>` / `ProgressSink<FakeProgress>` |
| `static_assert`（否定） | `!ByteSource<int>` / `!ByteSink<MemorySource>` / `!ProgressSink<MemorySink>` |
| `invariant.inv7` | `src/io` に `QtWidgets` の include が無い |
| `invariant.inv7.detects_violation` | 違反フィクスチャで**落ちる**（`WILL_FAIL`） |

- 完了条件: `katachi_io` が `Qt6::Widgets` を**リンクしていない**ことを `CMakeLists.txt` で確認でき、
  上記が green

#### T2 — `FileSource` / `FileSink`

- 追加: `src/io/FileSource.hpp/.cpp` / `src/io/FileSink.hpp/.cpp` / `tests/io/file_io_test.cpp`
- `FileSink` は `QSaveFile` を使う。**書き出しに失敗したとき部分ファイルを残さない**
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `FileSource reads back the bytes that were written` | 読み出した `QByteArray` が元の内容と**バイト一致** |
| `FileSource reports NotFound for a missing path` | `IoError::NotFound` |
| `FileSource reports OpenFailed for a directory` | `IoError::OpenFailed` |
| `FileSource reports TooLarge above the limit` | 上限 8 バイトのとき 9 バイトのファイルで `IoError::TooLarge`。**読み込み前にサイズで判定する**（メモリを踏まない） |
| `FileSink writes the exact bytes` | 書き出したファイルの内容が**バイト一致** |
| `FileSink reports WriteFailed for a missing directory` | 存在しない親ディレクトリで `IoError::WriteFailed` |
| `FileSink leaves no partial file when it fails` | 失敗後にそのパスが**存在しない** |

- 完了条件: `IoError` の `DestinationExists` を除く全値にテストがある（`DestinationExists` は T3）

#### T3 — 衝突ポリシー（ADR-0005 の宿題）

- 追加: `src/io/CollisionPolicy.hpp/.cpp` / `tests/io/collision_policy_test.cpp`
- 変更: `src/io/FileSink.hpp/.cpp`（ポリシーを受け取る）
- **既定は `Skip`。破壊的操作を既定にしない**（ADR-0005 の明示的な指示）
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `the default collision policy is Skip` | 既定値が `CollisionPolicy::Skip`。**`Overwrite` でない** |
| `no collision keeps the requested name` | 既存ファイルが無ければ要求どおりの名前 |
| `Overwrite replaces the existing file` | 既存の内容が新しい内容に置き換わる |
| `Skip leaves the existing file untouched` | 既存ファイルの内容が**変わらない**。戻り値は `IoError::DestinationExists` |
| `Rename appends _1 when the name is taken` | `photo.png` があるとき `photo_1.png` |
| `Rename continues to _2` | `photo.png` と `photo_1.png` があるとき `photo_2.png` |
| `Rename gives up after the limit` | 上限（10000）まで埋まっていれば `IoError::WriteFailed` を返し、**無限ループしない** |

- 完了条件: `IoError` の全列挙値にテストが揃う

#### T4 — `JobRunner<Sink, Progress>`

- 追加: `src/io/JobRunner.hpp`（**ヘッダのみ。`.cpp` を作らない** — `cpp-conventions.md` §2.4）/ `tests/io/job_runner_test.cpp`
- 提案するかたち（承認が要る箇所は判断事項 6 に含む）:

```
template <ByteSink Sink, ProgressSink Progress> class JobRunner
    JobRunner(const core::CapabilityTable&, Progress&, BatchCounter&)
    template <ByteSource Source> JobOutcome runOne(Source&, Sink&, const JobItem&)
```

  クラスの型引数は `spec-core.md` §6 の表記どおり **2 つ**に保ち、入力側は
  **メンバ関数テンプレート**として `ByteSource` で制約する。
  `BatchCounter` は `std::atomic<int> completed` と `int total` を持つ注入される構造体で、
  グローバル可変状態ではない。`onProgress(done, total)` の引数が本番でもテストでも同じ意味を持つ。
- **Qt のイベントループもファイルシステムも使わずにテストする**（`MemorySource` / `MemorySink` / `FakeProgress`）
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `runOne converts and writes through the sink` | Sink が受け取ったバイト列が `core::convert()` の出力と一致 |
| `runOne reports progress once per item` | 10 件を順に流すと `onProgress` が **10 回**、`done` が 1..10 の昇順 |
| `runOne skips work when already cancelled` | `isCancelled()` が true のとき **Sink が呼ばれない**。状態は `Cancelled` |
| `runOne records a decode failure with its reason` | `not_an_image.bin` 相当の入力で状態 `Failed`、理由 `ConvertError::DecodeFailed` |
| `runOne records a naming failure with its reason` | 空パターンで状態 `Failed`、理由 `NamingError::EmptyPattern` |
| `runOne records a sink failure with its reason` | 常に失敗する Sink で状態 `Failed`、理由 `IoError::WriteFailed` |
| `runOne carries the converter warnings` | アルファ非対応形式 + `Preserve` で `ConvertWarning::AlphaFlattenedFallback` が 1 件載る |
| `static_assert`（否定） | `!ProgressSink<（isCancelled が noexcept でない型）>` |

- 完了条件: `JobRunner.hpp` に `.cpp` が無く、テストが Qt のイベントループもファイルも使わずに green

#### T5 — `MemoryBudget`（`phases.md` §5.3）

- 追加: `src/io/MemoryBudget.hpp/.cpp` / `tests/io/memory_budget_test.cpp`
- 判断事項 1 で **A（バイト予算セマフォ）**が承認された場合の期待値:

| テスト | 期待値 |
|---|---|
| `the budget admits jobs up to its total` | 予算 4 単位で 2+2 は同時に通る |
| `the budget blocks the third job until one returns` | 4 単位で 2+2 を確保中、3 件目は待たされる。1 件返すと通る |
| `an oversized job runs exclusively` | 予算より大きい見積りは**全量を確保**して単独実行になる（デッドロックしない） |
| `the guard releases on every path` | 失敗・例外なしの早期 return でも確保が戻る（RAII） |
| `the estimate uses header dimensions` | 幅 64 × 高さ 64 の PNG の見積りが `ファイルサイズ + 4*64*64 + ファイルサイズ` |

- 完了条件: ASan / UBSan 込みで green（並行確保のテストを含む）

#### T6 — `JobRunnerBridge`

- 追加: `src/io/JobRunnerBridge.hpp/.cpp` / `tests/io/job_runner_bridge_test.cpp`
- 変更: ルート `CMakeLists.txt`（`find_package` に `Concurrent` を追加）
- 並列度は `QThread::idealThreadCount() - 1`（最低 1）。専用の `QThreadPool` に設定する
  （`globalInstance()` を書き換えない）
- **接続を済ませてから `setFuture()` を呼ぶ**（公式ドキュメントの指示）
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `the bridge finishes a small batch` | 5 件すべてが成功し、`finished` が **1 回**だけ届く |
| `the bridge throttles progress to 200ms` | タイマの `interval()` が **200**。かつ 200 件のバッチで進捗シグナルが **200 回未満** |
| `cancel stops the batch and the app returns to idle` | 開始直後の `cancel()` 後、出力ファイル数が入力件数**未満**。`finished` が届き、`isRunning()` が false |
| `the bridge can start a second batch after a cancel` | キャンセル後に再度開始して**全件成功**（正常状態への復帰） |
| `failed jobs stay in the results with a reason` | 壊れた入力を混ぜると、その 1 件が `Failed` と理由付きで結果に残り、他は成功 |
| `the worker never touches the main thread objects` | 変換中に `QThread::currentThread()` が main と異なることを、ワーカー側で記録して確認 |

- 完了条件: 上記が green。**「未確認として扱うもの」1〜3 の結果を本 progress に追記する**
  （Qt 6.8 での `QThreadPool*` 版の実在 / `Qt6::Concurrent` の入手 / キャンセル後の `finished`）

#### T7 — `JobTableModel`

- 追加: `src/app/JobTableModel.hpp/.cpp` / `tests/app/job_table_model_test.cpp` / `tests/app/main.cpp`（`QApplication`）
- 変更: `src/app/CMakeLists.txt` / `tests/CMakeLists.txt`（`katachi_app_tests` の新設）
- 列: 入力ファイル / 出力ファイル / 状態 / 理由
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `the model exposes one row per job` | 3 件追加で `rowCount() == 3`、`columnCount() == 4` |
| `a failed job keeps its row and shows the reason` | 失敗した行が**消えず**、理由列が空でない |
| `a skipped job is shown as skipped not failed` | `IoError::DestinationExists` の行が「スキップ」と表示される（判断事項 6） |
| `updating one row emits dataChanged for that row only` | `dataChanged` の範囲が当該行のみ |
| `the model never reorders rows` | 完了順が入力順と異なっても行の並びは**追加順のまま** |

- 完了条件: 表示文字列がすべて app 層にあり、INV3B が green

#### T8 — `SettingsPanel`

- 追加: `src/app/SettingsPanel.hpp/.cpp` / `tests/app/settings_panel_test.cpp`
- 出力形式の選択肢は `CapabilityTable::encodable()` から作る。**フォーマット名を書かない**
- 品質は対象形式が `supportsQuality` のときだけ有効化する
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `the format list comes from the capability table` | 項目数が `encodable().size()` と一致し、PNG 相当が含まれる |
| `the quality control follows supportsQuality` | `supportsQuality` が false の形式を選ぶと品質欄が `isEnabled() == false` |
| `the panel produces the spec it displays` | 画面の値から作った `ConversionSpec` が各欄と一致（品質 / リサイズ / アルファ / メタデータ / ICC） |
| `the default collision policy shown is Skip` | 初期表示が `Skip`（ADR-0005） |
| `no format name literal is present` | `invariant.inv3b` が green |

- 完了条件: 上記が green

#### T9 — `MainWindow` 統合

- 変更: `src/app/MainWindow.hpp/.cpp` / 追加: `tests/app/main_window_test.cpp`
- D&D、開始 / キャンセル、進捗バー、ステータス行、タブ順
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `dropping files adds one job per file` | URL 3 件の `QMimeData` を `dropEvent` に渡すと 3 行増える |
| `dropping a folder adds its images recursively` | 2 階層のフォルダ内の画像 2 枚が追加され、**画像でないファイルは追加されない**（判定は能力表の `extensions`） |
| `dropping the same file twice adds it once` | 重複は 1 行のまま |
| `the tab order is set explicitly` | 主要ウィジェットが `setTabOrder` で連結されている |
| `the table does not auto scroll` | 行追加後もビューの `verticalScrollBar()->value()` が**変わらない**。`autoScroll()` が false |
| `errors are shown in the status line not a dialog` | 失敗時にモーダルが開かない（`QApplication::activeModalWidget() == nullptr`）。ステータス行の文字列が空でない |

- 完了条件: 上記が green。キーボードのみで開始・キャンセルできることを実機で確認

#### T10 — 受け入れ基準の検証

- 追加: `tests/app/batch_responsiveness_test.cpp` / `tests/io/large_batch_test.cpp`
- テストと期待値:

| テスト | 期待値 |
|---|---|
| `a batch of 1000 files completes` | 一時ディレクトリに 1000 件を用意して全件完了。**成功件数 1000** |
| `the event loop keeps running during a 1000 file batch` | バッチ中に main thread の `QTimer(10ms)` が **20 回以上**発火する（イベントループが回り続けた証拠。閾値は余裕を持たせ、CI のばらつきで落ちないようにする） |
| `progress signals stay well below the job count` | 1000 件で進捗シグナルが **100 回未満**（200ms 間引きの実効性） |
| `the peak concurrent estimate stays within the budget` | `MemoryBudget` の同時確保量の最大値が予算を**超えない**（確保時の最大値を記録して検証） |
| `no animation or custom palette is used` | `grep` で `QPropertyAnimation` / `QGraphicsEffect` / `setStyleSheet` / `setPalette` が `src/` に **0 件** |

- 併せて実施: **macOS 実機での起動・D&D・変換・キャンセルの確認**、CI（macOS / Windows、Qt 6.8.3）の 4 ジョブ green
- 完了条件: `phases.md` §4 Phase 2 の 6 項目すべてに「達成」と根拠を書ける状態

---

### 差分規模の申告（停止条件 8）

**Phase 2 全体で 3200〜3800 行の見込み**（本体 1600 行 / テスト 1400 行 / 文書 400 行程度）。
Phase 1（+4723 行）と同程度。**T6 と T9 は単独で 400 行を超える見込み**であり、
Phase 1 の T5（724 行）と同じく事前に申告する。分割できるなら分割するが、
`JobRunnerBridge` は「並列実行 + 進捗 + キャンセル」が一体でないとテストが書けず、
`MainWindow` は「D&D + 開始 + 進捗 + キャンセル」が揃って初めて受け入れ基準を検証できる。

---

### 今回やらないこと（次 Phase 送り・明示的な非スコープ）

| 事項 | 理由 |
|---|---|
| 追加コーデック（HEIF / AVIF / JXL / RAW / PSD） | Phase 3 |
| 配布（`macdeployqt` / 署名 / インストーラ） | Phase 4 |
| EXIF 全体の保持 | ADR-0003。Qt 単体では不可 |
| **設定の永続化（`QSettings`）** | **指示書に記述が無い。** 停止条件 1 に該当するため実装しない |
| プリセット / 履歴 / 元に戻す | 同上 |
| 多フレーム画像（アニメーション GIF の各フレーム） | `spec-core.md` §8 で非スコープ |
| Windows 実機確認 | Phase 0 からの宿題。**手元に Windows 実機が無い。** CI のみである旨を報告に明記し続ける |

---

### 推測で埋めた箇所

**なし。** 確信が持てない 4 点（Qt 6.8 の `QThreadPool*` 版 / `Qt6::Concurrent` の入手 /
キャンセル後の `finished` / 実行中項目の中断可否）は、
**「未確認」と明記し、T6 で実測して結果を追記する形にした。**
実行中項目の中断可否については、明記が無い以上「中断されない」前提の設計（二段構えのキャンセル）を採る。

### 残課題 / 次にやること

1. **本計画の承認待ち。承認前に T0 へ進まない。** 特に判断事項 1〜6 の回答が要る。
2. 承認後、`phase2` ブランチを作って T0 から着手する。
3. `phase1` ブランチの削除が未実施（Phase 1 の残課題）。
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — 判断事項 1〜4 の回答を得た（計画の追補。実装は未着手）

**追記のみの原則に従い、上のエントリは書き換えず、決定をここに記録する。**

### 得られた判断（利用者回答）

| # | 事項 | 判断 |
|---|---|---|
| 1 | バッチ実行時のメモリ上限（`phases.md` §5.3） | **A: バイト予算セマフォ。** 総予算 1 GiB。見積り = 入力サイズ + 4 × 幅 × 高さ + 出力見込み。予算超のジョブは全量確保して単独実行 |
| 2 | 出力先選択の `QFileDialog`（モーダル） | **使ってよい。** エラー表示にモーダルを使わないことが §7 の趣旨であり、フォルダ選択は対象外とする |
| 3 | ICO の内部キー `_q_icoOrigDepth` | **Phase 2 で core を直す。** `_q_` で始まるキーを保持対象から除外する |
| 4 | 不変条件スキャナ INV7（`src/io` → `QtWidgets`） | **追加する。** 違反フィクスチャも併せて置く |

### 計画への反映

- **T0 の ADR-0008 に判断 1 の内容と 1 GiB の根拠を書く**（`maxPixels` 既定 268,435,456 px を
  `ARGB32` で保持した量と一致させ、「最大サイズの画像は必ず 1 枚ずつ処理される」という
  説明可能な性質にする）。ADR-0010 に判断 2 を書く
- **T1.5 を新設する（T1 の後、T2 の前）。** 判断 3 の core 修正を**単独コミット**で入れる。
  Phase をまたぐ変更を他のコミットに混ぜないため（`agent-protocol.md` §6）

| T1.5 の内容 | 詳細 |
|---|---|
| 変更 | `src/core/Converter.cpp`（メタデータ保持時に `_q_` で始まるキーを除外）/ `tests/fixtures/generate.cpp`（ICO フィクスチャの追加）/ `tests/core/converter_test.cpp` |
| テスト | `PreserveSupported does not leak Qt internal text keys` — ICO を読んで PNG へ変換した出力の `QImage::textKeys()` に **`_q_` で始まるキーが 1 つも無い**。かつ**利用者由来のテキストは従来どおり保持される**（除外が広すぎないことの否定側テスト） |
| 完了条件 | 上記 2 本が green。既存 93 テストが 1 本も落ちない |

- **T1 に INV7 を含める**（判断 4）。`scan_invariants.cmake` に `INV7`、`QualityGates.cmake` の
  `KATACHI_INVARIANT_CHECKS` に追加、`tests/invariants/fixtures/violations/inv7/io/WidgetsInclude.hpp` を置く。
  検査は 6 種から **7 種**になるため、`docs/phases.md` §3 の一覧にも追記する
- **T9 に `QFileDialog` を含める**（判断 2）。併せて「上書き（`Overwrite`）で実行するときのみ確認モーダルを出す」
  という §7 の唯一の例外もここで実装する

### 差分規模の再申告

T1.5（約 80 行）と INV7（約 60 行）の追加により、**Phase 2 全体は 3350〜3950 行の見込み**。
タスク単位の見込みは上のエントリから変更しない。

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **判断事項 5・6 の回答と、計画全体の承認待ち。** 承認前に T0 へ進まない。
2. 承認後、`phase2` ブランチを作って T0 → T1 → **T1.5** → T2 … の順で着手する。

---

## 2026-08-09 — T0 完了（ADR 0008〜0010 と指示書の更新）

### 実施内容

判断事項 5・6 の承認と計画全体の承認を得たため、`phase2` ブランチを作って T0 を実施した。
**コードは 1 行も書いていない。** 決定を文書に固定してから実装に入る順序を守るための先行タスク
（Phase 1 の T0 と同じ考え方）。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `docs/adr/0008-batch-memory-budget.md` | バイト予算セマフォ（総量 1 GiB）。見積り式とその係数の根拠 |
| `docs/adr/0009-collision-policy.md` | 衝突ポリシーは `FileSink` が適用。スキップは `IoError::DestinationExists` |
| `docs/adr/0010-ui-composition.md` | UI の縦 3 段構成、モーダルの許容 2 用途、§7 の 7 項目の担保方法 |

**変更**

| ファイル | 内容 |
|---|---|
| `docs/spec-core.md` §1 | 木に `io/CollisionPolicy.*` と `io/MemoryBudget.*` を追加。io が `Qt6::Widgets` をリンクしないこと、`tests/` に `app` を追加 |
| `docs/spec-core.md` §5 | 衝突ポリシーの適用場所とスキップの表し方を追記（ADR-0009） |
| `docs/phases.md` §5.1 | 決定 3 件（メモリ上限 / 衝突ポリシーの適用 / UI 構成）を追加 |
| `docs/phases.md` §5.3 | 「該当なし」に更新（唯一の項目が決着し §5.1 へ移動したため） |

**削除**: なし

### 承認された計画からの変更（申告）

**メモリ見積り式の係数を `4 * 幅 * 高さ` から `16 * 幅 * 高さ` に変えた。**

承認いただいた計画には「見積り = 入力サイズ + 4 × 幅 × 高さ + 出力見込み」と書いていた。
ADR を書くにあたって `src/core/Converter.cpp` を実際に読み直したところ、
**アルファ合成の局面で画像が 4 枚同時に生存する**ことが分かった。

| 局面 | 同時に生存する画像 | 根拠 |
|---|---|---|
| 復号のみ | 1 枚 | `Converter.cpp:120` |
| **アルファ合成** | **4 枚**（`image` / `normalized` / `canvas` / `convertToFormat` の戻り） | `Converter.cpp:41-56` |
| テキスト除去 | 2 枚 | `Converter.cpp:65-77` |
| リサイズ | 2 枚 | `Converter.cpp:136` |

係数 4 のままだと、**予算が最悪 4 倍の嘘をつく。**
方式（バイト予算セマフォ）と総量（1 GiB）は承認どおりで、係数のみを実測に合わせた。
併せて「リサイズで拡大する場合は出力側の寸法を使う」も加えた（`max(...)`）。

**不要であれば戻す。**

### 追加・変更したテスト

**なし。** T0 は文書のみのタスク。テストは T1 以降で追加する。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。**ステージ済みの状態で実行**）

文書のみの変更だが、ゲート未実行での報告はしない方針のため全 6 本を実行した。
`git ls-files '*.cpp' '*.hpp'` の対象は 33 ファイル。

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --preset dev` / `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **93 / 93 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 5 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **93 / 93 pass** |

**環境について記録しておく（次のセッションが同じ所で詰まらないため）。**
`clang-format` / `clang-tidy` は Homebrew LLVM（22.1.8。CI の `LLVM_VERSION` と同一）だが、
**keg-only のため `PATH` に載っていない。** `/opt/homebrew/opt/llvm/bin/` を明示して実行した。
`PATH` 頼りに実行すると `exit 127`（command not found）になり、
**終了コードだけ見ていると「落ちた」と誤読しかねない。**

### 推測で埋めた箇所

**なし。** 見積り係数は `src/core/Converter.cpp` を読んで数えた値であり、推定値ではない。

### 残課題 / 次にやること

1. **T1（`src/io` の土台）に着手する。** `IoError.hpp` / `IoConcepts.hpp` /
   `src/io/CMakeLists.txt`（**`Qt6::Widgets` をリンクしない**）/ 不変条件スキャナ INV7 と違反フィクスチャ /
   `tests/io/io_concepts_test.cpp`。`docs/phases.md` §3 の検査一覧を 6 種 → 7 種に更新するのもこのタスク。
2. T1 で `src/io` が実在するようになるため、**INV7 が空振りでないことを確認する**
   （Phase 1 の T1 で INV3A / INV4 について行ったのと同じ検証）。
3. `.serena/` が未追跡のまま残っている（本セッションで Serena を有効化したときに生成された）。
   **T0 のコミットには含めていない。** `.gitignore` に足すかどうかは別途判断を仰ぐ。
4. `phase1` ブランチの削除が未実施（Phase 1 の残課題）。
5. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T1 完了（`src/io` の土台と不変条件スキャナ INV7）

### 実施内容

`src/io` を新設し、io 層のエラー列挙と concept 3 種を実装した。
併せて承認済みの INV7（`src/io` から `QtWidgets` を include しない）を追加した。
TDD の順序（失敗するテスト → 意図した理由の確認 → 実装）を守った。

### TDD の経過

1. `tests/io/io_concepts_test.cpp` を先に書き、`tests/CMakeLists.txt` に配線してビルド
2. **意図した理由での失敗を確認**

```
tests/io/io_concepts_test.cpp:10:10: fatal error: 'io/IoConcepts.hpp' file not found
```

   `katachi_core` が `${PROJECT_SOURCE_DIR}/src` を PUBLIC な include ディレクトリとして
   公開しているため、`katachi_io` が存在しない状態でも**「ヘッダが無い」という
   意図どおりの理由**で落ちる（CMake の構成エラーにはならない）。
3. `IoError.hpp` / `IoConcepts.hpp` / `src/io/CMakeLists.txt` を実装
4. INV7 とその違反フィクスチャを追加

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/io/IoError.hpp` | `IoError`（5 値）。**発生させられる値だけを定義した** |
| `src/io/IoConcepts.hpp` | `ByteSource` / `ByteSink` / `ProgressSink`（`cpp-conventions.md` §2.2 の定義どおり） |
| `src/io/CMakeLists.txt` | `katachi_io`。**`Qt6::Widgets` をリンクしない。** T1 時点はヘッダのみのため `INTERFACE` |
| `tests/io/io_concepts_test.cpp` | 実行時テスト 2 本 + `static_assert` 7 本。テストダブル 4 種 |
| `tests/invariants/fixtures/violations/inv7/io/WidgetsInclude.hpp` | INV7 の違反フィクスチャ |

**変更**

| ファイル | 内容 |
|---|---|
| `CMakeLists.txt` | `add_subdirectory(src/io)` を core と app の間に追加（依存方向どおりの順） |
| `tests/CMakeLists.txt` | `io/io_concepts_test.cpp` の追加、`katachi_io` のリンク |
| `tests/invariants/scan_invariants.cmake` | INV7 の走査対象（`io`）と判定を追加 |
| `cmake/QualityGates.cmake` | `KATACHI_INVARIANT_CHECKS` に INV7 を追加（検査 12 本 → 14 本） |
| `docs/phases.md` §3 | スキャナを 6 種 → **7 種**に更新し、追加の根拠を明記 |

**削除**: なし

### 設計上の判断

**`katachi_io` は T1 時点では `INTERFACE` ライブラリ。** `IoError.hpp` / `IoConcepts.hpp` が
ヘッダのみで、`STATIC` にするとソースが無くて CMake が失敗する。
Phase 1 の `katachi_core` と同じ経緯であり、**`FileSource.cpp` が入る T2 で `STATIC` へ変更する。**

**`Qt6::Widgets` をリンクしないことが受け入れ基準 5 の実体的な担保になる。**
リンクしなければ `#include <QWidget>` のようなフラットヘッダは include パスに無く、
コンパイル時に落ちる。INV7 はテキスト上の検出を担う。両方が揃って初めて塞がる。

### 計画からの変更・追加（申告）

**1. テストを 1 本ではなく 2 本にした。**
承認された計画の T1 は実行時テスト 1 本（`IoError values are distinct and comparable`）だったが、
**テストダブルが実際に動くことを確かめる 1 本を足した**
（`the test doubles carry bytes through the io concepts`）。
`MemorySource` / `MemorySink` / `FakeProgress` は T4 の `JobRunner` テストの土台になる。
concept は構文しか見ないため、`static_assert` が通っても中身が壊れている可能性がある
（`cpp-conventions.md` §2.5）。T4 が落ちたときに切り分けられるようにした。

**2. `!ProgressSink<（isCancelled が noexcept でない型）>` を T4 から T1 へ前倒しした。**
計画では T4 に置いていたが、`ProgressSink` を定義するのは T1 であり、
否定側の `static_assert` は定義と同じ場所にある方が対応が見える。

**3. `tests/CMakeLists.txt` から `katachi_core` の明示リンクを外した。**
下記のリンカ警告を構成側で解消するため。`katachi_io` が INTERFACE で伝播する。

### 遭遇した問題: リンカ警告（抑制せず構成で解消した）

`katachi_tests` に `katachi_core` と `katachi_io` を両方書いたところ、
`katachi_io` が `katachi_core` を INTERFACE 伝播するため同じアーカイブがリンク行に 2 回並び、
Apple ld が警告を出した。

```
ld: warning: ignoring duplicate libraries: 'src/core/libkatachi_core.a'
```

**`-Werror` はリンカに効かないため、ビルドは exit 0 のまま警告だけが残る。**
終了コードだけ見ていれば見逃していた。`CLAUDE.md` の「警告ゼロ」に反するため、
**明示リンクを外して重複を無くした**（抑制はしていない）。理由は CMake にコメントとして残した。

### 追加・変更したテスト（4 本追加。93 → 97）

| テスト | 期待値 | 結果 |
|---|---|---|
| `IoError values are distinct and comparable` | 5 列挙値が互いに区別され、同値比較が成立 | pass |
| `the test doubles carry bytes through the io concepts` | `MemorySource` → `MemorySink` でバイト列が一致。`FakeProgress` の `cancel()` / `onProgress()` が反映される | pass |
| `invariant.inv7` | `src/io` に `QtWidgets` / `QWidget` の include が無い | pass |
| `invariant.inv7.detects_violation` | 違反フィクスチャで**落ちる** | pass（`WILL_FAIL`） |

`static_assert` による契約:

- 肯定: `ByteSource<MemorySource>` / `ByteSink<MemorySink>` / `ProgressSink<FakeProgress>`
- 否定: `!ByteSource<int>` / `!ByteSink<MemorySource>` / `!ProgressSink<MemorySink>` /
  **`!ProgressSink<NoexceptlessProgress>`**（`isCancelled()` が `noexcept` でない型。
  concept の `noexcept` 要求が実際に効いていることの確認）

**本番型の `static_assert` はまだ無い。** `docs/phases.md` §2.2 は
「その Phase で定義した concept について、本番型とテストダブル両方」を求めるが、
本番型（`FileSource` / `FileSink` / `JobRunnerBridge`）は T2 と T6 で入る。
**T2 で `ByteSource<FileSource>` / `ByteSink<FileSink>`、T6 で `ProgressSink<JobRunnerBridge>` を足す。**
Phase 2 完了時に 3 つとも揃っていることを T10 で確認する。

### INV7 が空振りでないことの確認（計画で約束した検証）

違反フィクスチャが落ちることに加えて、**実ファイルで検出できることを確かめた。**

| 確認 | 結果 |
|---|---|
| `src/io/IoError.hpp` に `#include <QtWidgets/QWidget>` を一時的に追加 | **検出**（`io/IoError.hpp:25`）。exit 1 |
| 一時的な追加を戻す | `[INV7] ok`。`git diff` が空であることも確認 |

### `Qt6::Widgets` を引いていないことの実測

生成された `build.ninja` の `katachi_tests` のリンク行を直接見た。

```
1 libkatachi_core.a
1 QtCore.framework
1 QtGui.framework
```

**`QtWidgets.framework` が無い。** かつ `libkatachi_core.a` が **1 回だけ**であり、
上記のリンカ警告が解消していることも同じ実測で確認できた。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev`（`--clean-first` でも確認） | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **97 / 97 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 5 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **97 / 97 pass** |

`clang-format` は新規 4 ファイルに対して整形違反を出したため（列挙子の行末コメント揃え）、
`clang-format -i` で整形してから再実行した。**整形したのは今回追加したファイルのみ。**

**clang-tidy の到達範囲に注意。** 対象は `git ls-files 'src/*.cpp'` であり、
`src/io` はまだ `.cpp` を持たないため**この 2 ヘッダには届いていない**。
`src/app` / `src/core` の 5 本はいずれも io ヘッダを include していないため、
`HeaderFilterRegex` 経由でも届かない。**T2 で `FileSource.cpp` が入るとゲートが io へ届く。
届いた結果として新たな指摘が出ないか確認する**（Phase 1 の T3 で同じことが起きた）。

### 推測で埋めた箇所

**なし。** concept の定義は `docs/cpp-conventions.md` §2.2 の記述をそのまま写した。
`IoError` の列挙値は、T2 / T3 で実際に発生させられることを確認できるものだけに絞った。

### 残課題 / 次にやること

1. **T1.5（ICO の内部キー `_q_icoOrigDepth`）に着手する。** 判断 3 で承認された core の修正。
   `src/core/Converter.cpp` のメタデータ保持で `_q_` で始まるキーを除外し、
   ICO フィクスチャとテスト 1 本（否定側込み）を足す。**単独コミットにする。**
2. その後 T2（`FileSource` / `FileSink`）。ここで `katachi_io` を `INTERFACE` → `STATIC` へ変更し、
   clang-tidy ゲートが io に届くようになる。
3. `.serena/` は未追跡のまま。T1 のコミットにも含めていない。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T1.5 完了（Qt の内部テキストキーが出力へ漏れる欠陥の修正）

### 実施内容

Phase 1 の T5 追補で「対応していないこと」として記録した宿題を果たした。
**判断 3 で「Phase 2 で core を直す」と承認を得たもの。**
`MetadataPolicy::PreserveSupported` が、Qt が読み取り時に注入する内部キーまで
出力へ書き出していた。**落ちるテストとして固定してから直した。**

### TDD の経過（RED を実測で確認した）

ICO フィクスチャとテスト 3 本を先に入れてビルドし、**2 本が落ちることを確認した。**

```
79/100 preserve supported drops qt internal text keys ................***Failed
  REQUIRE_FALSE( key.startsWith(u"_q_") )
  _q_icoOrigDepth                                    <- 出力 PNG に漏れていた
82/100 preserve supported keeps user text while dropping internal keys ***Failed
  REQUIRE_FALSE( decoded.textKeys().contains(u"_q_probe") )
83/100 reading the ico fixture injects a qt internal text key .........Passed
```

**83 は最初から pass。** これは前提の確認であり、Qt が実際に内部キーを注入することを示す。
79 / 82 が落ちたことで、**欠陥が実在し、テストがそれを捉えている**ことが確定した。

フィクスチャ生成器の実測出力（Qt 6.11.1 / macOS 14 arm64）。

```
  icon.ico  16958 bytes
  icon.ico: 読み戻せる（テキストキー: _q_icoOrigDepth）
```

### 変更ファイル

**変更**

| ファイル | 内容 |
|---|---|
| `src/core/Converter.cpp` | `withoutInternalText()` を追加し、`PreserveSupported` の経路で呼ぶ |
| `tests/fixtures/generate.cpp` | ICO フィクスチャ `icon.ico` の生成と、読み戻したテキストキーの出力 |
| `tests/core/fixtures_test.cpp` | `icon.ico` を大きさ検査へ追加、前提テスト 1 本を追加 |
| `tests/core/converter_test.cpp` | 内部キーのテスト 2 本を追加 |

**追加 / 削除**: なし（フィクスチャ画像はビルド時生成でコミットしない）

### 実装の要点

- **落とすのは `_q_` で始まるキーだけ。** 利用者のキーは畳まない。
  `PreserveSupported` の意味は「利用者の metadata を保つ」ことであり、
  Qt の内部情報を出力へ漏らすことではない
- **内部キーが 1 つも無ければ画像を作り直さない。** `withoutText()` は生ビットからの
  作り直しで確保を伴うため、必要なときだけ通す（ADR-0002）
- 作り直しの際に付随情報が落ちる問題は、T5 追補で `withoutText()` に入れた
  復元処理（カラーテーブル / DPI / devicePixelRatio / 色空間）がそのまま効く。
  **同じ穴を二度掘らずに済んだ**

### clang-tidy の指摘 1 件と対処（抑制していない）

```
Converter.cpp:88:11: error: no header providing "QStringList" is directly included
                            [misc-include-cleaner]
```

**`#include <QStringList>` は既に書かれているのに出た。**
Qt 6 の `QStringList` は `qcontainerfwd.h` で宣言された `QList<QString>` の別名であり、
フラットヘッダ `<QStringList>` は提供ヘッダとして認識されない。

対処: **型そのものを `QList<QString>` と綴り、`<QList>` を直接 include した。**
別名を隠すために `auto` へ逃げることはしなかった（型が読めなくなるため）。理由はコードにも残した。

Phase 1 の `QStringLiteral` の件と同種で、**Qt のフラットヘッダは
include-cleaner から見ると素通しではない**という一般則がある。今後も同じ形で出る。

### 追加・変更したテスト（3 本追加。97 → 100）

| テスト | 期待値 | 修正前 | 修正後 |
|---|---|---|---|
| `preserve supported drops qt internal text keys` | ICO → PNG の出力に `_q_` で始まるキーが**1 つも無い** | **fail**（`_q_icoOrigDepth`） | pass |
| `preserve supported keeps user text while dropping internal keys` | `_q_probe` は消え、**`Description` は残る**（除外が広すぎないことの否定側） | **fail** | pass |
| `reading the ico fixture injects a qt internal text key` | ICO を読むと `_q_` で始まるキーが注入される（前提の明示） | pass | pass |

2 本目は **ICO の内部挙動に依存しない**。内部キーと利用者キーの両方を持つ画像を
その場で作って通すため、Qt が ICO の扱いを変えても除外の精度は検証され続ける。

既存の `preserve supported keeps text metadata`（`with_text.png` のテキスト保持）も
落ちていない。**利用者由来の metadata の扱いは何も変わっていない。**

### CI で落ちうる点（先に書いておく）

**`reading the ico fixture injects a qt internal text key` は Qt の実装詳細に依存する。**
CI の Qt 6.8.3 が `_q_icoOrigDepth` を注入しない場合、このテストだけが落ちる。
その場合でも**修正そのものは正しい**（2 本目のテストが内部キーの除外を保証する）。
落ちたら前提テストの扱いを判断を仰ぐ。**先に想定を書いておき、驚かないようにする。**

### 前 Phase の記録との食い違い（訂正。修正はしていない）

`docs/progress/phase1.md` の T5 追補は
「`tests/core/fixtures_test.cpp` に `indexed.png` を大きさ検査の対象に追加」と書いているが、
**実際のコードにはその記述が無かった**（`icon.ico` を足すときに一覧を読んで気づいた）。
`indexed.png` を使うテスト自体は `converter_test.cpp` に存在し green のため、実害は無い。

**本タスクの範囲外なので直していない**（「ついでに」を避けた）。
`indexed.png` を大きさ検査へ足すかどうかは指示を仰ぐ。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **100 / 100 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 5 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **100 / 100 pass** |

### 推測で埋めた箇所

**なし。** 内部キーが実際に漏れることは、テストを落として確認した。
`_q_` 接頭辞が Qt の内部用であることは、`icon.ico` の読み戻しで
`_q_icoOrigDepth` が実測されたことに基づく。

### 残課題 / 次にやること

1. **T2（`FileSource` / `FileSink`）に着手する。** ここで `katachi_io` を
   `INTERFACE` → `STATIC` へ変更し、**clang-tidy ゲートが io に届くようになる。**
   届いた結果として新たな指摘が出ないか確認する（Phase 1 の T3 と同じ）。
2. `indexed.png` を `fixtures_test.cpp` の大きさ検査へ足すかどうかの判断（上記）。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T2 完了（`FileSource` / `FileSink`）

### 実施内容

io 層の入出力を実装した。`FileSink` は `QSaveFile` を使い、
**失敗しても部分的に書かれたファイルを残さない。**
TDD の順序を守り、実装前に意図した理由での失敗
（`'io/FileSink.hpp' file not found`）を確認した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/io/FileSource.hpp/.cpp` | `QFile` 読み出し。上限バイト数を持つ（ADR-0008）。`static_assert(ByteSource<FileSource>)` |
| `src/io/FileSink.hpp/.cpp` | `QSaveFile` による原子的な書き出し。`static_assert(ByteSink<FileSink>)` |
| `tests/io/file_io_test.cpp` | 実行時テスト 7 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `src/io/CMakeLists.txt` | **`INTERFACE` → `STATIC`**（`.cpp` が入ったため）。`Qt6::Core` を明示リンク |
| `src/io/IoConcepts.hpp` | 仮引数名を `t` → `source` / `sink` へ（下記の clang-tidy 指摘） |
| `tests/CMakeLists.txt` | `io/file_io_test.cpp` を追加 |

**削除**: なし

### 設計上の判断

**`FileSource::read()` は上限判定を読み込みより前に行う**（ADR-0008）。
`QFileInfo::size()` で測ってから `open()` する。読んでから測っても手遅れになる。
上限の既定は予算総量と同じ 1 GiB とした。

**`readAll()` の後に `QFile::error()` を確認する。**
開けたが読み切れなかった場合に、黙って短いバイト列を返さない。
途中で切れた入力をそのまま変換すると「壊れた画像」ではなく**「壊れた出力」**になって外へ出る。
専用の列挙値（`ReadFailed` 等）を作らず `OpenFailed` に寄せたのは、
**移植可能な形で発生させる手段が無く、`docs/phases.md` §2.2 の
「全列挙値にテストがある」を満たせないため。** この分岐にはテストが無い。理由をコードにも書いた。

**`FileSink::write()` は `const` メンバにした。** メンバを書き換えないため。
**T3 で衝突ポリシーを入れると解決後のパスを保持する必要が生じ、`const` を外す。**
先回りして非 `const` にしておくことはしなかった（`readability-make-member-function-const` が
現時点の実装を正しく指摘するため。将来のために嘘の署名を置かない）。

**ディレクトリを自動で作らない。** 親ディレクトリが無ければ `WriteFailed` を返す。
利用者が指していない場所へ書かない。

### clang-tidy がついに io へ届いた（T1 で予告した確認）

`FileSource.cpp` / `FileSink.cpp` が入ったことで、ゲートの対象が 5 本 → **7 本**になり、
`HeaderFilterRegex` 経由で `src/io` のヘッダにも届くようになった。予告どおり新たな指摘が出た。

```
IoConcepts.hpp:20:34: error: parameter name 't' is too short,
                             expected at least 3 characters [readability-identifier-length]
（同 25 行目・37 行目）
```

`docs/cpp-conventions.md` §2.2 の定義をそのまま写した結果、requires 式の仮引数が `t` だった。

**対処: `source` / `sink` へ改名した。** `src/core/Concepts.hpp` が同じ理由で
`source` / `format` としており、既存コードの慣習に合わせた形になる
（`agent-protocol.md` §1 の解決順序 3）。
**要求する操作は §2.2 のまま。契約は 1 つも変えていない。** 抑制もしていない。

### 追加・変更したテスト（7 本追加。100 → 107）

| テスト | 期待値 | 結果 |
|---|---|---|
| `FileSource reads back the bytes that were written` | 読み出した `QByteArray` が元の内容と**バイト一致**（NUL を含む 17 バイト） | pass |
| `FileSource reports NotFound for a missing path` | `IoError::NotFound` | pass |
| `FileSource reports OpenFailed for a directory` | `IoError::OpenFailed` | pass |
| `FileSource reports TooLarge above the limit` | 上限 8 バイトのとき、**8 バイトは通り**、9 バイトで `IoError::TooLarge` | pass |
| `FileSink writes the exact bytes` | 書き出したファイルの内容が**バイト一致** | pass |
| `FileSink reports WriteFailed for a missing directory` | 存在しない親ディレクトリで `IoError::WriteFailed` | pass |
| `FileSink leaves no partial file when it fails` | 失敗後にそのパスが存在せず、**出力先ディレクトリの一覧が失敗前と一致**（一時ファイルもディレクトリも残さない） | pass |

上限のテストは**境界の両側**を見ている。「ちょうど上限は通る」を固定しないと、
`>` と `>=` の取り違えが検出できない。

`IoError` は `DestinationExists` を除く 4 値にテストが揃った。
**`DestinationExists` は T3（衝突ポリシー）で発生させる。**

### 本番型の concept 適合（T1 で約束した宿題のうち 2 件）

| 契約 | 置き場所 |
|---|---|
| `static_assert(ByteSource<FileSource>)` | `FileSource.cpp` |
| `static_assert(ByteSink<FileSink>)` | `FileSink.cpp` |

ヘッダではなく `.cpp` に置いたのは、`IoConcepts.hpp` を利用者へ押し付けないため。
`src/core/CapabilityTable.cpp` と同じ置き方。
**残るは `ProgressSink<JobRunnerBridge>`（T6）。**

### リンク構成の実測（io が STATIC になった後）

```
1 libkatachi_core.a
1 libkatachi_io.a
1 QtCore.framework
1 QtGui.framework
```

**`QtWidgets.framework` は無い。** 各アーカイブも 1 回ずつで、
T1 で解消した重複リンクが `Qt6::Core` の明示リンク追加によって再発していないことも確認できた。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **107 / 107 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（**対象 7 ファイル**） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **107 / 107 pass** |

### 推測で埋めた箇所

**なし。** `QSaveFile` の「commit するまで目的のパスに現れない」挙動は、
`FileSink leaves no partial file when it fails` で実際に確認している。

### 残課題 / 次にやること

1. **T3（衝突ポリシー）に着手する。** `CollisionPolicy`（`Overwrite` / `Skip` / `Rename`、**既定 `Skip`**）を
   `src/io` に置き、`FileSink` へ適用する（ADR-0009）。
   ここで `FileSink` の構築引数が「ディレクトリ + 希望する名前 + ポリシー」に変わり、
   `write()` の `const` が外れる。`IoError::DestinationExists` のテストもここで入る。
2. `indexed.png` を `fixtures_test.cpp` の大きさ検査へ足すかどうかの判断（T1.5 で報告）。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T3 完了（衝突ポリシー）。並列実行時の競合を 1 件発見（提案あり）

### 実施内容

ADR-0005 が Phase 2 へ送った宿題を果たした。`CollisionPolicy` を `src/io` に置き、
`FileSink` が**書く直前に**適用するようにした（ADR-0009）。
TDD の順序を守り、実装前に意図した理由での失敗
（`'io/CollisionPolicy.hpp' file not found`）を確認した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/io/CollisionPolicy.hpp/.cpp` | `CollisionPolicy`（既定 `Skip`）、`OutputDirectory` / `OutputFileName`、`resolveCollision()` |
| `tests/io/collision_policy_test.cpp` | 実行時テスト 7 本 + `static_assert` 3 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `src/io/FileSink.hpp/.cpp` | 構築引数を「ディレクトリ + 名前 + ポリシー」へ。`resolvedPath()` を追加。`write()` の `const` を外した |
| `src/io/CMakeLists.txt` | `CollisionPolicy.cpp/.hpp` を追加 |
| `tests/io/file_io_test.cpp` | `FileSink` の新しい構築引数に追随（3 本） |
| `tests/CMakeLists.txt` | `io/collision_policy_test.cpp` を追加 |

**削除**: なし

### 設計上の判断

**`OutputDirectory` / `OutputFileName` の強い型を導入した。**
`FileSink(QString directory, QString fileName, ...)` は同型の引数が隣接し、
`bugprone-easily-swappable-parameters` に触れる。それ以上に、
**取り違えると意図しない場所へファイルを書く。**
Phase 1 の T6 で `NamePattern` / `NameExtension` を入れたのと同じ考え方
（`docs/spec-core.md` §2.1 の「強い型付き文字列」）で型を分けた。
**リンタを黙らせるためではなく、既存方針の適用である。**

**`resolvedPath()` は `ByteSink` concept に含めない。** concept が要求する操作は
原則 4 個以下（`docs/cpp-conventions.md` §2.6）であり、この値が要るのは
具象型を知っている呼び出し側（T6 の `JobRunnerBridge`）だけのため。

**`Rename` は拡張子を必ず維持する。** 末尾のドットで基底名と拡張子に分け、
その間に `_N` を挟む（`photo.png` → `photo_1.png`）。拡張子を落とすと出力形式が分からなくなる。

### 承認された計画からの変更（申告）

**1. `resolveCollision()` に上限を引数で注入できるようにした。**
計画のテストは「上限（10000）まで埋まっていれば `WriteFailed`」だったが、
**10000 個のファイルを作るテストは CI で重く、特に Windows では数十秒かかりうる。**
上限を引数（既定 `defaultMaxRenameAttempts` = 10000）にして、テストは上限 3 で同じ経路を通す。
`FileSource` の `maxBytes` と同じ考え方であり、そちらは計画の時点で承認済み。
**既定が 10000 であることは `static_assert` で固定した。**

**2. `FileSink::write()` の `const` を外し、構築引数を変えた。**
いずれも T2 の報告で「T3 で変わる」と予告したとおり。

### 追加・変更したテスト（7 本追加。107 → 114）

| テスト | 期待値 | 結果 |
|---|---|---|
| `the default collision policy is Skip` | `defaultCollisionPolicy == Skip`（`Overwrite` でない）。ポリシーを省略した `FileSink` が既存ファイルを**上書きしない** | pass |
| `no collision keeps the requested name` | 既存が無ければ要求どおりの名前。`resolvedPath()` が一致 | pass |
| `Overwrite replaces the existing file` | 内容が置き換わり、**余計なファイルを作らない**（ディレクトリの項目数 1） | pass |
| `Skip leaves the existing file untouched` | `IoError::DestinationExists`。既存の内容が**変わらず**、**別名でこっそり書いてもいない**（一覧が一致） | pass |
| `Rename appends _1 when the name is taken` | `photo_1.png` に新しい内容。**`photo.png` は触らない。拡張子も落とさない** | pass |
| `Rename continues to _2` | `photo.png` と `photo_1.png` があるとき `photo_2.png` | pass |
| `Rename gives up after the limit` | 上限 3 で埋まっていれば `IoError::WriteFailed`。**無限ループしない** | pass |

`static_assert`: `defaultCollisionPolicy == Skip` / `!= Overwrite` / `defaultMaxRenameAttempts == 10000`。

**`IoError` の全 5 値にテストが揃った**（`NotFound` / `OpenFailed` / `WriteFailed` / `TooLarge` /
`DestinationExists`）。`docs/phases.md` §2.2 のエラー種別テストを io 層でも満たした。

### 提案: `Rename` は並列実行で競合する（停止条件 9。実装していない）

**実装中に気づいた。T3 の時点では並列実行が無いため実害は無いが、
T6 で `QtConcurrent` を入れた瞬間に実バグになる。**

`resolveCollision()` が候補名を選んでから `QSaveFile::commit()` が完了するまでの間に、
**別のワーカーが同じ候補名を選べる。**

```
worker A: photo_1.png は空き -> 選ぶ
worker B: photo_1.png は空き -> 選ぶ    <- A はまだ commit していない
worker A: commit
worker B: commit                        <- A の出力を上書きする
```

`Skip` と `Overwrite` には起きない（`Skip` は書かない、`Overwrite` は上書きが仕様どおり）。
**`Rename` だけが「衝突を避ける」という約束を破る。**

対処案。

| 案 | 内容 | 評価 |
|---|---|---|
| **A（推奨）** | `JobRunnerBridge` が持つミューテックスで**名前の予約だけ**を直列化する。変換本体は並列のまま | 実装が小さく、失敗時の後始末が不要。直列化されるのは実在確認の数マイクロ秒だけ |
| B | `FileSink` が候補名を `QIODevice::NewOnly` で**原子的に予約**してから書く | OS 任せで直列化が不要。ただし書き出しに失敗したとき、予約した空ファイルの後始末が要る（「失敗しても部分ファイルを残さない」を自分で壊しかねない） |
| C | バッチ開始時に出力名を全件配り、実在確認を 1 回で済ませる | 1000 件で main thread が固まる（ADR-0009 が避けた形そのもの） |

**A を推奨する。T6 で実装してよいか判断を仰ぐ。**
なお、外部のプロセスが同時に同じディレクトリへ書く場合の競合は、
どの案でも完全には塞げない（ファイルシステム越しの排他が要る）。**そこまでは追わない。**

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **114 / 114 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 8 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **114 / 114 pass** |

clang-format は新規テストの 1 箇所で違反を出したため整形してから再実行した（対象は今回のファイルのみ）。

### 推測で埋めた箇所

**なし。** `QFileInfo::completeBaseName()` / `suffix()` の分割位置は、
`photo_1.png` が実際に生成されることをテストで確認している。

### 残課題 / 次にやること

1. **上記「`Rename` の並列競合」への対処方針の判断。** T6 で塞ぐ前提で A を推奨。
2. **T4（`JobRunner<Sink, Progress>`）に着手する。** ヘッダのみのテンプレート。
   Qt のイベントループもファイルシステムも使わずにテストする。
3. `indexed.png` を `fixtures_test.cpp` の大きさ検査へ足すかどうかの判断（T1.5 で報告）。
4. `.serena/` は未追跡のまま。
5. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。
