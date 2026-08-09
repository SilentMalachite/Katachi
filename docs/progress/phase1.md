# Phase 1 — コア

**追記のみ。既存の記述は書き換えない。** 誤りを見つけた場合も遡って修正せず、新しい日付で訂正を追記する。

---

## 2026-08-09 — 着手前の調査と判断（計画は承認待ち）

### 実施内容

実装は未着手。`phases.md` §5.2 が Phase 1 着手時に求める調査を行い、
指示書内の矛盾 3 件について判断を仰いで回答を得た。ブランチは `phase1`。

### 読み込んだファイル

`CLAUDE.md` / `docs/phases.md` / `docs/spec-core.md` / `docs/cpp-conventions.md` /
`docs/agent-protocol.md` / `docs/progress/phase0.md` / `docs/adr/0001-ui-toolkit.md` /
`docs/licenses.md`

### `phases.md` §5.2 の調査結果（Qt 6.8 公式ドキュメント）

「使用する Qt バージョンの EXIF / ICC の実挙動を公式ドキュメントで確認してから判断する。
今は推測しない」との指示に従い、`doc.qt.io/qt-6.8` の以下のページで API の実在を確認した。

- `qimagereader.html` / `qimagewriter.html` / `qimage.html` / `qimageiohandler.html` / `qcolorspace.html`

| 保持対象 | Qt 6.8 の API | 可否 |
|---|---|---|
| EXIF の**向きのみ** | `QImageReader::transformation()` / `QImageWriter::setTransformation()`。`QImageIOHandler::Transformations` の説明に "usually through **EXIF**" とある | **可** |
| テキスト metadata（key/value） | `QImageReader::text()` / `textKeys()` / `QImageWriter::setText()`。`QImageIOHandler::Description` 経由（"GIF and PNG ... embedding of text or comments"） | **可**（形式依存） |
| ICC プロファイル | `QImage::colorSpace()` / `setColorSpace()`、`QColorSpace::fromIccProfile()` / `iccProfile()` | **可** |
| **EXIF 全体**（カメラ機種・GPS・撮影日時等） | **該当 API が存在しない** | **不可** |

**結論: `MetadataPolicy::PreserveAll` は Qt 単体では字義どおり実装できない。**
`IccPolicy::Embed` / `Strip` は実装可能。

### 指示書内で見つけた矛盾・不足（停止条件 2）と、得られた判断

| # | 問題 | 判断（利用者回答） |
|---|---|---|
| 1 | `spec-core.md` §4 が「警告を結果に含める」と要求するが、§2 の `convert()` は `Result<QByteArray, ConvertError>` を返し**警告の置き場所が無い** | **戻り値型を変える。** `Result<ConversionOutput, ConvertError>` とし、`ConversionOutput { QByteArray bytes; std::vector<ConvertWarning> warnings; }` を追加する |
| 2 | `MetadataPolicy::PreserveAll` が Qt 単体で実現不可（上記調査） | **列挙名を実態に合わせる。** `PreserveAll` → `PreserveSupported` に改名し、「向き + テキスト + ICC を保持する」と ADR-0003 に定義。EXIF 全体は Phase 3 で外部ライブラリを検討 |
| 3 | `NamingError` の列挙が未定義。かつ衝突ポリシー（`Overwrite`/`Skip`/`Rename`）は `resolveOutputName()` の引数に無く、判定にファイルシステム参照が要るため **core では実装不可** | **core は純粋生成のみ。** `NamingError` を `EmptyPattern` / `UnknownPlaceholder` / `InvalidIndexSpec` / `EmptyResult` の 4 値で定義。衝突ポリシーの適用は Phase 2 の io 層へ送る |

進め方は **T0→T7 を順に実装し、各タスク完了時に品質ゲートを通して報告する**（1 タスク = 1 コミット）。
Phase 1 完了時に 1 PR。

### これに伴う `docs/spec-core.md` の変更（T0 で実施する）

**指示書そのものを変更するため、承認を得てから行う。**

1. §2: `convert()` の戻り値を `Result<ConversionOutput, ConvertError>` に変更。
   `ConvertWarning` と `ConversionOutput` の定義を追加
2. §2: `MetadataPolicy` の `PreserveAll` を `PreserveSupported` へ改名（`ConversionSpec::metadata` の既定値も追随）
3. §4: 3 行目の「警告を結果に含める」の具体的な手段（`ConvertWarning::AlphaFlattenedFallback`）を明記
4. §5: `NamingError` の 4 列挙値を明記し、衝突ポリシーの適用層が io（Phase 2）であることを追記

### 差分規模の申告（停止条件 8）

**Phase 1 全体の差分は 400 行を大幅に超える見込み**（本体 + テストで 1500〜2500 行程度）。
Phase 0 のような包括承認は無いため申告した。T0〜T7 の各タスクは概ね 400 行以内に収まる見込み。

### 作業分割（1 タスク = 1 コミット）

| # | 内容 |
|---|---|
| T0 | `docs/spec-core.md` の更新と ADR-0002〜0005 の作成 |
| T1 | `src/core/Concepts.hpp`（`ResultValue` / `ResultError`）+ `Result.hpp` |
| T2 | `FormatId` / `ConvertError` / `ConvertWarning` |
| T3 | `CapabilityTable` + `CapabilitySource` concept |
| T4 | テストフィクスチャの生成（`tests/fixtures/generate.cpp`） |
| T5 | `ConversionSpec` + `convert()`（心臓部） |
| T6 | `NamingRule` |
| T7 | `docs/format-matrix.md` のビルド時自動生成 |

ADR は実装より先（T0）に書く。決定を記録してから実装する順序を崩さないため。

### 推測で埋めた箇所

**なし。** Qt の API はすべて公式ドキュメントで実在を確認した。
矛盾 3 件は自分で判断せず停止して指示を仰いだ。

### 残課題 / 次にやること

1. **本計画の承認待ち。** 承認前に T0 へ進まない。
2. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T0 完了（指示書の更新と ADR 0002〜0005）

### 実施内容

計画が承認されたため T0 を実施した。**コードは 1 行も書いていない。**
決定を文書に固定してから実装に入る順序を守るための先行タスク。

### 変更ファイル

**変更**

| ファイル | 内容 |
|---|---|
| `docs/spec-core.md` §2 | `ConvertWarning` 追加 / `ConversionOutput` 追加 / `convert()` の戻り値を `Result<ConversionOutput, ConvertError>` に変更 / `MetadataPolicy::PreserveAll` → `PreserveSupported` に改名 / `ConversionSpec::metadata` の既定値を追随 |
| `docs/spec-core.md` §4 | アルファ表 3 行目に、警告の具体手段が `ConvertWarning::AlphaFlattenedFallback` であることを明記 |
| `docs/spec-core.md` §5 | `NamingError` の 4 列挙値を定義 / 衝突ポリシーの適用層が Phase 2 の `src/io` であることを追記 |
| `docs/phases.md` §5.1 | 決定 3 件（メタデータ / 警告の返し方 / 衝突ポリシーの担当層）を追加 |
| `docs/phases.md` §5.2 | 「該当なし」に更新（唯一の項目が決着し §5.1 へ移動したため） |

**追加**

| ファイル | 内容 |
|---|---|
| `docs/adr/0002-noexcept-and-allocation.md` | コア層全体の `noexcept` と確保失敗の方針。`agent-protocol.md` §5.2 が必須と定めた ADR |
| `docs/adr/0003-metadata-policy.md` | `PreserveSupported` への改名と保持範囲の定義。参照した Qt 公式ドキュメントの URL を記載 |
| `docs/adr/0004-convert-warnings.md` | 成功値に警告を載せる判断 |
| `docs/adr/0005-naming-responsibility.md` | 名前の生成は core、衝突の解決は io |

**削除**: なし

### 追加・変更したテスト

**なし。** T0 は文書のみのタスク。テストは T1 以降で追加する。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64）

文書のみの変更だが、ゲート未実行での報告はしない方針のため全 6 本を実行した。

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **16 / 16 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 |
| 5 | `cmake --build --preset asan` | exit 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **16 / 16 pass** |

`grep` で `docs/` 配下に `PreserveAll` の残存参照が無いことも確認した
（ADR-0003 と本 progress の、経緯として言及している箇所を除く）。

### 承認された計画に無かった変更（申告）

**`docs/phases.md` の §5.1 / §5.2 の更新は、承認時に提示した T0 の項目に含めていなかった。**

理由: §5.2 は「メタデータ保持の実装手段 — Phase 1 着手時に決める」と書かれており、
これを放置すると、決着済みの論点が未解決のまま残り、将来のセッションが
同じ判断をやり直すことになる。§5.2 自身の指示（着手時に決める）を完了させる
記録行為と判断して実施した。

**指示書の変更であるため、不要であれば戻す。**

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **T1（`Result<T,E>` と core 層 concept）に着手する。**
   `src/core/CMakeLists.txt` / `Concepts.hpp` / `Result.hpp` と
   `tests/core/result_test.cpp`。ルート `CMakeLists.txt` への `add_subdirectory(src/core)` 追加を含む。
2. T1 で `src/core` が実在するようになるため、`invariant.inv3a` / `invariant.inv4` が
   空振りでなくなる。**走査対象が 0 ファイルでないことを確認する。**
3. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T1 完了（`Result<T,E>` と core 層 concept）

### 実施内容

`src/core` を新設し、`Result<T,E>` と core 層の concept（`ResultValue` / `ResultError`）を実装した。
TDD の順序（失敗するテスト → 意図した理由の確認 → 実装）を守った。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/core/CMakeLists.txt` | `katachi_core`。**依存は `Qt6::Core` / `Qt6::Gui` のみ。`Qt6::Widgets` をリンクしない** |
| `src/core/Concepts.hpp` | `ResultValue` / `ResultError`（`cpp-conventions.md` §2.2 の定義どおり） |
| `src/core/Result.hpp` | `std::variant<T,E>` 実装。`requires(!std::same_as<T,E>)` |
| `tests/core/result_test.cpp` | 実行時テスト 4 本 + `static_assert` 群 |
| `tests/core/compile_fail/result_same_type.cpp` | `T == E` 排除のコンパイル失敗テスト用 |

**変更**

| ファイル | 内容 |
|---|---|
| `CMakeLists.txt` | `add_subdirectory(src/core)` を `src/app` より前に追加 |
| `tests/CMakeLists.txt` | `result_test.cpp` の追加、`katachi_core` のリンク、コンパイル失敗テストの登録 |

**削除**: なし

### 設計上の判断

**`katachi_core` は T1 時点では `INTERFACE` ライブラリ。** `Concepts.hpp` / `Result.hpp` が
ヘッダのみで、`STATIC` にするとソースが無くて CMake が失敗するため。
**`FormatId.cpp` が入る T2 で `STATIC` へ変更する。** 空の `.cpp` を置いて `STATIC` にする案は、
死んだコードを作るため採らなかった。

**`Qt6::Widgets` をリンクしないことが INV4 の実体的な担保になる。**
リンクしなければ `#include <QWidget>` のようなフラットヘッダは include パスに無く、
コンパイル時に落ちる。スキャナ INV4 はテキスト上の検出を担う。両方が揃って初めて塞がる。

**`value()` / `error()` は `std::get` を使わず `assert` + `std::get_if` で実装した。**
`std::get` は契約違反時に `std::bad_variant_access` を投げるが、
core では例外を送出できない（`CLAUDE.md` 絶対禁止）。契約は実行時アサートで担保する
（`cpp-conventions.md` §2.5 の表どおり）。`noexcept` は付けていない
（`spec-core.md` §2 のシグネチャに無いため。明示的な記述を優先した）。

### TDD の経過（記録として残す）

1. `tests/core/result_test.cpp` を先に書き、CMake を配線してビルド
2. **意図した理由での失敗を確認**: `fatal error: 'core/Concepts.hpp' file not found`
3. `Concepts.hpp` / `Result.hpp` を実装
4. **ここで想定外の失敗**（下記）

**想定外の失敗と対処。**

`static_assert(!requires { typename Result<QString, QString>; })` を否定側テストとして
書いたが、これはコンパイルエラーになった。

```
error: constraints not satisfied for class template 'Result' [with T = QString, E = QString]
```

制約を満たさないクラステンプレートの特殊化を名前で指すことは**代入失敗ではなくハードエラー**で、
`requires` 式が `false` に評価される前にコンパイルが止まる。当初の想定が誤っていた。

対処: **「そのファイルのビルドが失敗すること」を ctest で確認する方式に変更した。**
`tests/core/compile_fail/result_same_type.cpp` を `EXCLUDE_FROM_ALL` の
ターゲットとして持ち、`ctest` から `cmake --build --target` を呼んで `WILL_FAIL TRUE` で判定する。
不変条件スキャナのネガティブテストと同じ考え方。

**この過程で、失敗したビルドの直後に `ctest` を走らせて「16/16 pass」を得るという
誤りを一度犯した。** 実際には古いテストバイナリが走っていた。
以後、**ゲートは終了コードを変数で受けて判定する**（`grep` のパイプ後の `$?` を見ない）。

### 追加・変更したテスト（5 本追加。16 → 21）

| テスト | 期待値 | 結果 |
|---|---|---|
| `Result::ok reports success and yields the value` | `isOk()==true`、`value()==42` | pass |
| `Result::err reports failure and yields the error` | `isOk()==false`、`error()==TestError::Alpha` | pass |
| `Result carries a moved-in QByteArray payload` | `isOk()==true`、`value().size()==3` | pass |
| `Result distinguishes error values of the same type` | `Alpha != Beta` が区別される | pass |
| `core.result.rejects_same_type` | `Result<QString,QString>` を含むファイルの**ビルドが失敗する** | pass（`WILL_FAIL`） |

`static_assert` による契約（実行時テストではないがコンパイル時に検証される）:

- 肯定: `ResultValue<int>` / `ResultValue<QByteArray>` / `ResultValue<QString>` / `ResultError<TestError>`
- **否定: `!ResultValue<ThrowingMove>`**（move 構築が `noexcept` でない型）、
  **`!ResultError<NoEquality>`**（`operator==` を持たない型）
- 肯定: `requires { typename Result<QString, TestError>; }`
- `ResultValue<QString> && ResultError<QString>` — これにより、コンパイル失敗テストが落ちる理由が
  「concept 不適合」ではなく「`requires(!same_as<T,E>)`」であることが確定する

### 品質ゲートの実行結果（ローカル macOS 14 / arm64）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **21 / 21 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **21 / 21 pass** |

### 不変条件スキャナが空振りでなくなったことの確認（計画で約束した検証）

`src/core` に `Concepts.hpp` / `Result.hpp` が実在するようになったため、
Phase 0 で 0 ファイル走査だった 2 検査が実効性を持った。

- `INV3A` / `INV4` とも `src/core` を走査して green
- **空振りでないことの確認**: `src/core` に文字列リテラルを含む一時ファイルを置いて
  `INV3A` を実行し、`core/__tmp_probe.hpp:2` を指して落ちることを確認した。一時ファイルは削除済み

### 推測で埋めた箇所

**なし。**

`static_assert(!requires{...})` が使えると想定していた点は推測ではなく**誤り**であり、
ビルドで検出して方式を変更した。上記「TDD の経過」に記録した。

### 残課題 / 次にやること

1. **T2（`FormatId` / `ConvertError` / `ConvertWarning`）に着手する。**
   `katachi_core` を `INTERFACE` から `STATIC` へ変更するのはこのタスク。
2. `FormatId.hpp` は**フォーマット名の文字列リテラルが許される唯一の場所**。
   スキャナ INV3A の除外がこのファイルにだけ効いていることを、T2 で実際に確認する。
3. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T2 完了（`FormatId` / `ConvertError` / `ConvertWarning`）

### 実施内容

`FormatId`（強い型付き文字列）と、変換結果を説明する 2 つの列挙を実装した。
TDD の順序を守り、実装前に意図した理由での失敗（`'core/ConvertError.hpp' file not found`）を確認した。

### 前エントリ（T1）の訂正

**T1 の報告に書いた「`clang-format --dry-run --Werror` exit 0」は、
実行はしたが新規ファイルを検査していなかった。**

品質ゲートの `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')` は
`git ls-files` に依存する。`git ls-files` は**追跡済み（またはステージ済み）のファイルしか
列挙しない**。T1 ではコミット前にゲートを実行したため、新規追加した
`src/core/Concepts.hpp` などが対象から漏れていた。

T2 で `Concepts.hpp` が追跡済みになった結果、T1 由来の整形違反が顕在化した。

```
src/core/Concepts.hpp:25:22: error: code should be clang-formatted
```

**手順を変更する。以後、品質ゲートを実行する前に `git add` でステージする。**
ゲートのコマンド自体は `CLAUDE.md` の記載どおりで変更しない。
（CI では常にコミット済みの状態で走るため、この穴は CI には無い。
push していれば CI が検出していた。）

