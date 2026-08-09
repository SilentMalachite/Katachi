# Phase・TDD・品質ゲート・受け入れ基準

---

## 1. Phase 分割

| Phase | 内容 | 完了の意味 |
|---|---|---|
| **0** | 基盤：CMake、プリセット、CI、品質ゲート、**不変条件スキャナ**、空の実行ファイルが起動 | 土台が固まる |
| **1** | コア：`Result`、能力表、`ConversionSpec`、純粋 `convert()`、命名規則、テスト | GUI なしで変換ロジックが完成・テスト済み |
| **2** | GUI：D&D、ジョブ一覧、設定パネル、進捗、キャンセル、結果表示 | 手元で実用できる |
| **3** | 拡張コーデック：HEIF / AVIF / JPEG XL / RAW / PSD の可否調査と導入 | 対応フォーマットが広がる |
| **4** | 配布：`macdeployqt` / `windeployqt`、署名・公証、インストーラ、ライセンス同梱 | 他人に配れる |

**1 Phase = 1 ブランチ = 1 PR。**

---

## 1.5 依存の入手方法

| 依存 | 入手 | 備考 |
|---|---|---|
| Qt | ローカルは公式インストーラ、CI は `aqtinstall` | **CI は 6.8 系 LTS に固定する。** ローカルが 6.11 でも CI は下限に合わせる。6.8 以降に入った API を無自覚に使う事故を CI で検出するため。**モジュール指定は下記 1.5.1 を必ず読む** |
| Catch2 v3 | `FetchContent` + **固定タグ** + `FIND_PACKAGE_ARGS`（システム版があれば優先） | 両 OS の CI で追加セットアップが不要。バージョンはタグで固定し、ブランチ名やレンジで追跡しない |
| 追加コーデック | Phase 3 で決定（`docs/licenses.md` の更新が先） | オプショナル依存。無くてもビルド・起動できること |

> **「ネットワーク通信禁止」はアプリ実行時の制約であり、ビルド時の依存取得は対象外。**
> ただし再現性のため、`FetchContent` のタグは必ず固定する。

### 1.5.1 Qt のモジュール指定（CI で必ず明示する）

**画像フォーマットのプラグインは 2 つのモジュールに分かれている。**

| モジュール | 含まれるプラグイン（macOS / Windows 実測） |
|---|---|
| `qtbase` | png（組込）/ `qjpeg` / `qgif` / `qico` / `qpdf` / `qsvg` |
| **`qtimageformats`（アドオン）** | **`qtiff` / `qwebp` / `qjp2` / `qicns` / `qtga` / `qwbmp` / `qmacheif`** |

**`qtimageformats` は既定ではインストールされない。**
CI では `install-qt-action` の `modules: qtimageformats` で明示する。

> **入れ忘れると TIFF / WebP などが能力表から丸ごと消える。**
> Phase 1 の初回 CI で実際に起きた。4 ジョブすべてが
> 「`oriented.tiff` の書き出しに失敗（Unsupported image format）」で落ちた。
> ローカルは公式インストーラの全部入りで `qtimageformats` を含むため、
> **ローカルでは green、CI だけ落ちるという形で現れる。**

**能力表は実行時に生成されるため、この欠落はビルドエラーにならない。**
「対応形式が減る」という形で静かに現れる。だからこそ CI で明示する。

Phase 4 で `qtimageformats` を同梱する場合、その中の第三者ライブラリ
（libtiff / libwebp 等）のライセンス文も必要になる（`docs/licenses.md` §4）。

---

## 2. TDD

### 2.1 手順（例外なくこの順序）

1. 失敗するテストを書く
2. テストが**意図した理由で**失敗することを確認する
3. 最小の実装でテストを通す
4. 品質ゲートを全部通す（§3）
5. リファクタ
6. `docs/progress/phaseN.md` に**追記**してコミット（既存記述は書き換えない）

**テストの期待値は実装の都合で変更しない。** 期待値が誤っている可能性を検討することは許されるが、**その判断は自分で下さず停止して報告する。**

### 2.2 コアのテスト種別

