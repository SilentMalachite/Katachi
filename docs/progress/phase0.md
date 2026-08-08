# Phase 0 — 基盤構築

**追記のみ。既存の記述は書き換えない。** 誤りを見つけた場合も遡って修正せず、新しい日付で訂正を追記する。

---

## 2026-08-09 — 初回セッション / 着手予定（承認待ち）

### 実施内容

実装は未着手。`agent-protocol.md` §7 の 1〜2（通読・不明点の列挙・回答取得・本ファイルの作成）まで。

### 読み込んだファイル

- `CLAUDE.md`（リポジトリ直下）
- `phases.md` / `spec-core.md` / `cpp-conventions.md` / `agent-protocol.md`
  — **いずれもリポジトリ直下にあった。`docs/` 直下ではない**（Phase 0 受け入れ基準に違反した状態）
- `docs/progress/` — 存在しなかった（本ファイルが最初のエントリ）
- `docs/adr/` — 存在しなかった
- `docs/licenses.md` — 存在しなかった（Phase 0 受け入れ基準が要求）

### 環境調査の結果（実測値。推測ではない）

| 項目 | 実測値 | 備考 |
|---|---|---|
| CMake | 4.4.2 | 要件 3.24 以上を満たす |
| Ninja | 1.13.2 | — |
| コンパイラ | Apple clang 21.0.0 (clang-2100.1.1.101) | C++20 可 |
| Qt | `~/Qt/6.11.1/macos`（Widgets / Gui / Concurrent を確認） | ローカルは 6.11。CI は `phases.md` §1.5 に従い 6.8 系 LTS に固定する |
| Qt 画像プラグイン（ローカル実測） | gif, icns, ico, jpeg, macheif, macjp2, pdf, svg, tga, tiff, wbmp, webp | 組込の png / bmp / ppm / xbm / xpm を除く |
| clang-format / clang-tidy | `/opt/homebrew/opt/llvm/bin/`（Homebrew LLVM 22.1.8） | Xcode 同梱は clang-format のみで clang-tidy が無い。**LLVM 22 側に統一する** |
| Catch2 最新タグ | v3.15.3（GitHub API で取得） | 固定タグ候補 |
| git | 2.50.1 / **リポジトリ未初期化** | 品質ゲート 2 本が `git ls-files` に依存するため Phase 0 で初期化する |
| gh | 2.97.0 | — |

### 指示書に指定が無く、判断を仰いだ事項と回答

| # | 事項 | 回答 |
|---|---|---|
| 1 | git 初期化とリモート作成の分担 | **ローカルのコミットまでをエージェントが行う。** GitHub リモートの作成と push は利用者が行う |
| 2 | 不変条件スキャナ 6 種の実装手段（`phases.md` §3 に言語指定が無い） | **CMake script（`cmake -P`）**。新規依存が増えず、macOS / Windows で同一挙動 |
| 3 | スキャナ #3「フォーマット名の文字列リテラル」の判定方法 | **`src/core` は文字列リテラルを全面禁止**（`src/core/FormatId.hpp` のみ除外）。**`src/app` はスキャナが保持するフォーマット名一覧との照合**（`tr("...")` を許すため） |
| 4 | チェックリストに無い追加項目の可否 | **採用**: スキャナのネガティブテスト / Catch2 配線 + Qt 画像プラグイン smoke テスト / `src/app` をライブラリ + 薄い `main.cpp` に分割。**不採用**: `LICENSE` ファイルの配置（Phase 0 では行わない） |

### 未解決 / 保留（推測で埋めていない箇所）

1. **CI で固定する Qt 6.8 系のパッチ版が未確定。** `aqtinstall` で実在するバージョンを確認してから固定する。**現時点で番号を推測して書かない。**
2. **CI の clang-format / clang-tidy のメジャー版。** ローカル（LLVM 22）と CI ランナーの版が異なると整形結果が食い違うため、CI 側でも版を固定する。固定値はランナーで入手可能な版を確認してから決める。
3. **`phases.md` §5.2 のメタデータ保持方針**は Phase 1 着手時の判断事項。Phase 0 では触れない。
4. **Phase 0 受け入れ基準の「GitHub Actions が macOS / Windows の両方でビルド + テストを通す」は、利用者が push するまで検証不能。** ローカル作業完了時点では未達として扱い、push 後に本ファイルへ追記する。
5. **Windows は実機を持たないため、実機起動確認を行えない。** 受け入れ基準の要求どおり「CI ビルドのみ・実機未確認」と報告に明記する。