T1 由来の整形違反は本エントリの整形で解消済み。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/core/FormatId.hpp` | 強い型付き文字列と `QString` ⇄ `FormatId` 変換関数 |
| `src/core/ConvertError.hpp` | `ConvertError`（6 値）と `ConvertWarning`（1 値） |
| `tests/core/format_id_test.cpp` | 実行時テスト 7 本 + `static_assert` 2 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `tests/CMakeLists.txt` | `format_id_test.cpp` を追加 |
| `src/core/Concepts.hpp` | 整形のみ（上記の訂正による） |
| `tests/core/compile_fail/result_same_type.cpp` / `tests/core/format_id_test.cpp` | 整形のみ |

**削除**: なし

### 計画からの変更（申告）

**`katachi_core` を `INTERFACE` から `STATIC` へ変更しなかった。**

承認時の計画では「T2 で `STATIC` へ変更する」としていたが、実装前に
`docs/spec-core.md` §1 のディレクトリ構成を読み直したところ、

```
│   │   ├── FormatId.hpp            # 強い型付き文字列。文字列リテラル例外はここだけ
│   │   ├── CapabilityTable.hpp/.cpp
```

**`FormatId` は `.hpp` のみで `.cpp` が無い**（`CapabilityTable` は `.hpp/.cpp` と明記されている）。
指示書の明示的な記述を優先し（`agent-protocol.md` §1 の解決順序 1）、変換関数を `inline` として
ヘッダに置いた。したがって core にはまだ `.cpp` が無く、`STATIC` にできない。

**`STATIC` への変更は `CapabilityTable.cpp` が入る T3 で行う。**

### 実装上の判断（指示書に無い箇所）

1. **`formatIdFromString()` は前後の空白を落とし、小文字へ畳む。**
   `docs/spec-core.md` §2.1 は変換関数の存在を求めるが、正規化の有無を書いていない。
   正規化しないと、拡張子や利用者入力から作った `FormatId`（`"PNG"`）が
   Qt 由来の `FormatId`（`"png"`）と `operator==` で別物になり、
   `CapabilityTable::find()` が取りこぼす。**振る舞いを変えたい場合は指示を仰ぐ。**
   別名の吸収（`jpg` → `jpeg` 等）は**行っていない**。指示書に根拠が無く、勝手に決めないため。

2. **`ConvertError` / `ConvertWarning` に基底型 `std::uint8_t` を明示した。**
   推測ではなく実測で必要性を確認した。`clang-tidy --config-file=.clang-tidy` を
   ヘッダに直接かけたところ、`performance-enum-size` が **error** を出した（exit 1）。
   `docs/phases.md` §3 は `performance-*` を必須で有効にしており、除外可能なのは
   別の 2 種のみ。抑制コメントも禁止されている。列挙子の顔ぶれは
   `docs/spec-core.md` §2 のままで意味は変えていない。

3. **`formatIdToString()` の引数名を `id` から `format` にした。**
   `readability-identifier-length`（3 文字未満のパラメータ名を禁止）が error を出したため。
   **`docs/spec-core.md` §2.1 が定めるメンバ名 `v` は同チェックの対象外**であることを
   実測で確認済み。仕様の型定義は変えていない。

### 不変条件スキャナが自分の書いたコードを検出した件（記録）

`ConvertError.hpp` のコメントに **`NOLINT` という語そのもの**を書いたところ、
`invariant.inv6` が落ちた。

```
core/ConvertError.hpp:12: 警告抑制（CLAUDE.md で禁止。必要と判断したら停止して報告する）
  > // NOLINT による抑制は CLAUDE.md で禁止されている。
```

INV6 は**コメントを除去しない**設計である（抑制指示はコメントとして書かれるため）。
したがって語の言及と実際の抑制を区別できない。**これはスキャナの正しい動作**であり、
コメントの文言を変えて解消した。スキャナ側を緩めていない。

### `FormatId.hpp` の除外がそこにだけ効くことの確認（T1 で約束した検証）

1. `src/core/FormatId.hpp` に `"png"` を含む一時的な記述を足して `INV3A` を実行
   → **見逃される**（`ok`）。除外が効いている
2. **同じ記述を `src/core/Result.hpp` に足して `INV3A` を実行
   → `core/Result.hpp:50` を指して検出**。除外が他ファイルへ漏れていない

両方の一時的な記述は削除済み（`git status` で復元を確認）。

### 追加・変更したテスト（7 本追加。21 → 28）

| テスト | 期待値 | 結果 |
|---|---|---|
| `formatIdFromString round-trips through formatIdToString` | `formatIdToString(formatIdFromString("png")) == "png"` | pass |
| `formatIdFromString folds case so lookups are stable` | `"PNG"` と `"png"`、`"JpEg"` と `"jpeg"` が等しい | pass |
| `formatIdFromString trims surrounding whitespace` | `"  png\t"` と `"png"` が等しい | pass |
| `FormatId distinguishes different names` | `"png"` と `"bmp"` が等しくない | pass |
| `FormatId comparison is value based` | 同じ名前から作った 2 値が `==`、`.v` も一致 | pass |
| `ConvertError values are distinct and comparable` | 6 列挙値が互いに区別され、同値比較が成立 | pass |
| `ConvertWarning carries the alpha fallback case` | `AlphaFlattenedFallback` が比較可能 | pass |

`static_assert`: `ResultValue<FormatId>` / `ResultError<ConvertError>`。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。**ステージ済みの状態で実行**）

`git ls-files '*.cpp' '*.hpp'` の対象は 19 ファイル。

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **28 / 28 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **28 / 28 pass** |

**clang-tidy ゲートの現在の到達範囲に注意。** 対象は `git ls-files 'src/*.cpp'`、
すなわち `src/app/main.cpp` と `src/app/MainWindow.cpp` の 2 本だけである。
core はまだ `.cpp` を持たず、`src/app` の 2 本は core ヘッダを include していないため、
**`HeaderFilterRegex` 経由でも core のヘッダに届いていない。**
本エントリで core ヘッダに対して行った clang-tidy 検査は、
`--config-file` を使った手動実行である。
**T3 で `CapabilityTable.cpp` が入ると、ゲートが core ヘッダへ自動的に届くようになる。**

### 推測で埋めた箇所

**なし。** `performance-enum-size` と `readability-identifier-length` の必要性は、
いずれも clang-tidy を実際に走らせて確認した。

### 残課題 / 次にやること

1. **T3（`CapabilityTable` と `CapabilitySource` concept）に着手する。**
   ここで `katachi_core` を `INTERFACE` から `STATIC` へ変更する。
2. T3 で clang-tidy ゲートが core ヘッダに届くようになる。
   **届いた結果として新たな指摘が出ないか確認する。**
3. `formatIdFromString()` の正規化（小文字化・trim）は指示書に根拠が無い判断。
   不要であれば指示を仰いで戻す。
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T2 追補（`FormatId` の正規化を確定し、別名の吸収を追加）

### 実施内容

前エントリで「指示書に根拠が無い判断」として報告した `formatIdFromString()` の正規化について、
**正規化を行うこと、および別名を吸収することの指示を得た。** 決定として確定させ、
別名の吸収を追加した。

### 判断の根拠にした実測（Qt 6.11.1 / macOS 14 arm64）

別名をどう選ぶかを推測しないため、Qt が実際に報告する名前と MIME タイプを測った。

```
READ : bmp cur gif heic heif icns ico jfif jp2 jpeg jpg pbm pdf pgm png ppm
       svg svgz tga tif tiff wbmp webp xbm xpm
WRITE: bmp cur heic heif icns ico jfif jp2 jpeg jpg pbm pgm png ppm tif tiff
       wbmp webp xbm xpm
MIME : ... image/jpeg ... image/tiff ... image/heic image/heif ...
```

**Qt は `jpeg` / `jpg` / `jfif` を別々の名前として報告するが、MIME はいずれも `image/jpeg`。**
`tif` / `tiff` も同じく `image/tiff`。
一方 `heic` と `heif` は `image/heic` と `image/heif` で **別の MIME**。

**そこで「Qt が同一の MIME タイプを報告すること」を、別名を畳む基準に据えた。**
名前の見た目や一般的な慣習では判断しない。

| 別名 | 代表名 | 根拠 |
|---|---|---|
| `jpg` | `jpeg` | ともに `image/jpeg` |
| `jfif` | `jpeg` | ともに `image/jpeg` |
| `tif` | `tiff` | ともに `image/tiff` |

**畳まなかったもの（意図的）**: `heic` / `heif`、`svg` / `svgz`。いずれも MIME が異なる。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `docs/adr/0006-format-id-normalization.md` | 正規化の内容、別名を畳む基準、T3 への帰結 |

**変更**

| ファイル | 内容 |
|---|---|
| `src/core/FormatId.hpp` | 別名表を追加。**「フォーマット名の文字列リテラル」例外枠を実際に使う唯一の箇所になった** |
| `docs/spec-core.md` §2.1 | 正規化の 3 段階と、`buildFromQt()` が正規化後の重複を統合すべきことを追記 |
| `tests/core/format_id_test.cpp` | 別名のテスト 4 本を追加 |

**削除**: なし

### T3 への帰結（見落とすと `find()` が不定になる）

**`CapabilityTable::buildFromQt()` は正規化後の重複を畳まなければならない。**
Qt は `jpeg` / `jpg` / `jfif` を別々に報告するため、正規化すると 3 件が同じ `FormatId` になる。
**同一 `FormatId` の `FormatCapability` は 1 件へ統合し、`extensions` は和集合を取る。**
これを怠ると `find()` がどれを返すか不定になる。ADR-0006 と `spec-core.md` §2.1 に記載済み。

また `CapabilityTable` も**同じ `formatIdFromString()` で正規化する**。
表の鍵と問い合わせの鍵が必ず同じ経路を通るため、片側だけ畳まれて引けなくなることはない。

### 追加・変更したテスト（4 本追加。28 → 32）

| テスト | 期待値 | 結果 |
|---|---|---|
| `formatIdFromString folds jpeg aliases onto one id` | `jpg` / `JPG` / `jfif` が `jpeg` と同一。代表名の文字列が `"jpeg"` | pass |
| `formatIdFromString folds tiff aliases onto one id` | `tif` / `"  TIF "` が `tiff` と同一。代表名の文字列が `"tiff"` | pass |
| `formatIdFromString keeps heic and heif apart` | `heic` と `heif` が**等しくない** | pass |
| `formatIdFromString leaves unrelated names untouched` | `png` / `webp` / `bmp` が素通り | pass |

否定側（畳んではいけないものを畳んでいないこと）のテストを併せて置いた。
別名表が広すぎる事故を検出するため。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **32 / 32 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **32 / 32 pass** |

不変条件スキャナ 7 種を個別にも実行し、全て ok を確認した。
**別名表のリテラルは `FormatId.hpp` にあるため INV3A に検出されない**（除外が意図どおり働いている）。

### 推測で埋めた箇所

**なし。** 別名の選定は Qt の MIME タイプの実測に基づく。
`QStringLiteral` を使わず `QStringView(u"...")` としたのは、
`QStringLiteral` の定義ヘッダが Qt 6.8 と 6.11 で異なり、
`misc-include-cleaner` が CI でのみ違反を出した前例（Phase 0）を踏まえたもの。

### 残課題 / 次にやること

1. **T3（`CapabilityTable` と `CapabilitySource` concept）に着手する。**
   上記「T3 への帰結」の重複統合を必ず実装する。
2. T3 で `katachi_core` を `INTERFACE` から `STATIC` へ変更する。
3. T3 で clang-tidy ゲートが core ヘッダに届くようになる。新たな指摘が出ないか確認する。
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T3 完了（`CapabilityTable` と `CapabilitySource` concept）

### 実施内容

能力表を実装した。指示のあった「正規化後の重複の統合」を `mergeById()` として実装し、
`buildFromQt()` と `fromCapabilities()` の両方に適用した。

### 判断を仰いだ事項と回答

`FormatCapability` の `supportsAlpha` / `isLossless` に相当する Qt の API が存在しないことが
実装中に判明した（`QImageIOHandler::ImageOption` の全列挙を確認済み）。停止して指示を仰いだ。

| # | 事項 | 判断（利用者回答） |
|---|---|---|
| 1 | `supportsAlpha` / `isLossless` の決め方 | **メモリ上の往復で実測する。** ADR-0007 |
| 2 | 書き出し不可な形式の 2 フィールド | **`false` にして根拠を記録する。** ADR-0007 |

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/core/CapabilityTable.hpp` | `FormatCapability` と `CapabilityTable` |
| `src/core/CapabilityTable.cpp` | 実測探針、`mergeById()`、契約の `static_assert` |
| `tests/core/capability_table_test.cpp` | 実行時テスト 10 本 + `static_assert` 3 本 |
| `docs/adr/0007-capability-probing.md` | 実測方式の決定と、判定不能時の扱い |