| 種別 | 内容 |
|---|---|
| ラウンドトリップ | PNG → PNG、PNG → BMP など可逆同士は**ピクセル完全一致** |
| 非可逆 | JPEG 等はデコードし直して PSNR ≧ 35dB（バイト比較しない） |
| 決定性 | 同じ入力・同じ Spec で 2 回変換 → **バイト列が完全一致** |
| アルファ | `spec-core.md` §4 の表を**全行テストする** |
| エラー | `ConvertError` の全列挙値に、それを発生させるテストが 1 つ以上ある |
| 能力表 | Qt が返す形式リストが空でない。`encodable()` が少なくとも PNG 相当を含む |
| 不変条件 | `src/core` `src/app` にフォーマット名の文字列リテラルが無い |
| 依存方向 | `src/core` が `QtWidgets` / `src/io` を include していない |
| concept 適合 | **その Phase で定義した concept について**、本番型とテストダブル両方の `static_assert(C<T>)`。**加えて適合しない型への `static_assert(!C<U>)` を 1 つ以上**（制約が緩すぎて何でも通る事故を検出するため）。未実装の層の concept は対象外 |

### 2.3 決定性テストが通らない場合

1. エンコーダが時刻等を埋め込んでいないか調べる。止められるなら止める
2. `MetadataPolicy::StripAll` で解決するか確認する
3. それでも解決しない形式が判明した場合のみ、`docs/adr/` に「当該形式を決定性テストの対象外とする」根拠を記録し、除外リストを**コード内の 1 箇所に集約**する
4. **無言で除外しない。テストを削除しない。**

### 2.4 フィクスチャ

- `tests/fixtures/` には**自前で生成した**小さな画像のみ（各 < 50KB、他者の著作物を混ぜない）
- 生成スクリプトも `tests/fixtures/generate.cpp` として残す
- CI では `QT_QPA_PLATFORM=offscreen` を設定する

---

## 3. 品質ゲート（全て通ることが「完了」の定義）

```bash
cmake --preset dev
cmake --build --preset dev             # 警告ゼロ。-Wall -Wextra -Wpedantic -Werror / MSVC: /W4 /WX
ctest --preset dev --output-on-failure
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')
clang-tidy -p build/dev $(git ls-files 'src/*.cpp')
cmake --preset asan && ctest --preset asan   # ASan + UBSan（macOS / Linux）
```

`.clang-tidy` 有効チェック: `bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*`, `misc-*`
（`cppcoreguidelines-pro-bounds-*` と `modernize-use-trailing-return-type` は除外可）

**追加の除外（Phase 1 で承認）**: `bugprone-exception-escape`。
**ADR-0002 と原理的に両立しないため。** ADR-0002 は「コア関数は `noexcept` を維持し、
確保失敗時の `std::terminate` を意図的に受け入れる」と決めており、この検査は
まさにその形を禁止する。

**追加の除外（Phase 2 で承認）**: `cppcoreguidelines-owning-memory`。
**`cpp-conventions.md` §1 と原理的に両立しないため。** §1 は「Qt の親付き `new` は
`app/` 層のみ許可」と明示しているが、この検査は `new` の結果を非所有ポインタへ
入れること自体を咎める。局所変数でも引数でも指摘され、回避するとクラス構造が歪む
（**ADR-0011** に実測を記録）。

**これ以上の除外を増やすときも、同じように根拠を ADR に書き、
ここへ追記すること。黙って増やさない。**

**不変条件スキャナ 7 種（`tests/` に置く。1〜6 は Phase 0、7 は Phase 2 で実装する）**

CLAUDE.md の「絶対禁止」を機械化したもの。人手のレビューに頼らない。

1. `src/` に `std::enable_if` / `std::void_t` / SFINAE の痕跡が無い
2. `src/` に無制約の `template <typename` / `template <class` が無い（concept 定義そのものを除く）
3. `src/core` `src/app` にフォーマット名の文字列リテラルが無い（`src/core/FormatId.hpp` のみ除外）
4. `src/core` が `src/io` / `QtWidgets` を include していない
5. `src/` に `QtNetwork` / `QNetworkAccessManager` の include が無い
6. `src/` に `NOLINT` / `#pragma GCC diagnostic` / `#pragma warning` が無い
7. **`src/io` が `QtWidgets` / `QWidget` を include していない**（Phase 2 で追加）

**7 は Phase 2 で承認を得て追加した。** io はワーカースレッドで動く層であり（ADR-0010）、
`QWidget` に触れてはならない（§4 Phase 2 の「ワーカースレッドから `QWidget` に触れていない」）。
**実体の担保は `katachi_io` が `Qt6::Widgets` をリンクしないことであり、7 はテキスト上の二重の網である。**
これ以上スキャナを増やすときも、同じように根拠を ADR に書き、ここへ追記すること。

**`.clang-format`**: LLVM ベース / `IndentWidth: 4` / `ColumnLimit: 100` / `PointerAlignment: Left`

**エージェントはこれらを自分で実行する。**

- 実行していないゲートを「通った」と報告するのは重大な違反
- 実行できない環境的理由がある場合は、報告に明記する
- **警告抑制プラグマ・`NOLINT` の追加は禁止。** 必要だと判断したら停止する