### 着手予定（承認後に実施する順序）

1. `git init` + `.gitignore` + 初回コミット、`phase0` ブランチ作成
2. 参照 4 文書を `docs/` 直下へ移動
3. `.clang-format` / `.clang-tidy` の配置
4. `CMakeLists.txt` / `CMakePresets.json` / `cmake/QualityGates.cmake`
5. `src/app`（空のメインウィンドウ。ライブラリ + 薄い `main.cpp`）
6. 不変条件スキャナ 6 種 + 違反フィクスチャ + ネガティブテスト 6 種
7. Catch2 配線（v3.15.3 固定 + `FIND_PACKAGE_ARGS`）と Qt 画像プラグイン smoke テスト
8. `docs/licenses.md` / `docs/adr/0001-ui-toolkit.md`
9. `.github/workflows/ci.yml`
10. 品質ゲートをローカルで全て実行し、本ファイルへ結果を追記

### 今回やらないこと

- `src/core` の型実装一切（`Result` / `FormatId` / `CapabilityTable` / `ConversionSpec` / `convert()` / `NamingRule`）→ Phase 1
- `src/io` 一切 → Phase 2
- `docs/adr/0002-noexcept-and-allocation.md` → Phase 1
- `docs/format-matrix.md` の自動生成 → Phase 1
- 追加コーデックの調査・導入 → Phase 3
- 配布・署名・公証 → Phase 4

---

## 2026-08-09 — Phase 0 実装（ローカル作業完了）

### 実施内容

上記「着手予定」の 1〜10 を実施した。ブランチは `phase0`。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `.gitignore` | `build/` `CMakeUserPresets.json` `compile_commands.json` 等 |
| `.clang-format` | LLVM ベース / IndentWidth 4 / ColumnLimit 100 / PointerAlignment Left |
| `.clang-tidy` | `phases.md` §3 の 6 グループ。除外は明示された 2 種のみ |
| `CMakeLists.txt` | C++20 / 警告ゼロ / `katachi_warnings`・`katachi_sanitizers` の INTERFACE ターゲット |
| `CMakePresets.json` | `dev` / `asan`。Qt は `$env{QT_ROOT_DIR}` 経由 |
| `cmake/QualityGates.cmake` | スキャナ 14 テストの登録 |
| `src/app/CMakeLists.txt` | `katachi_app`（静的ライブラリ）+ `katachi`（実行ファイル） |
| `src/app/MainWindow.hpp` / `.cpp` | 空のメインウィンドウ |
| `src/app/main.cpp` | 薄いエントリポイント |
| `tests/CMakeLists.txt` | Catch2 v3.15.3 を固定タグ + `FIND_PACKAGE_ARGS` |
| `tests/main.cpp` | `QCoreApplication` を構築してから `Catch::Session` を回す |
| `tests/core/qt_image_plugins_smoke.cpp` | Qt 画像プラグインの smoke |
| `tests/invariants/scan_invariants.cmake` | 不変条件スキャナ本体 |
| `tests/invariants/fixtures/violations/inv{1,2,3a,3b,4,5,6}/` | 故意の違反サンプル 7 件 |
| `.github/workflows/ci.yml` | macOS / Windows |
| `docs/licenses.md` / `docs/adr/0001-ui-toolkit.md` | 受け入れ基準の 2 項目 |

**移動**: `phases.md` / `spec-core.md` / `cpp-conventions.md` / `agent-protocol.md` → `docs/` 直下

**削除**: なし

### 追加・変更したテスト（16 件）

| テスト | 期待値 | 結果 |
|---|---|---|
| `invariant.inv1` | `src/` に `enable_if` / `void_t` が 0 件 | pass |
| `invariant.inv2` | `src/` に無制約テンプレートが 0 件 | pass |
| `invariant.inv3a` | `src/core` に文字列リテラルが 0 件（`FormatId.hpp` 除外） | pass（※下記） |
| `invariant.inv3b` | `src/app` の文字列リテラルにフォーマット名が 0 件 | pass |
| `invariant.inv4` | `src/core` が `io/` / `QtWidgets` を include していない | pass（※下記） |
| `invariant.inv5` | `src/` が `QtNetwork` / `QNetwork*` を include していない | pass |
| `invariant.inv6` | `src/` に `NOLINT` / 警告抑制プラグマが 0 件 | pass |
| `invariant.inv{1..6}.detects_violation`（7 本） | 違反フィクスチャに対し**非ゼロ終了する** | pass |
| smoke 2 本 | `QImageReader` / `QImageWriter` の対応形式が空でなく PNG を含む | pass |

