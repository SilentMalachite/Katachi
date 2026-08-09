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