---

## 4. 受け入れ基準

### Phase 0
- [ ] GitHub Actions が macOS / Windows の両方でビルド + テストを通す
- [ ] CI の Qt が 6.8 系 LTS に固定されている（§1.5）
- [ ] **実機で空のメインウィンドウが起動することを、確認できた OS について確認する**
- [ ] **実機確認していない OS は、報告に「CI ビルドのみ・実機未確認」と明記する**（CI 成功を実機起動の根拠として報告しない）
- [ ] 不変条件スキャナ 6 種が実装され、`ctest` で走る（§3）
- [ ] 品質ゲートが全て CI で走り、警告ゼロ
- [ ] `.clang-format` が §3 の設定で置かれている
- [ ] `docs/adr/0001-ui-toolkit.md` に Qt Widgets 採用の根拠がある
- [ ] `docs/licenses.md` に Qt の LGPL 条件と、動的リンクである旨が記載されている
- [ ] 4 つの参照文書が `docs/` 直下にあり、`CLAUDE.md` のみリポジトリ直下にある

### Phase 1
- [ ] `convert()` が純粋関数として実装され、ファイル I/O・時刻・グローバル状態に触れていない
- [ ] `docs/adr/0002-noexcept-and-allocation.md` に `noexcept` 判断が記録されている
- [ ] `CapabilityTable::buildFromQt()` が実行時に能力表を生成する
- [ ] §2.2 の全テスト種別が存在し、green
- [ ] `ConvertError` の全列挙値にテストがある
- [ ] 不変条件スキャナ 6 種が引き続き green（Phase 0 で実装済み）
- [ ] `src/core/Concepts.hpp` に **core 層の** concept（`ResultValue` / `ResultError` / `CapabilitySource`）が集約され、それぞれに肯定・否定両方の `static_assert` がある
- [ ] `Converter.hpp` がテンプレートになっていない（実装が `.cpp` に閉じている）
- [ ] 決定性テストが green（除外形式がある場合は ADR に記録済み）
- [ ] `docs/format-matrix.md` がビルド時に自動生成される

### Phase 2
- [ ] ドラッグ&ドロップでファイル / フォルダを追加できる
- [ ] 変換中にキャンセルでき、キャンセル後にアプリが正常状態に戻る
- [ ] 1000 ファイルのバッチで UI が固まらない
- [ ] `spec-core.md` §7 の UI 非機能要件を全て満たす
- [ ] ワーカースレッドから `QWidget` に触れていないことを確認済み
- [ ] 失敗したジョブが結果一覧に理由付きで残る

### Phase 3
- [ ] 追加コーデックの導入**前**に `docs/licenses.md` が更新されている
- [ ] 追加コーデックが無い環境でもビルド・起動する（オプショナル依存）
- [ ] 能力表が追加コーデックを自動的に反映する（コード変更不要）

### Phase 4
- [ ] macOS: universal binary、`macdeployqt` 済み、署名・公証済み `.dmg`
- [ ] Windows: `windeployqt` 済み、ポータブル zip + インストーラ
- [ ] 成果物に `third_party_licenses.txt` が同梱されている
- [ ] Qt を動的リンクしていることを `otool -L` / `dumpbin /dependents` で確認済み
- [ ] クリーンな環境（Qt 未インストール）で起動を確認済み

---

## 5. 未解決の設計判断

### 5.1 決定済み（過去に未解決としていたが確定した）