**※ `invariant.inv3a` と `invariant.inv4` は、Phase 0 時点で `src/core` が存在しないため
走査対象 0 ファイルで通っている（空振り）。** 実質的な検出能力は
`detects_violation` 側で確認しており、Phase 1 で `src/core` を作った時点で実効性を持つ。
「全部 green」という表現だけで済ませないこと。

スキャナ実装時、7 種すべてが**意図と違う理由**（相対パスによる `file(RELATIVE_PATH)` エラー）で
落ちていた。`phases.md` §2.1-2「意図した理由で失敗することを確認する」の手順で検出し、
`file(REAL_PATH)` による正規化で修正した。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。`build/` を削除した状態から通しで実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --preset dev` | exit 0 |
| 2 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 3 | `ctest --preset dev --output-on-failure` | exit 0 / **16 / 16 pass** |
| 4 | `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')` | exit 0（clang-format 22.1.8） |
| 5 | `clang-tidy -p build/dev $(git ls-files 'src/*.cpp')` | exit 0（clang-tidy 22.1.8） |
| 6 | `cmake --preset asan` | exit 0 |
| 7 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 8 | `ctest --preset asan --output-on-failure` | exit 0 / **16 / 16 pass** |

**CI では未実行。** リモート未作成のため（下記「残課題」1）。

### 実機確認

- **macOS 14 / arm64（実機）: 確認済み。** アプリを起動し、Window Server の窓リスト
  （`CGWindowListCopyWindowInfo`）で `owner=katachi layer=0 size=960x632` の
  画面上ウィンドウを確認した。標準エラー出力は空。
  ウィンドウ**題名**は Screen Recording 権限が無いため取得できず未確認。
- **Windows: 実機未確認。** 実機を持たないため。CI も未実行のため、現時点では
  **ビルド確認すらできていない**（「CI ビルドのみ」ですらない）。

### 推測で埋めた箇所

`agent-protocol.md` §1 の 1〜4 で決まらず、5（一般的な慣習）に頼った箇所。

1. **CI の前提 3 点は未検証。** いずれも push するまで確かめられない。
   - `jurplel/install-qt-action@v4` が `QT_ROOT_DIR` を環境変数として設定すること
   - `windows-2022` ランナーに `choco` があること
   - `ilammy/msvc-dev-cmd@v1` で MSVC 環境が整うこと
2. **`.clang-tidy` の `WarningsAsErrors: '*'` と `HeaderFilterRegex: '.*/src/.*'`。**
   `phases.md` §3 はチェック集合のみを指定しており、この 2 項目の指定は無い。
   前者は「警告を残したまま通ったと報告できないようにする」ため、
   後者は Qt のヘッダを診断対象から外すために入れた。
3. **`src/app` の識別子命名規則。** 規約に指定が無いため Qt 風の lowerCamelCase を採った
   （`initialWindowWidth` 等）。`readability-identifier-naming` は無設定のため強制されていない。
4. **CI で clang-format / clang-tidy を macOS ジョブでのみ実行している。**
   `phases.md` §4 は「品質ゲートが全て CI で走り」としか書いておらず、
   OS ごとの実行要否の指定が無い。整形結果は OS に依存しないため 1 回で足りると判断した。

以下は**推測ではなく一次情報で確認した**もの（区別のため記載する）。

- Qt 6.8 系の最新パッチ版が **6.8.3** であること（`download.qt.io` の索引を mac_x64 /
  windows_x86 の両方で確認。6.8.0〜6.8.3 が存在）
- `clang-format` / `clang-tidy` の **22.1.8** が PyPI に存在し、macOS arm64 ホイールがあること
  （ローカル開発機の Homebrew LLVM 22.1.8 と同一版に固定できる）
- Catch2 の最新タグが **v3.15.3** であること（GitHub API）

### 指示書に無いが必要と判断して入れたもの（承認済みの追加スコープ以外）