**変更**

| ファイル | 内容 |
|---|---|
| `src/core/CMakeLists.txt` | **`INTERFACE` → `STATIC`**（`CapabilityTable.cpp` が入ったため） |
| `src/core/Concepts.hpp` | `CapabilitySource` concept を追加 |
| `tests/CMakeLists.txt` | `capability_table_test.cpp` を追加 |

**削除**: なし

### 設計上の判断

**`Concepts.hpp` が `CapabilityTable.hpp` を include する。** 逆ではない。
`CapabilitySource` が `FormatCapability` を参照するため、逆向きにすると循環する。
そのため `static_assert(CapabilitySource<CapabilityTable>)` は **`CapabilityTable.cpp` に置いた**。
`docs/spec-core.md` §3 はこの `static_assert` をクラス定義の直後に示しているが、
ヘッダに置くと循環するため配置のみ変えた。契約はコンパイル時に検証される。

**`find()` の引数を `FormatId` の値渡しから `const FormatId&` に変えた。**
`performance-unnecessary-value-param` が error を出したため（`FormatId` は `QString` を持つ）。
`fromCapabilities()` は仕様どおり値渡しを維持し、`mergeById()` へ `std::move` して
実際に消費することで同チェックを満たした。

**`mergeById()` の統合規則**: 真偽値は論理和、`extensions` は和集合（重複除去 + 昇順）。
`QMap` を使うため結果は id 昇順で決定的になる。

### clang-tidy が core に届いた結果（T2 で予告した確認）

`CapabilityTable.cpp` が入ったことで、clang-tidy ゲートの対象が
`src/app` の 2 本から **`src/core/CapabilityTable.cpp` を含む 3 本**になった。
予告どおり新たな指摘が出たため、すべてコード側で解消した（抑制は一切していない）。

| 指摘 | 対処 |
|---|---|
| `readability-magic-numbers` / `cppcoreguidelines-avoid-magic-numbers`（色成分・画像サイズ） | 名前付き `constexpr` 定数へ |
| `readability-identifier-length`（ループ変数 `x` / `y`、変数 `id`） | `column` / `row` / `formatId` へ改名 |
| `misc-include-cleaner`（`std::vector` / `std::optional` / `QList` 等） | 直接 include を追加 |
| `modernize-return-braced-init-list` | `return {first, last};` へ |
| `modernize-use-ranges` | `std::ranges::find_if` / `std::ranges::copy_if` へ |
| `performance-unnecessary-value-param` | `find()` を `const&` に、`mergeById()` で `std::move` 消費 |
| `performance-move-const-arg` | `QMap::insert` は値を `const&` で受けるため `std::move` を外した |

### 追加・変更したテスト（10 本追加。32 → 42）

| テスト | 期待値 | 結果 |
|---|---|---|
| `buildFromQt reports a non empty encodable set including PNG` | `encodable()` が空でなく、PNG が `canEncode` | pass |
| `buildFromQt yields exactly one entry per FormatId` | 重複 id が無い。Qt の全報告名を正規化しても行き先が必ず 1 件存在する | pass |
| `buildFromQt merges jpeg aliases into a single entry` | `jpeg` / `jpg` / `jfif` が同一 id。`extensions` に別名が残る | pass |
| `buildFromQt classifies PNG as lossless with alpha` | `supportsAlpha` かつ `isLossless` | pass |
| `buildFromQt classifies JPEG as lossy without alpha` | `!supportsAlpha` かつ `!isLossless` かつ `supportsQuality` | pass |
| `buildFromQt leaves unprobeable fields false for read only formats` | 書き出し不可な全形式で 3 フィールドが `false` | pass |
| `find returns nullopt for an unknown format` | 未登録 id で `std::nullopt` | pass |
| `fromCapabilities merges entries that normalize to the same id` | 3 件が 1 件へ。真偽値は論理和、`extensions` は和集合 | pass |
| `encodable returns only entries that can encode` | `canEncode` の項目のみ | pass |
| `encodable is ordered deterministically` | id 昇順で整列している | pass |

`static_assert`: 肯定 `CapabilitySource<CapabilityTable>`、
**否定 `!CapabilitySource<int>` と `!CapabilitySource<FormatId>`**。

