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