- **`CMAKE_OSX_SYSROOT` の明示設定（`CMakeLists.txt` 冒頭）。**
  Apple clang は SDK を暗黙に見つけるが、品質ゲートで使う Homebrew LLVM の clang-tidy は
  見つけられず `<type_traits>` の解決に失敗し、**誤検出**（`misc-const-correctness` が
  `show()` を const 可と誤判定）を出した。`compile_commands.json` に `-isysroot` を
  含めることで、`CLAUDE.md` 記載のゲートコマンドを**一字も変えずに**通せるようにした。
  `--extra-arg` を足す案は、ゲートコマンドが指示書と食い違うため採らなかった。

### 提案（実装していない。判断を仰ぐ）

`CLAUDE.md` 停止条件 9 に従い、実装せず提案として記す。

1. **`.clang-format` に `AccessModifierOffset: -4` を追加したい。**
   現在の指定（LLVM ベース + `IndentWidth: 4`）の帰結として、LLVM 既定の
   `AccessModifierOffset: -2` が効き、`public:` が **2 スペース**の位置に来る
   （本文は 4 スペース）。半端な見た目になる。`-4` にすると `public:` が
   カラム 0 に揃う。`phases.md` §3 は 4 項目のみを指定しているため、
   独断では変更していない。

### 残課題 / 次にやること

1. **GitHub リモートの作成と push（利用者側）。** これが済むまで Phase 0 受け入れ基準
   「GitHub Actions が macOS / Windows の両方でビルド + テストを通す」は**未達**。
   push 後に CI の結果を本ファイルへ追記する。上記「推測で埋めた箇所」1 の 3 点は
   初回 CI で判明する。
2. **Windows の実機／CI 確認。** 1 の完了後に「CI ビルドのみ・実機未確認」の形で確定させる。
3. **`docs/format-matrix.md` の自動生成** → Phase 1。`.gitignore` に登録済み。
4. **`phases.md` §5.2 のメタデータ保持方針**（`MetadataPolicy::PreserveAll` を Phase 1 で
   実装するか Phase 3 に送るか）→ Phase 1 着手時に、使用する Qt の EXIF / ICC の実挙動を
   公式ドキュメントで確認してから判断する。**今は推測しない。**
5. **ADR-0002（`noexcept` と確保失敗）** → Phase 1。
6. **`src/core` 作成後、`invariant.inv3a` / `invariant.inv4` が空振りでなくなることを確認する。**

---

## 2026-08-09 — リモートへ push、CI が検出した不具合の修正、Phase 0 完了

### 実施内容

`https://github.com/SilentMalachite/Katachi.git` へ `main` と `phase0` を push した。
初回 CI（run 31280911440）が **2 件の実際の移植性バグ**を検出したため修正し、
再実行（run 31283909573 / commit `d7a89f1`）で 4 ジョブすべて success になった。

### 初回 CI が検出した不具合と修正

**1. `clang-tidy` の `misc-include-cleaner` が CI でのみ違反を報告した**

```
src/app/main.cpp:9:42: error: no header providing "QStringLiteral" is directly included
```

`QStringLiteral` マクロの定義ヘッダが Qt 6.8 と 6.11 で異なるため、
**ローカル（6.11）では通り、CI（6.8）で落ちた。**
`phases.md` §1.5 が「ローカルが 6.11 でも CI は下限に合わせる。6.8 以降に入った API を
無自覚に使う事故を CI で検出するため」とした仕組みが、初回から機能した。

修正: `QStringLiteral("Katachi")` → `QString::fromUtf8("Katachi")`。
`QString::fromUtf8` は `<QString>` が提供するため、6.8 / 6.11 の双方で安定する。
`misc-include-cleaner` を除外する案は採らなかった（`phases.md` §3 が除外可としているのは
`cppcoreguidelines-pro-bounds-*` と `modernize-use-trailing-return-type` の 2 種のみ）。

**2. Windows で smoke テスト 2 件が失敗した**

```
No test cases matched '"Qt ???????????????????PNG ???"'
88% tests passed, 2 tests failed out of 16
```

`catch_discover_tests` はテスト名をそのままフィルタ引数として実行ファイルへ渡すため、
日本語のテスト名が Windows のコンソール encoding で `?` に化けて一致しなくなっていた。
**私が書いたテストの移植性バグであり、テストの期待値の問題ではない。**

修正: テスト名を ASCII にした。
`QImageReader supports at least PNG` / `QImageWriter supports at least PNG`。
説明は同ファイル内のコメントに日本語で残した。以後、**ctest に登録される名前は ASCII に限る。**

### 品質ゲートの実行結果（CI・run 31283909573）