読み込み専用形式のテストは、どの形式が読み込み専用かが環境で変わるため、
`QImageReader` と `QImageWriter` の差集合をテスト側で計算している。
`spec-core.md` §3 に無いメンバ（全項目を返す取得子）を足さずに済ませるため。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **42 / 42 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev`（対象 3 ファイル） | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **42 / 42 pass** |

### 推測で埋めた箇所

**なし。** `supportsAlpha` / `isLossless` の判定方式は実測に基づき、
判定不能な場合の扱いは指示を仰いで決めた。

### 残課題 / 次にやること

1. **T4（テストフィクスチャの生成）に着手する。** `tests/fixtures/generate.cpp`。
   `docs/phases.md` §2.4: 自前生成のみ・各 50KB 未満・生成スクリプトを残す。
2. その後 T5（`ConversionSpec` と `convert()`）。**Phase 1 の心臓部。**
3. `isLossless` は白黒 2 値の探針で判定するため、色を表現できない形式
   （`pbm` / `xbm` 等）も `lossless` と出る。任意の入力に対する可逆性ではない。
   ADR-0007 に明記済み。**T5 のラウンドトリップテストで形式を選ぶ際に注意する。**
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T4 完了（テストフィクスチャの生成）

### 実施内容

`tests/fixtures/generate.cpp` を実装し、T5 が必要とする画像を生成できるようにした。
生成器は**自分で検証し、意図した性質が保たれていなければ非ゼロ終了する。**

### 決めた事項: 生成物はコミットせずビルド時に生成する

T0 の計画で「実装時に決める」としていた点。**ビルド時生成**を採った。

- 生成器と成果物が乖離しない（コミットすると `generate.cpp` を直しても古い画像が残りうる）
- git にバイナリを置かない（レビューできない差分を作らない）
- `< 50KB` の制約を生成のたびに機械的に検査できる

`docs/phases.md` §2.4 は「`tests/fixtures/` には自前で生成した小さな画像のみ」と書いており、
その置き場所がリポジトリ内かビルドディレクトリかは指定していない。
生成器を残すという要件（同 §2.4）は満たしている。

`git ls-files` で画像バイナリが 1 つも管理下に無いことを確認済み。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `tests/fixtures/generate.cpp` | フィクスチャ生成器。大きさと性質を自己検証する |
| `tests/core/fixtures_test.cpp` | フィクスチャが T5 の前提を満たすかのテスト 7 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `tests/CMakeLists.txt` | `katachi_fixture_gen` と `katachi_fixtures` ターゲット、`KATACHI_FIXTURE_DIR` の注入 |

**削除**: なし

### 生成するフィクスチャ

| ファイル | 大きさ | 用途 |
|---|---|---|
| `gradient_rgb.png` | 347 B | ラウンドトリップ / PSNR / 決定性。アルファ無し |
| `gradient_alpha.png` | 231 B | アルファ表。完全透明と完全不透明の画素を必ず含む |
| `with_text.png` | 386 B | テキスト metadata の保持 |
| `with_icc.png` | 664 B | ICC プロファイルの保持（DisplayP3） |
| `oriented.tiff` | 12492 B | 向き metadata の保持（Rotate90） |
| `not_an_image.bin` | 64 B | `ConvertError::DecodeFailed` |

すべて 50KB 未満。乱数も時刻も使わない固定パターンで、同じ環境なら常に同じ内容になる。

### 生成器が検出した Qt の挙動 2 件（T5 に直結する重要な知見）

いずれも**推測ではなく、生成器の自己検証が失敗したことから切り分けて実測で確定させた。**

#### 1. PNG のテキストは `QImageReader::text()` では読めない

書き出しは `QImageWriter::setText()` でも `QImage::setText()` でも保持される。
しかし読み取りでは `QImageReader::textKeys()` が**空**になり、
`QImageReader::text()` も空文字を返す。
**デコード後の `QImage::text()` からしか読めない。**

```
/tmp/u_writer.png -> QImage::text='katachi fixture'  QImage::textKeys=[Description]
/tmp/u_image.png  -> QImage::text='katachi fixture'  QImage::textKeys=[Description]
```

**T5 でメタデータ保持を実装するとき、読み取りは `QImage::text()` / `QImage::textKeys()` を使う。**

なお、テキスト付き PNG の読み込み時に libpng が標準エラーへ
`libpng error: Read Error` を出す。`read()` は成功し内容も正しいため、無害な雑音として扱う。

#### 2. JPEG に向き metadata は書けない。Qt がピクセルへ焼き込む

当初 `oriented.jpeg` を作ろうとしたが、生成器の検証が
「向き metadata が保持されていない」で落ちた。切り分けた結果。

| 形式 | 書き出し | 読み戻しの transformation | 寸法（入力 16x8） |
|---|---|---|---|
| jpeg | 成功 | **None** | **8x16**（回転が焼き込まれた） |
| tiff | 成功 | **Rotate90** | 16x8（維持） |

JPEG のファイルに `Exif` マーカーは無かった。
これは `QImageWriter::setTransformation()` の
"If transformation metadata is not supported by the image format,
the transform is applied before writing" に対応する挙動である。

**フィクスチャを `oriented.tiff` に変更した。**

**T5 と ADR-0003 への影響**: 「向きの保持」は形式によって手段が変わる。
metadata として保持できる形式（TIFF 等）と、ピクセルへ焼き込まれる形式（JPEG）がある。
どちらも視覚的な向きは保たれるが、**同じ「保持」ではない。**
T5 で `MetadataPolicy::PreserveSupported` を実装する際に、この差を踏まえて
ADR-0003 に追記する。

### 追加・変更したテスト（7 本追加。42 → 49）

| テスト | 期待値 | 結果 |
|---|---|---|
| `every fixture stays under the 50KB limit` | 6 ファイルすべてが存在し、0 < size < 50KB | pass |
| `gradient_rgb has no alpha channel` | `hasAlphaChannel()` が false | pass |
| `gradient_alpha carries a fully transparent pixel` | アルファ有り、(0,0) の alpha が 0 | pass |
| `with_text carries text metadata` | `QImage::text("Description") == "katachi fixture"` | pass |
| `with_icc carries a colour space` | `colorSpace().isValid()` | pass |
| `oriented carries orientation metadata` | `transformation() != TransformationNone` | pass |
| `not_an_image cannot be decoded` | `QImage::fromData()` が null | pass |

生成器もビルド時に同等の検証を行う。二重になるが、
**生成器の検証はビルドを止め、テストは「テストから見て使える状態か」を確認する**もので、
落ちたときの切り分けが変わる。T5 の変換テストが落ちたとき、
それが `convert()` の不具合かフィクスチャの不備かを分けられるようにしている。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **49 / 49 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 / 指摘 0 件 |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **49 / 49 pass** |

`git ls-files` に画像バイナリが 1 つも無いことも確認した。

### 推測で埋めた箇所

**なし。** Qt の挙動 2 件はいずれも実測で確定させた。

### 残課題 / 次にやること

1. **T5（`ConversionSpec` と `convert()`）に着手する。Phase 1 の心臓部。**
   上記の知見 2 件（テキストの読み取り経路、向きの形式差）を実装に反映する。
2. `oriented.tiff` が 12.5KB とやや大きい。TIFF は非圧縮寄りのため。
   50KB 制限内だが、他のフィクスチャ（1KB 未満）と比べて突出している。
3. `isLossless` は白黒 2 値の探針で判定するため、色を表現できない形式も lossless と出る
   （ADR-0007）。**T5 のラウンドトリップテストで形式を選ぶ際に注意する。**
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T5 完了（`ConversionSpec` と `convert()`）

### 実施内容

Phase 1 の心臓部。純粋関数 `convert()` を実装した。
テストを先に書き（期待値は `docs/spec-core.md` から導いた）、実装した。

**サブエージェントの使い方**: 利用者から「適宜使ってよい」との指示があったため、
`docs/agent-protocol.md` §3 の範囲（**調査のみ**。`src/core/` の実装とテストの期待値の
決定には使わない）で Qt API の挙動調査を 2 件並列で走らせた。
**実装とテストの期待値はすべて自分で書いた。**

### 停止して判断を仰いだ事項 2 件

どちらも品質ゲートが落ちた状態で停止し、指示を得てから進めた。

#### 1. `bugprone-exception-escape` と ADR-0002 が原理的に両立しない

```
Converter.cpp: an exception may be thrown in function 'convert' which should not throw exceptions
  note: 'length_error' may be thrown ... '__emplace_back_slow_path<ConvertWarning>'
```

ADR-0002 は「`noexcept` を維持し確保失敗時の `std::terminate` を意図的に受け入れる」と
決めている。この検査はまさにその形を禁止する。発生源は `std::vector::push_back` だが、
`ConversionOutput::warnings` が `std::vector` である以上（`spec-core.md` §2、ADR-0004）、
確保を伴う限り必ず反応する。**リンタを黙らせるために承認済みの型定義を歪めることはしなかった。**

**判断（利用者回答）: `.clang-tidy` から除外する。**
`phases.md` §3 が「除外可」とした 2 種を超える 3 つ目の除外のため、
`.clang-tidy` / `phases.md` §3 / ADR-0002 の 3 箇所に根拠を書いて相互参照させた。
抑制コメントは使っていない。

#### 2. ADR-0003 と `IccPolicy` の責務が重複していた

ADR-0003（T0 で私が書いたもの）は `PreserveSupported` を
「向き + テキスト + **ICC** を保持」と定義していた。しかし `spec-core.md` §2 は
`MetadataPolicy` と `IccPolicy` を**別フィールド**として持つため、
`StripAll` + `IccPolicy::Embed` の組み合わせで `IccPolicy` が無意味になる。

**判断（利用者回答）: 直交させ、ADR-0003 を訂正する。**
`MetadataPolicy` = 向き + テキスト、`IccPolicy` = ICC。互いを上書きしない。
ADR-0003 に「訂正」節を追加し、T4 で判明した
「向きの保持は形式により metadata か焼き込みかが変わる」
「テキストは `QImage::text()` からしか読めない」も併せて記録した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/core/ConversionSpec.hpp` | `AlphaPolicy` / `MetadataPolicy` / `IccPolicy` / `ConversionSpec` |
| `src/core/Converter.hpp` | `ConversionOutput` と `convert()` の宣言 |
| `src/core/Converter.cpp` | `convert()` の実装。非テンプレートで `.cpp` に閉じる |
| `tests/core/converter_test.cpp` | 実行時テスト 22 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `.clang-tidy` | `bugprone-exception-escape` を除外（根拠コメント付き） |
| `docs/phases.md` §3 | 3 つ目の除外を追記 |
| `docs/adr/0002-noexcept-and-allocation.md` | 除外の経緯を追記 |
| `docs/adr/0003-metadata-policy.md` | ICC の責務についての訂正節を追加 |
| `src/core/CMakeLists.txt` / `tests/CMakeLists.txt` | 新規ファイルの登録 |

**削除**: なし

### 実装の要点

**処理順序を固定した**（決定性のため。入れ替えない）。

```
空判定 -> 出力形式の妥当性 -> maxPixels 事前判定 -> 復号
      -> リサイズ -> アルファ -> メタデータ / ICC -> 符号化
```

- **`maxPixels` は復号前に判定する**（ADR-0002）。`QImageReader::size()` で
  ヘッダの寸法を見てから `read()` する。復号後の実寸でも再確認する
  （ヘッダの寸法が信用できない形式があるため）
- **アルファ合成はプリマルチプライド前提で行わない**（`spec-core.md` §4）。
  `Format_ARGB32` に正規化 → `flattenColor` で塗ったキャンバスへ
  `CompositionMode_SourceOver` で描画
