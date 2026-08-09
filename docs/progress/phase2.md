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

---

## 2026-08-09 — `Rename` の並列競合について案 A の承認を得た（文書のみ）

### 実施内容

T3 で報告した「`Rename` が並列実行で競合する」件について、**案 A の承認を得た。**
**T6 まで決定が失われないよう、ADR-0009 に追補として書いた。** コードは変更していない。

### 変更ファイル

**変更**: `docs/adr/0009-collision-policy.md`（追補「並列実行では名前の予約を直列化する」）

**追加 / 削除**: なし

### 決定の内容と、書きながら直した誤り

`JobRunnerBridge` が `QMutex` と「このバッチで予約済みの出力パス」の集合を持ち、
**ロックの中では名前の決定と予約だけを行う。**

1. ロックを取る → 2. `resolveCollision()`（予約済みの集合も「埋まっている」として扱う）
→ 3. 得たパスを予約集合へ入れる → 4. ロックを放す → 5. **ロックを持たずに**書き出す

**最初に書いた追補では「予約から `commit()` までを 1 つの臨界区間に入れる」としていたが、
これは誤りだった。** それでは書き出し全体が直列化され、「変換本体は並列のまま」という
案 A の前提を自分で壊す。**コミット前に気づいて直した。**

正しくは、ロックの外で書けるようにするために**予約済みの集合**が要る。
集合が無ければ「まだ実在しない」ので 2 つのワーカーが同じ名前を選び、
ロックを取っても競合は消えない。

### T6 への帰結（実装時に見落とさないため）

- `resolveCollision()` に「予約済みの集合」を渡す引数を足す
- この機構は**同一バッチ内で 2 つの入力が同じ出力名になる場合**にも効く。
  `Skip` は 2 件目を正しくスキップし、`Rename` は 2 件目へ別の番号を割り当てる
- **外部プロセスとの競合は塞がない**（ファイルシステム越しの排他が要るため対象外）
- 並列実行で `Rename` が同じ名前を選ばないことをテストで確認する

### 追加・変更したテスト

**なし。** 文書のみのため。テストは T6 で追加する。

### 品質ゲートの実行結果

**未実行。** コードを 1 行も変更していないため（`docs/adr/` の追記のみ）。
直前の T3 で 6 本すべて exit 0（114 / 114 pass）を確認済みで、その状態から
ソースは変わっていない。

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **T4（`JobRunner<Sink, Progress>`）に着手する。**
2. `indexed.png` を `fixtures_test.cpp` の大きさ検査へ足すかどうかの判断（T1.5 で報告）。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T4 完了（`JobRunner<Sink, Progress>`）

### 実施内容