| ジョブ | 結果 |
|---|---|
| ビルド + テスト (macOS 14 / arm64、Qt 6.8.3) | **success / 16 / 16 pass** |
| ビルド + テスト (Windows 2022 / MSVC、Qt 6.8.3 msvc2022_64) | **success / 16 / 16 pass** |
| clang-format + clang-tidy (macOS、ともに 22.1.8) | **success** |
| ASan + UBSan (macOS) | **success / 16 / 16 pass** |

CI ログ全体（2283 行）に `warning:` / `error:` を含む行は **0 行**。
`QT_ROOT_DIR` は macOS が `/Users/runner/work/Katachi/Qt/6.8.3/macos`、
Windows が `D:\a\Katachi\Qt\6.8.3\msvc2022_64` で、いずれも 6.8.3 に固定されていることを
ログで確認した。

### 前エントリの「推測で埋めた箇所」1 の確定

初回 CI の Windows ジョブが ctest まで到達し 16 中 14 が pass していたことから、
下記 3 点は**いずれも正しかった**と確認できた（推測ではなくなった）。

- `jurplel/install-qt-action@v4` が `QT_ROOT_DIR` を設定する
- `windows-2022` ランナーに `choco` がある
- `ilammy/msvc-dev-cmd@v1` で MSVC 環境が整う（`/W4 /WX` 付きビルドが警告ゼロで通過）

### 推測で埋めた箇所

**なし。** 本エントリの記述はすべて CI のログで確認した事実に基づく。

なお、前エントリに記載した推測 2〜4（`.clang-tidy` の 2 設定、識別子命名規則、
リンタを macOS ジョブでのみ実行）は、CI が green になったことで
「動作する」ことは確認できたが、**指示書に根拠が無い判断であることは変わらない。**

### Phase 0 受け入れ基準（`phases.md` §4）

| 項目 | 状態 |
|---|---|
| GitHub Actions が macOS / Windows の両方でビルド + テストを通す | **達成**（run 31283909573） |
| CI の Qt が 6.8 系 LTS に固定されている | **達成**（6.8.3。ログで確認） |
| 実機で空のメインウィンドウが起動することを、確認できた OS について確認する | **達成**（macOS 14 / arm64 実機） |
| 実機確認していない OS は「CI ビルドのみ・実機未確認」と明記する | **達成**（Windows。CI でビルド + テストは通ったが**実機未確認**） |
| 不変条件スキャナ 6 種が実装され、`ctest` で走る | **達成**（INV3 を core / app に分けた 7 検査 + 検出確認 7 本 = 14 テスト） |
| 品質ゲートが全て CI で走り、警告ゼロ | **達成** |
| `.clang-format` が §3 の設定で置かれている | **達成** |
| `docs/adr/0001-ui-toolkit.md` に Qt Widgets 採用の根拠がある | **達成** |
| `docs/licenses.md` に Qt の LGPL 条件と動的リンクである旨が記載されている | **達成** |
| 4 つの参照文書が `docs/` 直下にあり、`CLAUDE.md` のみリポジトリ直下にある | **達成** |

**Phase 0 は完了した。**

### 残課題 / 次にやること

1. **`phase0` → `main` の PR 作成とマージ**（1 Phase = 1 ブランチ = 1 PR）。未実施。
2. **前エントリの提案（`.clang-format` に `AccessModifierOffset: -4`）の可否判断。** 未実施。
3. Phase 1 着手時: ADR-0002（`noexcept` と確保失敗）、`phases.md` §5.2 のメタデータ保持方針、
   `docs/format-matrix.md` の自動生成、`src/core` 作成後に
   `invariant.inv3a` / `invariant.inv4` が空振りでなくなることの確認。
4. **Windows の実機確認は引き続き未了。** 実機を用意できるまで「CI のみ」の状態が続く。

---

## 2026-08-09 — PR #1 を main へマージ。Phase 0 クローズ

### 実施内容

`phase0` → `main` の PR を作成しマージした。「1 Phase = 1 ブランチ = 1 PR」（`phases.md` §1）の完了。