- **`reader.setAutoTransform(false)`** を明示する。true だと向きが画素へ適用され、
  metadata としての保持も除去も選べなくなる
- **テキストの除去には「生ビットから QImage を作り直す」手段を使った。**
  Qt に「全テキストを消す」API が無いため。色空間は明示的に引き継ぐ
  （`MetadataPolicy` は ICC に関与しないため）
- `quality` は `[0, 100]` に clamp する。`spec-core.md` §2 は範囲を示すが
  範囲外に対するエラー値を定義していないため

### 追加・変更したテスト（22 本追加。49 → 71）

**`docs/phases.md` §2.2 の全種別を満たした。**

| 種別 | テスト | 期待値 |
|---|---|---|
| エラー | 6 本 | `ConvertError` の**全 6 列挙値**を発生させる |
| アルファ | 5 本 | `spec-core.md` §4 の**全 5 行** |
| ラウンドトリップ | 2 本 | PNG→PNG、PNG→BMP が**ピクセル完全一致** |
| 非可逆 | 1 本 | PNG→JPEG の **PSNR ≧ 35dB**（バイト比較しない） |
| 決定性 | 1 本 | png / jpeg / bmp / tiff の 4 形式で 2 回変換し**バイト列完全一致** |
| リサイズ | 1 本 | 64x64 を 32x16 に収めると **16x16**（アスペクト比保持） |
| メタデータ | 6 本 | テキスト / ICC / 向きの保持と除去（各 2 方向） |

`EncodeFailed` は「能力表が書けると言っているが Qt は書けない」形式を
`fromCapabilities()` で作って発生させた。環境に依存しない。

アルファの警告は**両方向**を確認している。
2 行目で `AlphaFlattenedFallback` がちょうど 1 件積まれること、
**それ以外の行では `warnings` が空であること**（警告が濫発されないこと）。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **71 / 71 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **71 / 71 pass** |

### 差分規模の申告（停止条件 8）

**T5 の差分は 724 行で、事前に伝えた「400 行に近づく見込み」を超えた。**
Phase 1 全体としての 400 行超過は承認済みだが、
タスク単位でも 400 行以内に収める見込みだと述べていた点は外れた。
内訳は実装 3 ファイルで約 260 行、テスト 1 ファイルで約 330 行、文書が残り。
**テストが実装より大きいのは §2.2 の種別を全て満たしたためで、削っていない。**

### 推測で埋めた箇所

**なし。** 矛盾 2 件はいずれも自分で判断せず停止して指示を仰いだ。

### 残課題 / 次にやること

1. **T6（`NamingRule`）に着手する。**
2. その後 T7（`docs/format-matrix.md` のビルド時自動生成）で Phase 1 完了。
3. Qt API 調査のサブエージェント 2 件の結果がまだ届いていない。
   届いたら、実装の判断（テキスト除去の手段、ICC の落とし方、`autoTransform` の既定値、
   `QPainter` を `QCoreApplication` だけで使えること、アルファ合成の数値的正しさ）と
   食い違いがないか照合する。**食い違いがあれば報告する。**
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T5 追補（サブエージェント調査との照合で実装の欠陥 2 件を修正）

### 実施内容

利用者の指示で使ったサブエージェント 2 件（Qt API の調査）の結果が届いたため、
`docs/agent-protocol.md` §3 に従い**自分の実装と照合した。その結果、実装の欠陥が 2 件見つかった。**
落ちるテストとして固定してから修正した。

**サブエージェントの使い方**: §3 の「使ってよい領域（調査のみ）」に限定した。
`src/core/` の実装とテストの期待値の決定には使っていない。
調査結果には実測出力と根拠 URL を付けさせた。

### 欠陥 1: 合成すると ICC が消える

`QPainter` は描画先へ色空間を伝播しない。そのため `flattenOnto()` で合成した時点で
入力の `QColorSpace` が失われ、**`IccPolicy::Embed` の指定が無視されていた。**

修正: 合成後に `canvas.setColorSpace(source.colorSpace())` で明示的に引き継ぐ。

**テストの前提を一度誤った。** 当初この回帰テストの出力形式を BMP にしたが落ち続けた。
原因を確定させるため、`convert()` を通さず Qt が直接書いた場合を実測した。

```
bmp   write=1  readback colorSpace valid=0
jpeg  write=1  readback colorSpace valid=1
png   write=1  readback colorSpace valid=1
tiff  write=1  readback colorSpace valid=1
```

**BMP はそもそも ICC を保持できない。** 実装ではなくテストの前提が誤っていた。
出力形式を「アルファ非対応（＝合成が起きる）かつ ICC を保持できる」JPEG に変更した。
**期待値は緩めていない。** 検証したい性質（合成で色空間を落とさない）はそのままで、
成立しない実験条件を正した。

### 欠陥 2: テキスト除去で索引色画像が壊れる

`withoutText()` は生ビットから `QImage` を作り直すが、この方法は**テキスト以外の付随情報も落とす**。
特に**カラーテーブルが失われる**ため、`Format_Indexed8` の画像は
画素値（インデックス）が同じでも見た目が別物になる。
DPI・`devicePixelRatio` も既定値へリセットされる。

修正: `setColorTable()` / `setDotsPerMeterX/Y()` / `setDevicePixelRatio()` /
`setColorSpace()` を明示的に戻す。

検出のため**索引色フィクスチャ `indexed.png` を追加した**（4 色パレット、テキスト付き）。

### 併せて行った改善: 合成の丸め精度

調査で、`QPainter` の `SourceOver` は描画先のフォーマットで精度が変わることが実測された
（16,777,216 サンプル）。

| 描画先 | 理論値と完全一致 | 最大誤差 |
|---|---|---|
| `Format_RGB32` / `ARGB32_Premultiplied` | 76.3% | 0.98 LSB |
| `Format_ARGB32` / `RGBA8888` | **99.995%** | **0.51 LSB** |

`flattenOnto()` のキャンバスを `RGB32` から `ARGB32` に変え、最後に `RGB32` へ落とす形にした。
1 行の変更で誤差が半減する。`docs/spec-core.md` §4 が「取りこぼしやすい」と明示した箇所であり、
精度を上げておく価値があると判断した。

### 前エントリの訂正: `QImageReader::text()` について

T4 で「PNG のテキストは `QImageReader::text()` では取れない」と書いたが、**言い過ぎだった。**
調査によれば、取れるかどうかは **tEXt チャンクが IDAT の前か後か**、
および **`read()` の前か後か**で変わる。Qt 自身が書いた PNG は IDAT の前に置くため
`read()` 前なら取れる。

**結論（`QImage::text()` を使う）は変わらない**が、理由が違った。
`QImage::text()` は取りこぼしが無いから使う、が正しい。
コードとテストのコメントを訂正した。

### 調査で判明したが今回は対応していないこと（記録）

1. **ICO を読むと Qt が `_q_icoOrigDepth` という内部キーを注入する。**
   `PreserveSupported` で ICO → PNG に変換すると、この内部キーが tEXt として出力に漏れる。
   Phase 2 以降で ICO を扱うときに検討する。
2. **ICNS は `supportsOption(Description)` が false を返すのに、内包する PNG 経由で
   テキストがファイルに残る。** `supportsOption` は「バイト列にテキストが残らない保証」ではない。
3. **`QImageWriter::setText()` は値を `simplified()` で潰し、キーに空白や `:` があると壊す。**
   本実装は書き込みに `QImageWriter::setText()` を使っていないため影響を受けない。
   フィクスチャ生成器では使っているが、単純なキーと値のみなので実害はない。
4. **JPEG は `supportsOption(ImageTransformation)` が true を返すのに向き metadata を書かない。**
   この戻り値を「metadata で保持できる形式か」の判定に使うと誤判定する。本実装は使っていない。
5. **`autoTransform` の既定値は `false`。** 実測とソース（`UsePluginDefault` は無条件に false を返す）
   の両方で確認された。公式ドキュメントに既定値の記述は無い。
   本実装は既定に頼らず明示的に `setAutoTransform(false)` している。
6. **`QPainter` は `QCoreApplication` だけで動く**（ラスタ描画のみ。`QFontDatabase` に触れると qFatal）。
   本実装はテキスト描画をしないため問題ない。

### 変更ファイル

**変更**

| ファイル | 内容 |
|---|---|
| `src/core/Converter.cpp` | `flattenOnto()` の色空間引き継ぎと ARGB32 キャンバス化、`withoutText()` の付随情報復元 |
| `tests/fixtures/generate.cpp` | 索引色フィクスチャ `indexed.png` を追加、コメント訂正 |
| `tests/core/converter_test.cpp` | 回帰テスト 2 本を追加 |
| `tests/core/fixtures_test.cpp` | `indexed.png` を大きさ検査の対象に追加、コメント訂正 |

**追加 / 削除**: なし

### 追加・変更したテスト（2 本追加。71 → 73）

| テスト | 期待値 | 修正前 | 修正後 |
|---|---|---|---|
| `flatten keeps the colour space when icc is embed` | アルファ合成が起きても `IccPolicy::Embed` なら出力の色空間が有効 | **fail** | pass |
| `strip all keeps indexed colours intact` | `StripAll` 後もテキストは消え、**索引色の見た目は変わらない** | **fail** | pass |

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **73 / 73 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **73 / 73 pass** |