| 事項 | 決定 | 根拠 |
|---|---|---|
| `FormatId` の実体 | **強い型付き文字列**（`struct FormatId { QString v; }`） | 能力表は実行時生成のため、インデックスは生成順に依存する。`fromCapabilities()` のテストダブルと本番で同じ値が別の形式を指しうる。無効値の排除は `find()` が `std::optional` を返すことで担保される |
| `resize` の補間方式 | **`Qt::SmoothTransformation` 固定** | 選択肢を増やすと決定性テストの組み合わせが増える。可変にするなら ADR を書いてから |
| `convert()` の入力型 | **`QByteArray`**（`spec-core.md` §2 で確定） | — |
| `JobRunner` の注入方式 | **テンプレート引数で注入**（`cpp-conventions.md` §2.3） | 同 §2.3 の表で決定済み。`std::function` 案は採らない |
| メタデータ保持の実装手段 | **`MetadataPolicy::PreserveAll` を `PreserveSupported` に改名**し、向き / テキスト / ICC のみ保持する。EXIF 全体の保持は Phase 3 | Phase 1 着手時に Qt 6.8 の公式ドキュメントで確認したところ、EXIF 全体を読み書きする API が存在しなかった（**ADR-0003**） |
| `convert()` の警告の返し方 | **成功値を `ConversionOutput` にし、`warnings` を載せる** | `spec-core.md` §4 が要求する警告の置き場所が `Result<QByteArray, ConvertError>` に無かった（**ADR-0004**） |
| 衝突ポリシーの担当層 | **core は名前の生成のみ。衝突の解決は Phase 2 の `src/io`** | 衝突判定にファイルシステム参照が要り、core では禁止されているため（**ADR-0005**） |
| バッチ実行時のメモリ上限 | **バイト予算セマフォ。総量 1 GiB を `src/io/MemoryBudget` が管理する** | `maxPixels` 既定を `ARGB32` で持つと 1 枚 1 GiB。並列度を掛けると理論ピークが 9 GiB を超える。予算の単位を「デコード済み最大画像 1 枚分」に据えた（**ADR-0008**） |
| 衝突ポリシーの適用場所とスキップの表し方 | **`FileSink::write()` がワーカースレッドで適用する。スキップは `IoError::DestinationExists`** | 事前に main thread で 1000 回実在確認すると UI が止まる。`ByteSink` の戻り値型（`cpp-conventions.md` §2.2）を変えずに済む（**ADR-0009**） |
| UI の構成と §7 の担保方法 | **縦 3 段の単一ウィンドウ。モーダルは「出力先の選択」と「上書き実行の確認」の 2 用途のみ** | §7 を人手の注意ではなく機械検査（`grep` テスト・タイマ間隔・`activeModalWidget`）で守る。`QFileDialog` の可否は §7 の字義と衝突するため判断を仰いだ（**ADR-0010**） |

### 5.2 未解決（Phase 1 着手時に決める）

**該当なし。** かつてここにあった「メタデータ保持の実装手段」は、Phase 1 着手時に
Qt 6.8 の公式ドキュメントで実挙動を確認したうえで決定し、§5.1 へ移した（ADR-0003）。

新たに未解決事項が生じた場合はここに追記する。**推測で埋めず、決めた根拠を ADR に残すこと。**

### 5.3 Phase 2 着手時に決める

**該当なし。** かつてここにあった「バッチ実行時のメモリ上限」は、Phase 2 着手時に
`src/core/Converter.cpp` の一時画像の生存範囲を実際に読んだうえで決定し、§5.1 へ移した（ADR-0008）。

決定にあたって併せて確定させた 2 件（衝突ポリシーの適用場所 / UI の構成と §7 の担保方法）も §5.1 にある。

新たに未解決事項が生じた場合はここに追記する。**推測で埋めず、決めた根拠を ADR に残すこと。**

### 5.4 Phase 3 着手時に決める

Phase 3 着手時（2026-08-09）に計画として提示し、承認を得た決定。
経緯は `docs/progress/phase3.md`、根拠は該当 ADR に置く。

| # | 論点 | 決定 | 記録先 |
|---|---|---|---|
| D1 | 追加コーデックの導入方式 | **第三者 `QImageIOPlugin` を `find_package` で探してビルドする**（既定 OFF）。自前で `QImageIOPlugin` を書かない（`src/` に新層が増え §1 の構成を変えるため） | ADR-0013 |
| D2 | 対象コーデック | **AVIF / JPEG XL / PSD / RAW。HEIF は自前導入しない**（macOS は `qmacheif` で既に読み書き可） | ADR-0013 |
| D3 | HEIF エンコーダのライセンス | **未確定。** libheif の HEVC エンコーダ backend に GPL-2.0-only のものがあると `docs/licenses.md` §5 の表によりリンクできない。一次情報を確認するまで判断しない | ADR-0013 |
| D4 | 追加コーデックの既定値 | **`KATACHI_EXTRA_CODECS=OFF`。** 既定パスが常に「追加コーデックが無い環境」になり、§4 Phase 3 の受け入れ基準 2 が毎回の品質ゲートで検証される | ADR-0013 |
| D5 | 受け入れ基準 3 の検証方法 | **テスト専用のダミー `QImageIOPlugin` を `tests/plugins/` に置き、`src/` を 1 行も変えずに能力表へ現れることを検査する。** 実コーデックの可用性と独立に検証できる | ADR-0013 |
| D6 | EXIF 全体保持（ADR-0003 の宿題） | **Phase 3 では見送る。** 受け入れ基準 3 項目に含まれず、外部 EXIF ライブラリは本体が**リンク**するため「オプショナル依存」の枠に収まらない | ADR-0014 |

**D3 だけが未確定である。** 確定するまで HEIF 関連の依存を導入しない。