| 項目 | 内容 |
|---|---|
| PR | [#1](https://github.com/SilentMalachite/Katachi/pull/1) |
| 差分 | 28 ファイル（追加 23 / 変更 1 / リネーム 4）、+1090 行、8 コミット |
| マージ方式 | **merge commit**（squash しない） |
| マージコミット | `7732263` |

squash を採らなかったのは、`agent-protocol.md` §6「1 コミット = 1 つの意味のある変更」に従って
積んだ 8 コミットの粒度を `main` に残すため。

### 変更ファイル

**追加 / 変更 / 削除ともになし。** 本エントリは PR 作成・マージの記録であり、コードの変更を伴わない。

### 追加・変更したテスト

**なし。** 既存の 16 テストのまま。

### 品質ゲートの実行結果

マージ前後で計 3 回、CI 全ジョブの green を確認した。

| タイミング | run | 結果 |
|---|---|---|
| `phase0` への push（`f780d1f`） | 31284036906 | 4 ジョブ success |
| PR 作成による `pull_request` トリガ | 31284242195 | 4 ジョブ success |
| **マージ後の `main`（`7732263`）** | **31284444275** | **4 ジョブ success** |

マージ直前の `gh pr checks` は **8 / 8 pass、pending・fail は 0**、
`mergeStateStatus=CLEAN` を確認してからマージした。pending のままマージしていない。

マージコミットに対する CI（3 番目）も green であり、`main` が壊れていないことを確認済み。

### 推測で埋めた箇所

**なし。** すべて CI と `gh` の出力で確認した事実。

### 作業中に起きた紛らわしい事象（記録として残す）

`gh pr merge` の直後に exit 1 が出たが、**マージの失敗ではない。**
同じコマンド列に続けて書いた確認用の `gh pr view --json merged` が、
存在しないフィールド名を指定していたことによるもの。
マージ自体は成功していた（`state=MERGED` / `mergedAt=2026-08-08T23:38:52Z`）。

同様に、初回 CI の待機に使った `gh run watch --exit-status` は
**run が failure でも exit 0 を返した。** 終了コードを信じず
`gh run view --json conclusion` で確認したため、2 件の失敗に気づけた。
**以後、CI の成否は `gh run view` の `conclusion` で判定する。**

### Phase 0 の最終状態

`phases.md` §4 の受け入れ基準 10 項目すべて達成。**Phase 0 はクローズする。**

### 残課題 / 次にやること

1. **`.clang-format` に `AccessModifierOffset: -4` を足す提案の可否判断。** 未実施のまま持ち越す。
2. **Windows の実機起動確認。** CI ではビルド + テストが通っているが実機未確認。
   実機を用意できるまで解消しない。
3. **`phase0` ブランチの削除。** 未実施。指示を待つ。
4. **Phase 1 の計画提示と承認取得。** 未着手。Phase 1 着手時の宿題は次のとおり。
   - ADR-0002（コア層全体の `noexcept` と確保失敗の方針）
   - `phases.md` §5.2 のメタデータ保持方針 — 使用する Qt の EXIF / ICC の実挙動を
     公式ドキュメントで確認してから判断する。**今は推測しない**
   - `docs/format-matrix.md` のビルド時自動生成
   - `src/core` 作成後、`invariant.inv3a` / `invariant.inv4` が空振りでなくなることの確認

---

## 2026-08-09 — `.clang-format` に `AccessModifierOffset: -4` を追加（提案が承認された）

### 実施内容

2026-08-09 の「提案」節に記した `.clang-format` への `AccessModifierOffset: -4` 追加が
**承認されたため実装した。**

`phases.md` §3 が指定する `.clang-format` は 4 項目（LLVM ベース / `IndentWidth: 4` /
`ColumnLimit: 100` / `PointerAlignment: Left`）のみで、`AccessModifierOffset` の指定は無い。
LLVM 既定の `-2` が効くため、`IndentWidth: 4` と組み合わさると `public:` が
2 スペースの位置に来て、本文（4 スペース）ともクラス宣言（0 スペース）とも揃わなかった。

**`phases.md` §3 の指定を上書きしたのではなく、指定の無い項目を追加した。**
5 項目目である旨と理由を `.clang-format` 内のコメントに残した。

### 変更ファイル

**変更**

| ファイル | 内容 |
|---|---|
| `.clang-format` | `AccessModifierOffset: -4` と理由コメントを追加 |
| `src/app/MainWindow.hpp` | 再整形により `public:` が 2 スペース → カラム 0 へ |

**追加 / 削除**: なし

### 追加・変更したテスト

**なし。** 整形設定の変更であり、テストの追加も期待値の変更も無い。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **16 / 16 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 |
| 5 | `cmake --build --preset asan` | exit 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **16 / 16 pass** |

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **Windows の実機起動確認。** 変わらず未了。
2. **Phase 1 の計画提示と承認取得。** 未着手。