### 推測で埋めた箇所

**なし。** サブエージェントの主張を鵜呑みにせず、
**欠陥 2 件はいずれも自分のテストで再現させてから修正した。**
BMP が ICC を保持できないことも、自分で実測して確定させた。

### 残課題 / 次にやること

1. **T6（`NamingRule`）に着手する。**
2. その後 T7（`docs/format-matrix.md` のビルド時自動生成）で Phase 1 完了。
3. 上記「対応していないこと」1〜3 は Phase 2 以降の検討事項として残る。
4. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T6 完了（`NamingRule`）

### 実施内容

出力ファイル名の生成を実装した。ADR-0005 のとおり**名前を組み立てるだけで衝突の解決はしない。**

### 計画時に書いた期待値の訂正

T0 の計画で命名規則のテスト期待値を
`resolveOutputName("photo", 1, "{name}_{index:03}", "png")` → `"photo_001.png"`
と書いたが、**`docs/spec-core.md` §5 の例は `"{name}_{index:03}.{ext}"` であり、
パターン自体に `.{ext}` を含む。** 仕様の例を正とし、テストもそちらで書いた。

### 停止して判断を仰いだ事項 2 件

#### 1. INV3A（core の文字列リテラル全面禁止）が正当な実装を阻んだ

`NamingRule.cpp` の予約語 `{name}` / `{ext}` / `{index}` を検出して落ちた。

```
core/NamingRule.cpp:83: core 層の文字列リテラル（FormatId.hpp 以外では全面禁止）
  > if (key == QStringView(u"name")) {
```

**全面禁止は私が T2 で足した強化だった。** `docs/spec-core.md` §3 の原文は
「**フォーマット名の**文字列リテラルを書かない」であり、そこに全面禁止は無い。
`QChar` を並べてスキャナを回避する実装は書かなかった（clang-tidy のときと同じ理由）。

**判断（利用者回答）: 仕様どおり名前一覧判定に戻す。**
INV3A を INV3B と同じ「フォーマット名の一覧との照合」に統一した。
`docs/spec-core.md` §3 にも、禁止対象がフォーマット名であることを明記した。

戻したあと 3 方向で確認した。

| 確認 | 結果 |
|---|---|
| `src/core` の予約語リテラル（フォーマット名でない） | **通る**（意図どおり） |
| `Result.hpp` に `"png"` を入れる | **検出**（`core/Result.hpp:50`） |
| 違反フィクスチャ | **検出**（`core/StringLiteral.hpp:7`） |

`FormatId.hpp` の除外は維持されている。

#### 2. `bugprone-easily-swappable-parameters` が仕様のシグネチャを指摘

`resolveOutputName(..., const QString& pattern, const QString& extension)` の
隣接した同型 2 引数を指摘された。**指摘自体は妥当**（呼び出し側が取り違えうる）。

**判断（利用者回答）: 強い型を導入し仕様を更新する。**

```cpp
struct NamePattern   { QString v; };
struct NameExtension { QString v; };
```

`FormatId` と同じ「強い型付き文字列」の考え方（`spec-core.md` §2.1）で一貫させた。
**リンタを黙らせるためではなく、取り違えを型で塞ぐという既存方針の適用である。**
`docs/spec-core.md` §5 を更新した。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `src/core/NamingRule.hpp` | `NamingError` / `NamePattern` / `NameExtension` / `resolveOutputName()` |
| `src/core/NamingRule.cpp` | 実装 |
| `tests/core/naming_rule_test.cpp` | 実行時テスト 15 本 + `static_assert` 1 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `tests/invariants/scan_invariants.cmake` | INV3A を名前一覧判定へ統一（INV3B と同じ経路） |
| `tests/invariants/fixtures/violations/inv3a/core/StringLiteral.hpp` | フォーマット名を含む違反に差し替え |
| `docs/spec-core.md` §3 / §5 | 禁止対象の明確化、強い型の導入 |
| `src/core/CMakeLists.txt` / `tests/CMakeLists.txt` | 新規ファイルの登録 |

**削除**: なし

### clang-tidy の指摘 8 件と対処

core に新しい `.cpp` が入ったため、まとめて指摘が出た。すべてコード側で解消した。

| 指摘 | 対処 |
|---|---|
| `misc-include-cleaner`（`cstdint` 不要 / `qint64` / `qsizetype`） | include を整理し `<QtTypes>` を追加 |
| `readability-math-missing-parentheses` | `(width * decimalBase) + ...` と明示 |
| `readability-magic-numbers`（10） | `decimalBase` 定数へ |
| `readability-function-cognitive-complexity`（30 > 25） | プレースホルダ展開を `expandPlaceholder()` へ切り出し |
| `bugprone-easily-swappable-parameters`（`padIndex`） | 桁数を `IndexWidth` 型で包む |
| `bugprone-easily-swappable-parameters`（`resolveOutputName`） | 上記の判断 2 のとおり強い型を導入 |

**認知的複雑度の指摘は正当だった。** 切り出しで実際に読みやすくなった。

### 追加・変更したテスト（15 本追加。73 → 88）

| 種別 | 内容 |
|---|---|
| 正常系 | 仕様の例そのもの / 桁指定なし / 0 詰め / 桁より長い数は切り詰めない / 負数の符号 / 前後の literal / 同じ名前の複数回 / 決定性 |
| エラー | **`NamingError` の全 4 列挙値**（`EmptyPattern` / `UnknownPlaceholder` / `InvalidIndexSpec` / `EmptyResult`） |
| 境界 | 閉じ括弧なし / 桁指定が数字でない 4 パターン / 桁指定が過大（上限 32） / 桁指定を取らない `{name:03}` |

`{index}` の桁指定に上限（32）を設けたのは、上限が無いと巨大な指定で確保が走り
`std::terminate` しうるため（ADR-0002）。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **88 / 88 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **88 / 88 pass** |

### 推測で埋めた箇所

**なし。** 判断が必要な 2 件はいずれも停止して指示を仰いだ。

### 残課題 / 次にやること

1. **T7（`docs/format-matrix.md` のビルド時自動生成）で Phase 1 完了。**
2. INV3A を名前一覧判定に戻したため、**core にフォーマット名以外の文字列リテラルが
   書けるようになった。** 表示用の文言を core に書いてしまう余地が生まれたが、
   それは `CLAUDE.md` の「core に UI 文言を置かない」という設計方針で担保する（機械検査は無い）。
3. Windows の実機起動確認は Phase 0 から未了のまま。

---

## 2026-08-09 — T7 完了（`docs/format-matrix.md` の自動生成）。Phase 1 のローカル作業完了

### 実施内容

能力表から `docs/format-matrix.md` をビルド時に生成する仕組みを作った。
これで T0〜T7 が揃い、Phase 1 のローカル作業は完了。

### 計画に無かった追加（申告）

**`tools/` ディレクトリを新設した。** `docs/spec-core.md` §1 のディレクトリ構成に
`tools/` は無かった。承認いただいた計画では「`tools/` 配下 + CMake ターゲット」と
書いていたが、§1 の木を更新する話は書いていなかった。§1 に 1 行追記した。

生成器の置き場所として `src/app`（Widgets に依存してしまう）も
`tests/`（テストではない）も適さないため、独立させた。

### 変更ファイル

**追加**

| ファイル | 内容 |
|---|---|
| `tools/format_matrix.cpp` | 生成器。表が空 / PNG が無い場合は非ゼロ終了する |
| `cmake/FormatMatrix.cmake` | 生成ターゲットとテストへの配線 |
| `tests/core/format_matrix_test.cpp` | 生成物の検証 5 本 |

**変更**

| ファイル | 内容 |
|---|---|
| `CMakeLists.txt` | `include(cmake/FormatMatrix.cmake)` |
| `tests/CMakeLists.txt` | `format_matrix_test.cpp` を追加 |
| `docs/spec-core.md` §1 | `tools/format_matrix.cpp` を木に追記 |

**削除**: なし

### 設計上の判断

**生成物はコミットしない**（`.gitignore` に登録済み。Phase 0 から）。
能力表は実行環境の Qt プラグイン構成で決まるため、コミットすると
「誰かの環境の一覧」が正になってしまう。`git ls-files` で 0 件を確認済み。

**`CapabilityTable` に全項目を返す取得子を足さなかった。** `docs/spec-core.md` §3 は
`find()` と `encodable()` のみを定めている。読み込み専用の形式も一覧に載せる必要が
あるため、生成器側で Qt の報告する名前を正規化・重複除去してから `find()` で引いた。
T3 のテストと同じ手口。**仕様に無い API を足さずに済ませた。**

**生成器は自分で中身を検証する。** 表が空、あるいは PNG が能力表に無い場合は
非ゼロ終了してビルドを止める。中身の無い一覧を黙って出力しない。

### 生成結果（ローカル macOS 14 / arm64、Qt 6.11.1）

22 形式が並んだ。ADR-0006 の別名統合が効いていることが一覧からも見える。