1 件の変換ジョブを実行するテンプレートを実装した。**ヘッダのみで `.cpp` を作っていない。**
TDD の順序を守り、実装前に意図した理由での失敗
（`'io/JobRunner.hpp' file not found`）を確認した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/io/JobRunner.hpp` | `JobStatus` / `JobFailure` / `JobItem` / `JobOutcome` / `BatchCounter` / `outputFileNameFor()` / `JobRunner<Sink, Progress>` |
| `tests/io/test_doubles.hpp` | テストダブルの共有ヘッダ（T1 の 3 種を移し、`FailingSink` を追加） |
| `tests/io/job_runner_test.cpp` | 実行時テスト 8 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `src/io/CMakeLists.txt` | `JobRunner.hpp` を追加 |
| `tests/io/io_concepts_test.cpp` | ダブルの定義を共有ヘッダへ移動し、`FakeProgress` の記録方法の変更に追随 |
| `tests/CMakeLists.txt` | `io/job_runner_test.cpp` を追加 |

**削除**: なし

### 承認された計画からの変更（申告。重要）

**命名を `runOne()` の中から外へ出した。**

計画では `runOne records a naming failure with its reason`（空パターンで
`NamingError::EmptyPattern`）を T4 のテストに挙げていた。実装に入って**設計の矛盾に気づいた。**

- `runOne(Source&, Sink&, const JobItem&)` の `Sink` は**呼び出し側が構築済み**である
- `FileSink` は「ディレクトリ + ファイル名 + ポリシー」で構築される（ADR-0009）
- つまり**出力ファイル名は `runOne()` を呼ぶ前に決まっていなければならない**
- したがって `runOne()` の中で名前を決めても、その名前を使う先が無い

さらに ADR-0009 の追補（案 A）で、**名前の決定と予約は Bridge がロックの中で行う**と決めた。
命名は `runOne()` の外に置くのが正しい。

**対処: 純粋関数 `outputFileNameFor(const JobItem&)` を `JobRunner.hpp` に置き、
命名のテストをそちらへ移した。** 期待値（空パターン → `NamingError::EmptyPattern`）は変えていない。
正常系のテストも 1 本足した（`{name}_{index:03}.{ext}` → `photo_001.png`）。

**テストダブルを共有ヘッダへ切り出した。** T1 で `tests/io/io_concepts_test.cpp` の
無名名前空間に置いたが、T4 でも使うため `tests/io/test_doubles.hpp` へ移した
（T1 のコメントに「T4 でも使う」と書いていたもの）。`FailingSink` を追加し、
`FakeProgress` は呼ばれた `(done, total)` の列を記録するようにした。

### 設計上の判断

**`JobItem` も `JobOutcome` もバイト列を持たない。** `QFuture` は全件の結果を保持するため、
出力バイト列を載せると 1000 件分がメモリに残る（ADR-0008）。
入力は `ByteSource` から必要になった時点で読む。

**`JobOutcome::status` の既定は `Failed`。** 入れ忘れが「成功」に見えないようにするため。

**`JobOutcome::outputPath` は呼び出し側（T6 の Bridge）が入れる。**
`FileSink::resolvedPath()` は `ByteSink` concept の外にあり（ADR-0009）、
テンプレートの `runOne()` からは触れない。案 A では Bridge が名前を決めるため、
Bridge は自分が決めたパスをそのまま入れられる。

**キャンセルされた件は進捗に数えない。** 処理していないため。
失敗とスキップは数える（1 件終わったことに変わりはない）。

**メンバは参照ではなくポインタ。** `cppcoreguidelines-avoid-const-or-ref-data-members` による。
いずれも非所有で、寿命は呼び出し側が保証する。

### 提案: 出力の拡張子が `.jpeg` になる（実装していない）

`outputFileNameFor()` は拡張子を `FormatId` の代表名から作る。
**文字列リテラルを書かないという原則には合っているが、`jpeg` の代表名は `jpeg` のため
出力が `photo.jpeg` になる**（ADR-0006 で `jpg` / `jfif` を `jpeg` へ畳んだため）。

`.jpg` を期待する利用者は多いと思われる。ただし**別名のうちどれを選ぶかの根拠が指示書に無い。**
`CapabilityTable::extensions` は別名の和集合（昇順）であり、そこから 1 つ選ぶ根拠も無い。

**T8（`SettingsPanel`）で「拡張子の選択」を設けるかどうか、判断を仰ぐ。**
現状は代表名のままとし、勝手に決めていない。

### 追加・変更したテスト（8 本追加。114 → 122）

| テスト | 期待値 | 結果 |
|---|---|---|
| `runOne converts and writes through the sink` | Sink が受け取ったバイト列が `core::convert()` の出力と**完全一致** | pass |
| `runOne reports progress once per item` | 10 件流すと `onProgress` が **10 回**、`done` が 1..10 の昇順、`total` は 10 で据え置き | pass |
| `runOne skips work when already cancelled` | 状態 `Cancelled`。**Sink が呼ばれず、進捗も数えない** | pass |
| `runOne records a decode failure with its reason` | 状態 `Failed`、理由 `ConvertError::DecodeFailed`。Sink は呼ばれない。**進捗は 1 件進む** | pass |
| `runOne records a sink failure with its reason` | 状態 `Failed`、理由 `IoError::WriteFailed` | pass |
| `runOne carries the converter warnings` | アルファ有り → jpeg / `Preserve` で `AlphaFlattenedFallback` が **1 件** | pass |
| `outputFileNameFor builds the name from the pattern and the target format` | `{name}_{index:03}.{ext}` → **`photo_001.png`** | pass |
| `outputFileNameFor reports a naming failure with its reason` | 空パターンで `NamingError::EmptyPattern` | pass |

**テスト自身はファイルを 1 つも読み書きしない。** 入力画像はその場でメモリ上に符号化して作る。
Qt のイベントループも使わない。`docs/cpp-conventions.md` §2.3 が
`JobRunner` をテンプレートにした理由を、テストの形で満たしている。

### 完了条件の確認

| 条件 | 結果 |
|---|---|
| `JobRunner.hpp` に `.cpp` が無い | **確認済み**（`src/io/` に `JobRunner.cpp` は存在しない） |
| テストが Qt のイベントループもファイルも使わずに green | **達成**（8 本とも） |

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **122 / 122 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 8 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **122 / 122 pass** |

**clang-tidy はまだ `JobRunner.hpp` に届いていない。** ヘッダのみで、`src/*.cpp` の
どれからも include されていないため。**T6 で `JobRunnerBridge.cpp` が include すると届く。
届いた結果として新たな指摘が出ないか確認する**（T2 で io に届いたときと同じ）。

### 推測で埋めた箇所

**なし。** 命名を外へ出す判断は、`FileSink` の構築引数（T3 で実装済み）と
ADR-0009 追補から導いた。

### 残課題 / 次にやること

1. **T5（`MemoryBudget`）に着手する。** ADR-0008 のバイト予算。
2. 上記「出力の拡張子が `.jpeg` になる」件の判断（T8 の範囲）。
3. `indexed.png` を `fixtures_test.cpp` の大きさ検査へ足すかどうかの判断（T1.5 で報告）。
4. `.serena/` は未追跡のまま。
5. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — `indexed.png` を大きさ検査へ追加（T1.5 で報告した食い違いの解消）

### 実施内容

T1.5 で報告した「`docs/progress/phase1.md` の T5 追補は `indexed.png` を大きさ検査へ
追加したと書いているが、実際のコードには無い」件について、**追加の指示を得たので直した。**

T1.5 の範囲外だったため当時は直さず報告に留めていた（「ついでに」を避けた）。
**単独の変更として 1 コミットにする。**

### 変更ファイル

**変更**: `tests/core/fixtures_test.cpp`（`every fixture stays under the 50KB limit` の一覧）

**追加 / 削除**: なし

### 生成器と検査対象が一致したことの確認

足し忘れが再発しないよう、**生成器が作るファイル名と検査一覧を突き合わせた。**

```
=== 生成器が作るフィクスチャ ===        === 大きさ検査の一覧 ===
gradient_alpha.png                      gradient_alpha.png
gradient_rgb.png                        gradient_rgb.png
icon.ico                                icon.ico
indexed.png                             indexed.png
not_an_image.bin                        not_an_image.bin
oriented.tiff                           oriented.tiff
with_icc.png                            with_icc.png
with_text.png                           with_text.png
```

**8 件対 8 件で完全に一致。** 一覧の先頭に「生成器が作る全 8 件を並べる」旨と、
足し忘れると 50KB 制限がその 1 件だけ検査されないまま通ることをコメントで残した。

### `docs/progress/phase1.md` の扱い

**書き換えていない。** 同ファイルの冒頭が「誤りを見つけた場合も遡って修正せず、
新しい日付で訂正を追記する」と定めており、Phase 1 は既にクローズ・マージ済みのため。
訂正の記録は T1.5 のエントリと本エントリ（Phase 2 の記録）に残す。
**Phase をまたぐ変更を 1 コミットに混ぜないため**（`agent-protocol.md` §6）、
`phase1.md` への追記は行わなかった。必要であれば別途指示を仰ぐ。

### 追加・変更したテスト

**新規の追加は無し。** 既存の `every fixture stays under the 50KB limit` の
検査対象が 7 件 → **8 件**になった。テスト総数は 122 のまま変わらない。

期待値（各フィクスチャについて `exists()` / `size() > 0` / `size() < 50KB`）は変えていない。
`indexed.png` は 203 バイトで通る。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **122 / 122 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **122 / 122 pass** |

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **T5（`MemoryBudget`）に着手する。** ADR-0008 のバイト予算。
2. 出力の拡張子が `.jpeg` になる件の判断（T4 で報告。T8 の範囲）。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T5 完了（`MemoryBudget`）

### 実施内容

ADR-0008 のバイト予算を実装した。`docs/phases.md` §5.3 が
「Phase 2 着手時に決める」としていた項目の実装側。
TDD の順序を守り、実装前に意図した理由での失敗
（`'io/MemoryBudget.hpp' file not found`）を確認した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/io/MemoryBudget.hpp/.cpp` | `estimateJobBytes()` / `MemoryBudget` / `MemoryBudget::Reservation`（RAII） |
| `tests/io/memory_budget_test.cpp` | 実行時テスト 6 本（うち 1 本は別スレッドで待ちを確認） |

**変更**

| ファイル | 内容 |
|---|---|
| `src/io/CMakeLists.txt` | `MemoryBudget.cpp/.hpp` を追加 |
| `tests/CMakeLists.txt` | `io/memory_budget_test.cpp` を追加 |

**削除**: なし

### 設計上の判断

**単位を MiB にした。** `QSemaphore` は `int` で数えるため、バイトのまま渡すと
総量を大きくしたときに溢れる。1 GiB は `int` に収まるが、収まることに依存したくない。

**見積りが総量以上なら「全量を確保」に丸める。** これが ADR-0008 の「単独実行」。
判定を切り上げ計算より**先**に置いたのは、`estimateJobBytes()` が
「寸法が分からない」を `qint64` の最大値で表すため。後に置くと
`bytes + budgetUnitBytes - 1` が溢れる。

**`Reservation` は move のみ。** move 後の元オブジェクトは `owner_` を `nullptr` にして
**二重に返さない。** これをテストで固定した。

**`peakUnits()` を持たせた。** T10 の受け入れ基準
「同時確保量の最大値が予算を超えない」を検証するため。CAS で最大値を更新する。

### 承認された計画からの変更（申告）

**見積りのテストの期待値を ADR-0008 の式に合わせた。**

計画（承認時）の期待値は `ファイルサイズ + 4*64*64 + ファイルサイズ` だったが、
**その後 T0 で係数 16 への変更を承認いただいている**（`src/core/Converter.cpp` の
アルファ合成で画像が 4 枚同時に生存するため）。テストは ADR-0008 の式

```
estimate = 2 * fileSize + 16 * max(sourcePixels, resizeBoundPixels)
```

をそのまま期待値にした。**係数を変えた承認は T0 で得ているが、
計画のテスト期待値の記述は更新していなかったため、ここで明示しておく。**

### 追加・変更したテスト（6 本追加。122 → 128）

| テスト | 期待値 | 結果 |
|---|---|---|
| `the budget admits jobs up to its total` | 総量 4 単位で 2+2 が同時に通り、`usedUnits()` と `peakUnits()` が 4 | pass |
| `the budget blocks the third job until one returns` | 4 単位を使い切った状態で 3 件目は**通らない**。1 件返すと通る | pass |
| `an oversized job runs exclusively` | 総量の 250 倍の見積りでも**デッドロックせず**、確保量は総量ちょうど。抜けると 0 に戻る | pass |
| `the guard releases on every path` | スコープを抜けると 0。**move 後の元ガードは二重に返さない** | pass |
| `the estimate follows the ADR-0008 formula` | `2*1000 + 16*64*64`。拡大リサイズでは出力側の寸法、縮小では入力側のまま | pass |
| `an unknown size is treated as the whole budget` | 無効な `QSize` は総量へ丸められ、確保は総量ちょうど | pass |

**待ちのテストは空振りしない形にした。** 別スレッドが `acquire()` に入ったことを
フラグで確認してから「まだ通っていない」を判定する。入る前に判定すると、
スレッドが起動していないだけで通ってしまう。

### フレーキーでないことの確認

待ちを含むテストは時間に依存するため、**予算関連のテストだけを 5 回連続で実行した。**
5 回とも exit 0。ASan / UBSan 版でも green。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **128 / 128 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 9 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **128 / 128 pass**（並行確保のテストを含む） |

**clang-tidy の指摘 1 件と対処（抑制なし）**

```
MemoryBudget.hpp:25:43: error: performing an implicit widening conversion to type 'const qint64'
                               of a multiplication performed in type 'int'
                               [bugprone-implicit-widening-of-multiplication-result]
```

`1024 * 1024` を `int` で計算してから `qint64` へ広げていた。`1024LL * 1024` に直した。
**この値は溢れないが、指摘自体は正しい**（同じ書き方で総量を大きくすると溢れる）。

### 推測で埋めた箇所

**なし。** 係数 16 は `src/core/Converter.cpp` を読んで数えた値であり、
「寸法が分からないときの扱い」は ADR-0008 の決定に従った。

### 残課題 / 次にやること

1. **T6（`JobRunnerBridge`）に着手する。** Phase 2 で最も大きいタスク。
   `QtConcurrent::mapped` / `QFutureWatcher` / 200ms の進捗間引き / キャンセル /
   **ADR-0009 追補の名前予約（案 A）**。
   併せて、T0 で「未確認」とした 3 点を実測して記録する
   （Qt 6.8 の `QThreadPool*` 版 / `Qt6::Concurrent` の入手 / キャンセル後の `finished`）。
2. 出力の拡張子が `.jpeg` になる件の判断（T4 で報告。T8 の範囲）。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T6 完了（`JobRunnerBridge`）。並列実行・キャンセル・進捗の間引きが揃った

### 実施内容

バッチ実行の司令塔を実装した。Phase 2 で最も大きいタスク。
`QtConcurrent::mapped` による並列実行、200ms の進捗間引き、キャンセルの二段構え、
**ADR-0009 追補（案 A）の名前予約**を入れた。
TDD の順序を守り、テストを先に書いてから実装した。

### T0 で「未確認」とした 4 点の結果

計画の時点で「推測で進めない」として保留した項目を実測した。

| # | 事項 | 結果 |
|---|---|---|
| 1 | `Qt6::Concurrent` が入手できるか | **ローカルでは qtbase 同梱で `find_package` が通った。** CI（Qt 6.8.3 / `aqtinstall`）は**未確認**。push して確認する |
| 2 | `QtConcurrent::mapped(QThreadPool*, ...)` が Qt 6.8 にあるか | **ローカル 6.11.1 では動作した。** 6.8 は**未確認**。CI で確認する |
| 3 | キャンセル後に `finished` が来るか | **来る。** テスト `cancel stops the batch and the app returns to idle` が `finished` を待って通った。**これが来なければ UI が実行中のまま戻れなかった** |
| 4 | 実行中の項目がキャンセルで中断されるか | 公式ドキュメントに記述が無い状態は変わらない。**「中断されない」前提の二段構えを維持した** |

**1 と 2 はローカルの結果を CI の結果として報告しない。** Phase 1 の `qtimageformats` は
まさにこの形（ローカル green / CI のみ失敗）で現れた。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/io/JobRunnerBridge.hpp/.cpp` | `BatchRequest` と `JobRunnerBridge`（QObject アダプタ） |
| `tests/io/job_runner_bridge_test.cpp` | 実行時テスト 7 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `CMakeLists.txt` | `find_package` に `Concurrent` を追加 |
| `src/io/CMakeLists.txt` | `AUTOMOC` を有効化、`JobRunnerBridge` を追加、`Qt6::Concurrent` / `Qt6::Gui` をリンク |
| `src/io/CollisionPolicy.hpp/.cpp` | `resolveCollision()` に**予約済み集合**の引数を追加（ADR-0009 追補） |
| `src/io/JobRunner.hpp` | `std::move` の除去（clang-tidy の指摘） |
| `tests/io/collision_policy_test.cpp` | 引数追加への追随 |
| `tests/CMakeLists.txt` | `io/job_runner_bridge_test.cpp` を追加 |

**削除**: なし

### 設計上の判断

**ワーカーはシグナルを出さない。** `onProgress()` は atomic を更新するだけで、
発火は main thread の `QTimer(200ms)` が行う。`docs/spec-core.md` §7 の
「進捗表示は 200ms 以下の間隔で更新しない」を**構造的に**満たす。
1000 件でシグナルが 1000 回飛ぶ設計を最初から採らない。

**結果も同じタイマでまとめて出す。** `QFuture::resultCount()` / `resultAt()` から
新しく揃った分だけ取り出して `resultsReady` を 1 回で送る。
ワーカーが別の入れ物へ積む方式より経路が 1 本で済み、ロックも増えない。

**`QtConcurrent::mapped` へは `std::move(items)` で渡す。**
左辺値で渡すと `Sequence&&` が参照に束縛され、ローカルの `items` を指したまま
ワーカーが走る（`start()` を抜けた瞬間に宙に浮く）。

**デストラクタで `waitForFinished()` する。** ワーカーは `this` を触るため、
終わってからでないと壊せない。

**走っている間の再投入は無視する。** `start()` は `isRunning()` なら何もしない。

**Sink のポリシーは `Overwrite`。** 名前は既に予約済みで、
`FileSink` の中で再度衝突解決をする必要が無いため。

### ADR-0009 追補（案 A）の実装

`resolveCollision()` に「このバッチで予約済みの出力パス」の集合を渡せるようにし、
**実在するものと同じに扱う。** `JobRunnerBridge::reservePath()` が
`QMutex` の中で「解決 → 予約集合へ追加」までを行い、**書き出しはロックの外**で並列に走る。

臨界区間はファイル数個分の実在確認だけ。変換も書き出しも並列のまま。

**テストで固定した**（`parallel renames never collide`）。
50 件すべてが同じ出力名 `out.png` を要求する状況で、
**50 件が別々のファイルになる**ことを確認している。予約が無ければ、
まだ commit されていない出力を実在確認では見つけられず、複数のワーカーが同じ名前を選ぶ。

### clang-tidy の指摘 11 件と対処（すべてコード側。抑制ゼロ）

`JobRunnerBridge.cpp` が入ったことで、ゲートの対象が 9 本 → **10 本**になり、
`JobRunner.hpp` を含む io のヘッダ群へ初めて届いた。予告どおり指摘が出た。

| 指摘 | 対処 |
|---|---|
| `performance-move-const-arg`（`JobFailure` は trivially copyable で `std::move` が無効） | `std::move` を外した |
| `misc-include-cleaner` × 6（`QObject` / `Qt::CoarseTimer` / `std::memory_order_relaxed` / `QtConcurrent::mapped` / `core::NamingError` / `emit`） | 直接 include を追加 |
| `misc-const-correctness`（`QImageReader reader`） | `const` を付けた |
| `readability-redundant-casting`（`resultCount()` は既に `int`） | キャストを外した |
| `readability-redundant-access-specifiers`（`public slots:` が直前の `public:` と重複） | **`slots` を書かない形にした。** 接続は関数ポインタ形式に限る規約（`cpp-conventions.md` §1）のため、moc へスロット登録する必要が無い |

**`emit` の提供ヘッダで 2 回外した。** 最初 `<QObject>` で足りると考え、次に
`<QtCore/qobjectdefs.h>` を入れたが、いずれも解消しなかった。
`grep` で実際の定義位置を調べたところ **Qt 6 では `qtmetamacros.h`** だった。
**推測で 2 回外し、実測で確定させた。**

### 追加・変更したテスト（7 本追加。128 → 135）

| テスト | 期待値 | 結果 |
|---|---|---|
| `the bridge finishes a small batch` | 5 件すべて `Succeeded`、`finished` が **1 回だけ**、出力ファイルが 5 件 | pass |
| `the bridge throttles progress to 200ms` | タイマ間隔が **200**。200 件でも進捗シグナルは **200 回未満** | pass |
| `cancel stops the batch and the app returns to idle` | 300 件を開始直後にキャンセル → **出力が入力件数未満**、`finished` が届き `isRunning()` が false | pass |
| `the bridge can start a second batch after a cancel` | キャンセル後に再投入して**全件成功**（状態が残っていない） | pass |
| `failed jobs stay in the results with a reason` | 壊れた 1 件が `Failed` + `ConvertError::DecodeFailed` で**残り**、他 2 件は成功 | pass |
| `the conversion runs off the main thread` | 変換したスレッドが main と**異なる**。UI へ届くシグナルは main から出る | pass |
| `parallel renames never collide` | 50 件が同じ名前を要求しても**50 件別々のファイル**になる（ADR-0009 追補） | pass |

### フレーキーでないことの確認

並行と時間に依存するため、**ブリッジのテストだけを 5 回連続で実行した。** 5 回とも exit 0。
ASan / UBSan 版でも 135 / 135 green。

### 本番型の concept 適合（T1 の宿題を完了）

`static_assert(ProgressSink<JobRunnerBridge>)` を `JobRunnerBridge.cpp` に置いた。
**これで `ByteSource` / `ByteSink` / `ProgressSink` の 3 つとも本番型の `static_assert` が揃った**
（`docs/phases.md` §2.2 の concept 適合テスト）。

### 差分規模（停止条件 8。計画時に申告済み）

**10 ファイル / +713 行 / -11 行。** 計画で「T6 は単独で 400 行を超える見込み」と
申告したとおりになった。内訳は本体 371 行、テスト 304 行、CMake と既存の追随が残り。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **135 / 135 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（**対象 10 ファイル**） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **135 / 135 pass** |

### 推測で埋めた箇所

**なし。** `emit` の提供ヘッダは 2 回外したうえで `grep` で確定させた。
キャンセル後の `finished` はテストで実測した。
**`Qt6::Concurrent` と `QThreadPool*` 版の Qt 6.8 での可用性は「未確認」のままであり、
ローカルの結果を CI の結果として報告しない。**

### 残課題 / 次にやること

1. **T7（`JobTableModel`）に着手する。** ここで `tests/app/` と `katachi_app_tests`
   （`QApplication` が要るため別実行ファイル）を新設する。
2. **CI で未確認の 2 点を確認する。** `Qt6::Concurrent` の入手と
   `QtConcurrent::mapped(QThreadPool*, ...)` の Qt 6.8 での実在。
   **T7 以降のどこかで push して早めに確かめる方が安全**（Phase 1 は最後にまとめて
   push して 4 ジョブ全滅した）。push の時期について判断を仰ぐ。
3. 出力の拡張子が `.jpeg` になる件の判断（T4 で報告。T8 の範囲）。
4. `.serena/` は未追跡のまま。
5. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — T7 完了（`JobTableModel`）。app 層のテスト実行ファイルを新設

### 実施内容

ジョブ一覧の表を実装した。併せて `tests/app/` と `katachi_app_tests` を新設した。
TDD の順序を守り、テストを先に書いてから実装した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/app/JobTableModel.hpp/.cpp` | `QAbstractTableModel`。列は 入力 / 出力 / 状態 / 理由 |
| `tests/app/main.cpp` | `QApplication` を構築する Catch2 の入口 |
| `tests/app/job_table_model_test.cpp` | 実行時テスト 7 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `src/app/CMakeLists.txt` | `JobTableModel` を追加、`katachi_io` をリンク |
| `tests/CMakeLists.txt` | `katachi_app_tests` の新設（`QT_QPA_PLATFORM=offscreen` 付き） |
| `.clang-tidy` | `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access` を除外（下記） |

**削除**: なし

### 設計上の判断

**行を並べ替えない。** 並列実行では完了順が入力順と一致しないが、
表の並びが勝手に動くと利用者が見ている行が入れ替わる（`docs/spec-core.md` §7 の
「自動スクロール禁止」と同じ趣旨）。結果は `sourcePath` で引いて**その行だけ**を更新する。

**更新は `dataChanged` を行単位で出す。** 全体を `beginResetModel` すると
選択もスクロール位置も飛ぶ。テストで「1 行だけ」を固定した。

**表に無い入力の結果は黙って捨てる。** 行を増やすと並びが崩れる。落ちもしない。

**列は `enum` ではなく `constexpr int`。** `QModelIndex::column()` が `int` を返すため、
`enum class` にすると比較のたびに変換が要る。素の `enum` は
`cpp-conventions.md` §1 と clang-tidy の `cppcoreguidelines-use-enum-class` が禁じる。

**表示文言は app 層にしか置かない**（ADR-0010）。`QObject::tr()` を使い、
`QStringLiteral` は使わない（Qt 6.8 と 6.11 で提供ヘッダが異なる件。Phase 0 の知見）。

### 承認された計画からの追加（申告）

計画の T7 は 5 本のテストだったが、次を足した。

1. **`succeededCount()` / `failedCount()` / `skippedCount()` を持たせた。**
   T9 のステータス行で「成功 n 件 / 失敗 m 件」を出すために要る。テストでも検証した。
2. **成功時の警告を理由列に出す**（テスト `a successful job shows its output and any warning`）。
   ADR-0004 は「変換は成功したが指定どおりには処理できなかった」ことを
   `ConversionOutput::warnings` に載せると決めたが、**UI での行き先が決まっていなかった。**
   理由列に出さないと、この警告は誰にも届かない。
3. **`an unknown outcome is ignored`** を足した。並列実行では、表を作り直した直後に
   前のバッチの結果が届きうる。落ちないことを固定した。
4. **`katachi_app_tests` に `QT_QPA_PLATFORM=offscreen` を設定した。**
   `docs/phases.md` §2.4 は CI について同じ指定をしている。GUI を持たない環境でも走るようにした。

### clang-tidy の指摘 6 件と対処

app に新しい `.cpp` が入ったため、まとめて指摘が出た。

| 指摘 | 対処 |
|---|---|
| `cppcoreguidelines-use-enum-class` / `performance-enum-size`（列の `enum`） | `constexpr int` の定数へ |
| `misc-include-cleaner`（`std::decay_t` / `QStringList` / `emit`） | `<type_traits>` を追加、`QList<QString>` と綴る、`<QtCore/qtmetamacros.h>` を追加 |
| `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access`（`rows_[row]`） | **`.clang-tidy` で除外した**（下記） |

**除外を 1 つ増やしたが、新しい種類の除外ではない。**
`docs/phases.md` §3 は「`cppcoreguidelines-pro-bounds-*` は除外可」と明示しており、
既に同じ枠から 3 つ除外している。今回の 4 つ目はその枠の中にある。
（Phase 1 で承認を得た `bugprone-exception-escape` は枠の外だったため事情が違う。）
`.clang-tidy` に理由をコメントとして残した。**`docs/phases.md` の変更は不要。**

添字が範囲内であることは直前の `QHash` 検索が保証している。
`std::next(begin(), row)` へ書き換える案は、読みやすさを落とすだけと判断した。

### 追加・変更したテスト（7 本追加。135 → 142）

| テスト | 期待値 | 結果 |
|---|---|---|
| `the model exposes one row per job` | 3 行 4 列。入力列は**ファイル名**。実行前は「待機中」で出力も理由も空。見出しが 4 列とも空でない | pass |
| `a failed job keeps its row and shows the reason` | 行は**消えず**、状態が「失敗」、理由が空でない。**他の行は巻き込まれない** | pass |
| `a skipped job is shown as skipped not failed` | `DestinationExists` の行は「スキップ」。失敗と**別の文言**。`failedCount()==1` / `skippedCount()==1` | pass |
| `a successful job shows its output and any warning` | 「成功」、出力列にファイル名、**警告が理由列に出る** | pass |
| `updating one row emits dataChanged for that row only` | `dataChanged` が **1 回**、範囲は**その行のみ**（row 2 → 2） | pass |
| `the model never reorders rows` | 完了順が c → a でも並びは a / b / c のまま。未完了の行は「待機中」 | pass |
| `an unknown outcome is ignored` | 表に無い入力の結果で**行が増えず**、既存の行も変わらない | pass |

**文言そのものを期待値に固定した。** 表示文言は app 層にしか無く、
変えるときはテストも一緒に動くべきで、黙って変わってよいものではない。

### 実行ファイルを分けたことの確認（リンク行の実測）

```
=== katachi_tests ===              === katachi_app_tests ===
libkatachi_core.a                  libkatachi_app.a
libkatachi_io.a                    libkatachi_core.a
QtConcurrent.framework             libkatachi_io.a
QtCore.framework                   QtConcurrent.framework
QtGui.framework                    QtCore.framework
                                   QtGui.framework
                                   QtWidgets.framework
```

**core と io のテストは `QtWidgets` を引いていない。** 1 つにまとめると、
io が Widgets を引いていなくてもテストバイナリ経由で見えてしまい、担保が弱くなる。
どのアーカイブも 1 回ずつで、重複リンクも起きていない。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **142 / 142 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 11 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **142 / 142 pass** |

不変条件スキャナ 7 種も引き続き green。**`INV3B`（app にフォーマット名の文字列リテラルが無い）
が実効性を持った**（`src/app` に初めて文言を持つ `.cpp` が入ったため）。

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **T8（`SettingsPanel`）に着手する。** 出力形式の選択肢を能力表から作る。
   **出力の拡張子が `.jpeg` になる件（T4 で報告）はこのタスクの範囲。**
2. **CI で未確認の 2 点**（`Qt6::Concurrent` の入手 / `mapped` の `QThreadPool*` 版が Qt 6.8 にあるか）。
   push の時期について判断を仰ぎたい。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了。

---

## 2026-08-09 — `phase2` を push。**未確認だった 2 点が CI で確認できた**

### 実施内容

T0 から「未確認」として持ち越していた 2 点を確かめるため `phase2` を push した。
**初回 CI で 4 ジョブすべて success。** Phase 1 の初回 CI が 4 ジョブ全滅だったことを踏まえ、
T7 の時点で早めに確認した。

| 項目 | 内容 |
|---|---|
| run | [31297752976](https://github.com/SilentMalachite/Katachi/actions/runs/31297752976) |
| 差分 | 11 コミット（T0〜T7 + ADR 追補 + フィクスチャ検査） |
| 結果 | **4 ジョブすべて success** |

### 未確認だった 2 点の結果

| # | 事項 | 結果 | 根拠 |
|---|---|---|---|
| 1 | `Qt6::Concurrent` が CI の Qt 6.8.3 で入手できるか | **できる** | `find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent ...)` は **`REQUIRED`** であり、無ければ構成が落ちる。4 ジョブとも構成・ビルドが通った |
| 2 | `QtConcurrent::mapped(QThreadPool*, ...)` が Qt 6.8 に実在するか | **する** | `JobRunnerBridge.cpp` が **macOS / Windows の両方**で Qt 6.8.3 に対してコンパイルされ、ブリッジのテストが両 OS で通った |

**両 OS とも Qt 6.8.3**（`QT_ROOT_DIR` が `.../Qt/6.8.3/macos` と `...\Qt\6.8.3\msvc2022_64`）。
ローカルは 6.11.1 なので、**6.8 での可用性はここで初めて確認できた。**

### CI の結果

| ジョブ | 結果 | テスト |
|---|---|---|
| ビルド + テスト (macOS 14 arm64、Qt 6.8.3) | **success** | **142 / 142 pass**（10.94 秒） |
| ビルド + テスト (Windows 2022 MSVC、Qt 6.8.3) | **success** | **142 / 142 pass**（10.12 秒） |
| clang-format + clang-tidy (macOS、LLVM 22.1.8) | **success** | — |
| ASan + UBSan (macOS) | **success** | **142 / 142 pass**（19.31 秒） |

**CI ログ全 3281 行に `warning:` / `error:` を含む行は 0 件。**

### Windows で通ったことの確認（環境差が出やすい箇所）

Phase 1 では環境差が「ローカル green / CI だけ失敗」という形で出た。
今回**新しく入った並列実行と app 層**が Windows でも通ったことを個別に確認した。

```
Windows:
  Test #118: the bridge can start a second batch after a cancel ... Passed
  Test #120: the bridge throttles progress to 200ms ............... Passed
  Test #123: the conversion runs off the main thread .............. Passed
  Test #136: a skipped job is shown as skipped not failed ......... Passed
  Test #139: the model exposes one row per job .................... Passed
  Test #140: the model never reorders rows ........................ Passed
```

**懸念していたが問題にならなかったこと。**

| 懸念 | 結果 |
|---|---|
| `QThread::idealThreadCount()` の差で並列度が変わり、キャンセルや進捗のテストが落ちる | **落ちなかった。** 両 OS とも 142 / 142 |
| `QSaveFile` の改名が Windows で別挙動になる | **ならなかった** |
| `QApplication` を使う `katachi_app_tests` が CI で起動しない | **起動した**（`QT_QPA_PLATFORM=offscreen` が効いている） |
| 200ms タイマの粒度が Windows で粗く、進捗のテストが落ちる | **落ちなかった** |

### 変更ファイル

**変更**: `docs/progress/phase2.md`（本エントリ）。**コードは変更していない。**

### 品質ゲートの実行結果

**ローカルでは未実行**（コードを変更していないため）。
直前の T7 で 6 本すべて exit 0（142 / 142）を確認済みで、その状態を push した。
**CI が同じ内容に対して 4 ジョブ success を返している。**

### 推測で埋めた箇所

**なし。** 2 点はいずれも CI の実行結果で確定させた。

### 残課題 / 次にやること

1. **T8（`SettingsPanel`）に着手する。**
   **出力の拡張子が `.jpeg` になる件（T4 で報告）はこのタスクの範囲。判断を仰ぐ。**
2. **`.github/workflows/ci.yml` のステップ名が古い。**
   「テスト（不変条件スキャナ 14 本 + smoke 2 本）」のままだが、実際は 142 本ある。
   **本タスクの範囲外なので直していない。** 直すかどうか指示を仰ぐ。
3. `.serena/` は未追跡のまま。
4. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了
   （**CI では通っているが実機未確認**）。

---

## 2026-08-09 — CI のテストステップ名から古い件数を外した

### 実施内容

前エントリで報告した「ステップ名が Phase 0 当時の件数のまま」を、指示を得て直した。
**単独の変更として 1 コミットにした。**

### 変更ファイル

**変更**: `.github/workflows/ci.yml`（`build-and-test` ジョブのテストステップ名）

**追加 / 削除**: なし。**コードは変更していない。**

### 内容

```
- テスト（不変条件スキャナ 14 本 + smoke 2 本）   <- Phase 0 当時の件数
+ テスト（ctest）                                 <- 件数を名前に書かない
```

**件数を名前に書かない方針にした。** テストが増えるたびに古くなるため。
同じ理由をコメントとして `ci.yml` に残した。
`sanitizers` ジョブのステップ名は元から件数を含んでいないため変更していない。

### 品質ゲートの実行結果

**ローカルでは未実行。** `ci.yml` のみの変更で、ソースもテストも 1 行も変えていないため。
**この変更を検証できるのは CI 自身であり、実際に CI で確認した。**

| 確認 | 結果 |
|---|---|
| run | [31297993733](https://github.com/SilentMalachite/Katachi/actions/runs/31297993733) |
| 4 ジョブ | **すべて success** |
| 新しいステップ名がログに出る | **出る**（`テスト（ctest）`） |
| 古いステップ名が残っていない | **0 件** |
| テスト | **142 / 142 pass**（macOS / Windows / ASan の 3 ジョブとも） |
| `warning:` / `error:` を含む行 | **0 件** |

YAML として読めることは push 前にローカルでも確認した（`yaml.safe_load` でジョブと
ステップ名を列挙）。**ただし「読める」ことと「CI が意図どおり動く」ことは別なので、
CI の実行結果をもって確認とした。**

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **T8（`SettingsPanel`）に着手する。**
   **出力の拡張子が `.jpeg` になる件（T4 で報告）はこのタスクの範囲。判断を仰ぐ。**
2. `.serena/` は未追跡のまま。
3. `phase1` ブランチの削除、Windows の実機起動確認はいずれも未了
   （**CI では通っているが実機未確認**）。

---

## 2026-08-09 — `phase1` ブランチの削除依頼。**既に存在しなかった**（何もしていない）

### 実施内容

`phase1` ブランチの削除を指示されたため状態を確認したところ、
**ローカルにもリモートにも存在しなかった。削除操作は行っていない。**

Phase 1 の最終エントリから「`phase1` ブランチの削除。未実施。」を残課題として
引き継いできたが、**実際には PR #2 のマージ時点で既に消えていた**（GitHub の
マージ時自動削除と思われる）。**残課題の記述の方が実態と食い違っていた。**

### 確認した内容

| 確認 | 結果 |
|---|---|
| ローカルのブランチ | `main` / `phase2` のみ |
| `git branch -r` | `origin/main` / `origin/phase2` のみ |
| GitHub の API（`repos/.../branches`） | **`main` と `phase2` の 2 本だけ** |
| PR #2 | `MERGED`（2026-08-09T02:04:32Z）、head=`phase1`、マージコミット `e3289ee` |
| `git log origin/main..origin/phase1` | 参照できない（ブランチが無い）。**Phase 1 の内容は `main` に入っている** |

**Phase 1 の成果が失われていないことは PR #2 のマージコミット `e3289ee` が `main` に
あることで確認できる。**

### 変更ファイル

**変更**: `docs/progress/phase2.md`（本エントリ）。**コードもブランチも変更していない。**

### 追加・変更したテスト

**なし。**

### 品質ゲートの実行結果

**未実行。** リポジトリの状態を確認しただけで、ソースもテストも変更していないため。

### 推測で埋めた箇所

**なし。** 「既に消えていた」ことは GitHub の API とローカルの両方で確認した。
**「削除した」とは報告しない。**

### 残課題 / 次にやること

1. **T8（`SettingsPanel`）に着手する。**
   **出力の拡張子が `.jpeg` になる件（T4 で報告）はこのタスクの範囲。判断を仰ぐ。**
2. `.serena/` は未追跡のまま。
3. **Windows の実機起動確認は Phase 0 から未了**（CI では通っているが実機未確認）。
   `phase1` ブランチの件は本エントリで決着したため、以後の残課題から外す。

---

## 2026-08-09 — T8 完了（`SettingsPanel`）。拡張子を選べるようにした

### 実施内容

変換設定の入力欄を実装した。**T4 で報告した「出力の拡張子が `.jpeg` になる」件について
「`.jpg` を選べるようにする」との判断を得たため、拡張子の選択を設計に入れた。**
TDD の順序を守り、テストを先に書いてから実装した。

### 得られた判断（利用者回答）

| 事項 | 判断 |
|---|---|
| 出力の拡張子が代表名（`jpeg`）になる | **利用者が選べるようにする。** 選択肢は能力表の `extensions`（別名の和集合）から作る |

**既定は代表名のままにした。** 別名のどれを既定にすべきかの根拠は依然として無く、
既定を変えると従来の出力名が黙って変わる。**選択肢を用意し、選ぶのは利用者に委ねる。**

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/app/SettingsPanel.hpp/.cpp` | `QFormLayout` の設定欄。選択肢はすべて能力表から作る |
| `tests/app/settings_panel_test.cpp` | 実行時テスト 7 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `src/io/JobRunner.hpp` | `JobItem::extension` を追加。`outputFileNameFor()` が使う（空なら代表名） |
| `src/io/JobRunnerBridge.hpp/.cpp` | `BatchRequest::extension` を追加し、各 `JobItem` へ配る |
| `tests/io/job_runner_test.cpp` | 拡張子のテスト 1 本を追加 |
| `src/app/CMakeLists.txt` / `tests/CMakeLists.txt` | 新規ファイルの登録 |

**削除**: なし

### 承認された計画からの変更（申告）

**T8 の範囲を超えて io 層に触れた。** `JobItem` と `BatchRequest` に `extension` を足し、
`outputFileNameFor()` の実装を変えた。**拡張子を選べるようにするには、
選んだ値が命名まで届く経路が要る。** T8 の中で完結させると、
`SettingsPanel` が値を持つだけで誰にも渡らない。

**既定の振る舞いは変えていない。** `extension` が空なら従来どおり代表名を使う。
T4 で入れたテストはそのまま通っている。

### 設計上の判断

**選択肢はすべて能力表から作る。** 出力形式は `encodable()`、拡張子は
選択中の形式の `extensions`。**`SettingsPanel` にフォーマット名の文字列リテラルは 1 つも無い**
（INV3B が検査する）。テスト側も形式名を直接書かず、能力表から条件で選んでいる
（環境で対応形式が変わってもテストの意味が保たれる）。

**品質欄は `supportsQuality` に追随して無効化する。** 扱えない形式で値だけ残しても意味が無い。

**合成色（`flattenColor`）の欄は作らなかった。** 色の選択には `QColorDialog` が要り、
ADR-0010 が認めたモーダルの 2 用途（出力先の選択 / 上書きの確認）に当たらない。
**既定の白のままにした。** 必要になればモーダルの許容範囲を ADR で広げてから作る。

**ウィジェットを外へ晒さない。** 値は意味のある単位（`spec()` / `namePattern()` /
`extension()` / `collisionPolicy()`）で返す。テストも T9 の `MainWindow` もこの API 越しに扱う。

**タブ順をこの層で明示した**（`docs/spec-core.md` §7）。ウィンドウ全体の連結は T9 で行う。

### clang-tidy との衝突 1 件と対処

```
SettingsPanel.cpp: initializing non-owner 'QFormLayout *' with a newly created 'gsl::owner<>'
                   [cppcoreguidelines-owning-memory]
```

`cppcoreguidelines-owning-memory` は **Qt の親子オーナーシップと正面から衝突する。**
`docs/cpp-conventions.md` §1 は「Qt の親付き `new` は `app/` 層のみ許可」と
**明示的に許している**が、この検査は局所変数・引数への `new` をすべて咎める。

試した順序。

1. `auto* layout = new QFormLayout(this);` → **指摘される**（局所変数）
2. `setLayout(new QFormLayout);` → **指摘される**（引数）
3. **メンバ初期化子で持つ** → **指摘されない**

**3 を採った。** 他のウィジェットもすべてメンバ初期化子で作っており、
レイアウトだけ別扱いにする理由が無い。**検査を外していない。**
理由はヘッダにコメントとして残した。

**T9 で局所のレイアウトが増えて同じ衝突が頻発するようなら、
`.clang-tidy` から外すことを ADR 付きで提案する。黙って外さない。**

### 追加・変更したテスト（8 本追加。142 → 150）

| テスト | 期待値 | 結果 |
|---|---|---|
| `the format list comes from the capability table` | 項目数が `encodable().size()` と一致。**全項目が能力表で書き出し可能** | pass |
| `the quality control follows supportsQuality` | 品質を扱える形式では有効、扱えない形式では**無効** | pass |
| `the extension list comes from the selected format` | 一覧が `extensions` と一致し**複数ある**。既定は**代表名**。**別名を選べる** | pass |
| `changing the format repopulates the extensions` | 形式を変えると前の拡張子が**残らない**。既定は新しい形式の代表名 | pass |
| `the panel produces the spec it displays` | 画面の値と `spec()` が一致（形式 / 品質 / リサイズ / アルファ / メタデータ / ICC）。**リサイズは外せる** | pass |
| `the default collision policy shown is Skip` | 初期表示が `Skip`。**`Overwrite` でない** | pass |
| `the default naming pattern keeps the source name` | 既定が `{name}.{ext}` | pass |
| `outputFileNameFor uses the chosen extension`（io 側） | 指定が無ければ `photo.jpeg`、`jpg` を指定すると **`photo.jpg`** | pass |

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **150 / 150 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 12 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **150 / 150 pass** |

`invariant.inv3b`（app にフォーマット名の文字列リテラルが無い）も green。
**選択肢を能力表から作る方針が機械検査で守られている。**

### 推測で埋めた箇所

**なし。** 拡張子の既定を代表名のままにしたのは、別名を選ぶ根拠が指示書に無いため。
**勝手に `.jpg` を既定にしていない。**

### 残課題 / 次にやること

1. **T9（`MainWindow` 統合）に着手する。** D&D、開始 / キャンセル、進捗バー、
   ステータス行、ウィンドウ全体のタブ順、`QFileDialog` による出力先選択、
   上書き実行時の確認モーダル。
2. `.serena/` は未追跡のまま。
3. Windows の実機起動確認は Phase 0 から未了（CI では通っているが実機未確認）。