| 確認点 | 結果 |
|---|---|
| `jpeg` の行の拡張子 | `jfif, jpeg, jpg`（3 別名が 1 行に統合） |
| `tiff` の行の拡張子 | `tif, tiff` |
| `jpg` / `jfif` 単独の行 | **無い**（統合済み） |
| 読み込み専用（`gif` / `pdf` / `svg` / `svgz` / `tga`） | 3 つの実測列がすべて `-` |
| `png` | 読み書き・アルファ・品質・可逆すべて `o` |

`-` が「その性質が無い」ではなく「判定していない」を意味することを本文に明記した（ADR-0007）。

### 追加・変更したテスト（5 本追加。88 → 93）

| テスト | 期待値 |
|---|---|
| `the format matrix is generated and not empty` | ファイルが存在し、表の見出し行を含む |
| `the format matrix says it is generated` | 「ビルド時に自動生成」と生成元パスが本文にある |
| `the format matrix lists png as readable and writable` | `png` の行が読み書きとも `o` |
| `the format matrix shows merged alias extensions` | `jpeg` に別名が並び、**`jpg` / `jfif` 単独の行が無い** |
| `the format matrix explains what a dash means` | 「判定していない」の説明が本文にある |

生成器自身もビルド時に検証するが、こちらは「生成物が実際に置かれ、内容を反映しているか」を見る。
落ちたときの切り分けが変わる。

### 品質ゲートの実行結果（ローカル macOS 14 / arm64。ステージ済みの状態で実行）

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | exit 0 / **warning・error 0 行** |
| 2 | `ctest --preset dev --output-on-failure` | exit 0 / **93 / 93 pass** |
| 3 | `clang-format --dry-run --Werror` | exit 0 |
| 4 | `clang-tidy -p build/dev` | exit 0 / **指摘 0 件** |
| 5 | `cmake --build --preset asan` | exit 0 / 警告 0 |
| 6 | `ctest --preset asan --output-on-failure` | exit 0 / **93 / 93 pass** |

### Phase 1 受け入れ基準（`docs/phases.md` §4）

| 項目 | 状態 |
|---|---|
| `convert()` が純粋関数として実装され、ファイル I/O・時刻・グローバル状態に触れていない | **達成**（`QBuffer` のみ。不変条件スキャナと依存方向で担保） |
| `docs/adr/0002-noexcept-and-allocation.md` に `noexcept` 判断が記録されている | **達成**（`bugprone-exception-escape` 除外の経緯も追記済み） |
| `CapabilityTable::buildFromQt()` が実行時に能力表を生成する | **達成** |
| §2.2 の全テスト種別が存在し、green | **達成**（ラウンドトリップ / 非可逆 PSNR / 決定性 / アルファ全行 / エラー全列挙 / 能力表 / 不変条件 / 依存方向 / concept 適合） |
| `ConvertError` の全列挙値にテストがある | **達成**（6 / 6） |
| 不変条件スキャナ 6 種が引き続き green | **達成**（7 検査 + 検出確認 7 = 14 テスト） |
| `src/core/Concepts.hpp` に core 層の concept が集約され、肯定・否定両方の `static_assert` がある | **達成**（`ResultValue` / `ResultError` / `CapabilitySource` の 3 つとも両方あり） |
| `Converter.hpp` がテンプレートになっていない（実装が `.cpp` に閉じている） | **達成**（`grep template` が 0 件） |
| 決定性テストが green | **達成**（png / jpeg / bmp / tiff の 4 形式。除外形式は無し） |
| `docs/format-matrix.md` がビルド時に自動生成される | **達成** |

**10 項目すべて達成。CI では未検証**（このブランチは未 push）。

### 推測で埋めた箇所

**なし。**

### 残課題 / 次にやること

1. **`phase1` ブランチを push し、CI（macOS / Windows）で確認する。** 未実施。
   Qt 6.8 と 6.11 の差、Windows 固有の挙動がここで出る可能性がある。
   特に **T3 の能力表実測と T4 のフィクスチャ生成は環境依存**であり、
   Windows で異なる結果になれば関連テストが落ちうる。
2. その後 PR を作成して `main` へマージ。
3. Windows の実機起動確認は Phase 0 から未了のまま。
4. Phase 2 着手時の宿題: `docs/phases.md` §5.3（バッチ実行時のメモリ上限）、
   衝突ポリシーの io 層での実装、ICO の内部キー `_q_icoOrigDepth` の扱い。

---

## 2026-08-09 — `phase1` を push。CI が環境差を検出し、修正して green

### 実施内容

`phase1` を push した（10 コミット / 37 ファイル / +4599 行）。
**初回 CI は 4 ジョブすべて失敗した。** 原因を特定して修正し、再実行で全 green になった。

### 初回 CI の失敗（run 31288967302）と原因

4 ジョブとも**同一の原因**だった。

```
書き出しに失敗: .../oriented.tiff (Unsupported image format)
FAILED: tests/fixtures/gradient_rgb.png
```

**フィクスチャ生成器が TIFF を書けず、設計どおりビルドを止めた。**
T4 で「意図した性質が保たれなければ非ゼロ終了する」を入れた狙いが働いた。
黙って壊れたフィクスチャを置いていたら、T5 の変換テストが
「convert() の不具合」に見える形で落ちていた。

### 根本原因（推測ではなく確認した）

ローカルの `~/Qt/components.xml` に `qt.qt6.6111.addons.qtimageformats` が
インストール済みとして記録されていた。**`qtimageformats` は Qt の別アドオンモジュールで、
`install-qt-action` は既定でインストールしない。**

| モジュール | プラグイン（macOS 実測） |
|---|---|
| `qtbase` | png（組込）/ `qjpeg` / `qgif` / `qico` / `qpdf` / `qsvg` |
| **`qtimageformats`（アドオン）** | **`qtiff` / `qwebp` / `qjp2` / `qicns` / `qtga` / `qwbmp` / `qmacheif`** |

ローカルは公式インストーラの全部入りで `qtimageformats` を含むため、差が出ていた。
PNG / JPEG / BMP / GIF / ICO が CI でも動いていたのは、それらが `qtbase` 由来だから。

**`docs/phases.md` §1.5 は「CI は `aqtinstall`」としか書いておらず、
モジュールの指定が無かった。** ここが穴だった。

### 修正

`.github/workflows/ci.yml` の Qt 導入ステップ 3 箇所すべてに
`modules: qtimageformats` を追加した。

**`docs/licenses.md` §4 にモジュール別の内訳を追記した。**
Phase 4 で `qtimageformats` を同梱するなら、その中の第三者ライブラリ
（libtiff / libwebp 等）のライセンス文も `third_party_licenses.txt` に
含める必要がある。**配布物の構成に影響する事項なので記録した。**

### 修正後の CI（run 31289109546 / `1af5e0d`）

| ジョブ | 結果 |
|---|---|
| ビルド + テスト (macOS 14 arm64、**Qt 6.8.3**) | **success / 93 / 93 pass** |
| ビルド + テスト (Windows 2022 MSVC、**Qt 6.8.3 msvc2022_64**) | **success / 93 / 93 pass** |
| clang-format + clang-tidy (macOS、22.1.8) | **success** |
| ASan + UBSan (macOS) | **success / 93 / 93 pass** |

CI ログ全 3574 行に `warning:` / `error:` を含む行は **0 件**。

**Windows でもフィクスチャ生成の検証がすべて通った。**

```
  oriented.tiff  12492 bytes
  indexed.png  203 bytes
検証:
  gradient_alpha.png: アルファあり
  gradient_rgb.png: アルファなし
  with_text.png: テキスト metadata あり
  with_icc.png: ICC プロファイルあり
  oriented.tiff: 向き metadata あり
```

### 事前に懸念していたが、実際には問題にならなかったこと

push 前に「Windows や Qt 6.8 で落ちうる」と挙げていた点の結果。

| 懸念 | 結果 |
|---|---|
| T3 の能力表実測（アルファ / 可逆性の分類）が環境で変わる | **変わらなかった。** `png` / `jpeg` の分類テストは Windows でも通った |
| T4 のフィクスチャ（向き metadata、ICC）が保持されない | **保持された**（`qtimageformats` 導入後） |
| Qt 6.8 と 6.11 の API 差 | **出なかった。** Phase 0 の `QStringLiteral` の件を踏まえ、`QStringView(u"...")` を使っていたのが効いた可能性がある |
| 決定性テスト | 同一環境内の 2 回比較なので影響なし。両 OS で通った |

**環境依存の実測（ADR-0007 の往復判定）が、異なる OS・異なる Qt バージョンで
同じ結論を出したことが確認できた。**

### 推測で埋めた箇所

**なし。** モジュール構成はローカルのインストール記録で確認した。

### 残課題 / 次にやること

1. **PR を作成して `main` へマージする。** 未実施。
2. **`docs/phases.md` §1.5 に CI のモジュール指定を追記するか**は未対応。
   現状 `ci.yml` にコメントで根拠を書いてあるが、指示書側には無い。
3. Windows の実機起動確認は Phase 0 から未了のまま。
4. Phase 2 着手時の宿題: `docs/phases.md` §5.3（バッチ実行時のメモリ上限）、
   衝突ポリシーの io 層での実装、ICO の内部キー `_q_icoOrigDepth` の扱い。
