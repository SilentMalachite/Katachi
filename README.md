# Katachi

**Qt 6 / C++20 のクロスプラットフォーム画像フォーマット変換アプリ。**

対応フォーマットをハードコードしない。実行時に `QImageReader` / `QImageWriter` へ問い合わせて
能力表を作るため、**Qt のビルドに入っているプラグインがそのまま対応フォーマットになる。**

**アプリはネットワーク通信を一切行わない。** これは設計上の制約であり、
`QNetworkAccessManager` を include した時点で違反とみなす（[CLAUDE.md](CLAUDE.md)）。

---

## 状態

**Phase 4（配布）を実施中。** 配布物は作れる。**公証は任意**（必須ではない）。
残る必須は **macOS のクリーン環境での実機起動確認**である。

| Phase | 内容 | 状態 |
|---|---|---|
| 0 | 基盤：CMake / プリセット / CI / 品質ゲート / 不変条件スキャナ | 完了 |
| 1 | コア：`Result` / 能力表 / `convert()` / 命名規則 | 完了 |
| 2 | GUI：D&D / ジョブ一覧 / 設定パネル / 進捗 / キャンセル / 結果表示 | **完了（下記 2 点を除く）** |
| 3 | 拡張コーデック：AVIF / JPEG XL / PSD / RAW（オプショナル依存） | **完了（macOS のみ。下記参照）** |
| 4 | 配布：`macdeployqt` / `windeployqt` / 署名（公証は任意） / インストーラ | **実施中（受け入れ 5 件中 4 件達成・1 件は macOS 実機待ち）** |

### 未確認・未対応の項目（正直に記載する）

- **キーボードのみでの開始・キャンセル** — タブ順の連結と到達可能性は自動テスト済みだが、
  マウス無しでの実操作は未確認
- **Windows での実機起動** — **必須としない**（2026-08-17）。CI の機械検査（P9 / `package`）で
  受け入れ基準 5 の Windows 側は足りる、と決めた。実機での感触は未確認のまま残す
- **Windows での追加コーデック** — 未対応。`KATACHI_EXTRA_CODECS=ON` の CI ジョブは macOS のみ
- **追加コーデック ON でのアプリ実機起動** — 未確認。ビルドとテストが通ることのみ確認済み
- **HEIF** — 自前導入は対象外。macOS は `qmacheif`（Qt 同梱）で読み書き可、Windows は使えないまま
- **macOS の公証** — **任意。** Developer ID 署名までが必須。未公証のため Gatekeeper は
  `Unnotarized Developer ID` を返す（失敗ではなく正しい状態）。開き方は下記「配布物」を参照
- **macOS のクリーンな環境での起動** — **未確認（受け入れ基準 5 の残り）。**
  Qt を持たない別アカウントでの実機確認がまだである。CI の P8 は代替にしない
- **仮想キーボードを外した影響** — 配布物から `platforminputcontexts` を削っている。
  macOS の日本語入力は `cocoa` プラグインが担うため影響しないと考えているが、
  **実機で日本語を打って確かめていない**

**macOS については、CI の成功や自動テストの結果を実機操作の根拠にはしない。**

---

## できること

- **ドラッグ & ドロップ**でファイルとフォルダを追加する。フォルダは**再帰的**にたどり、
  能力表が読める拡張子だけを拾う。重複は除外する
- 出力形式と拡張子を選ぶ。**選択肢はすべて能力表から作る**（ハードコードしない）
- 品質 / アルファ / メタデータ / ICC / リサイズ / 命名パターン / 衝突ポリシーを指定する
- 進捗を見ながらバッチ変換し、**途中でキャンセルできる**
- **失敗したジョブは理由付きで一覧に残る。** スキップは失敗と別に表示する

### UI の方針

アニメーション・フェード・自動スクロールを入れない。独自テーマを持たず**ダークモードは
システム設定に追随する**。ウィンドウは 1 枚のみ。モーダルは出力先選択と上書き確認の 2 用途だけで、
**エラーはステータス行と結果一覧の理由列に出す。**

これらは `docs/spec-core.md` §7 の要件であり、うち 3 件（アニメーション / 独自テーマ /
複数ウィンドウ）は**機械検査**で守っている。

### 追加コーデック（オプション、既定 OFF）

`KATACHI_EXTRA_CODECS=ON` でビルドすると、AVIF / JPEG XL / PSD / RAW（29 拡張子）が
**`src/` の変更なしに**能力表・出力候補・変換対象へ加わる（対応形式 21 → 57）。
書き出しも可能なのは AVIF と JPEG XL のみで、PSD / RAW は読み込み専用。

- 既定は **OFF**。無い環境でも同じアプリがビルド・起動する
- ON でビルドしても、アプリ本体は追加コーデックへ**リンクしない**（実行時ロードのプラグイン）
- **現時点は macOS のみ CI 検証済み。** Windows は未対応
- 詳細は [`docs/adr/0013-extra-codecs.md`](docs/adr/0013-extra-codecs.md)

```bash
brew install libavif jpeg-xl libraw pkgconf   # macOS
cmake --preset dev-codecs
cmake --build --preset dev-codecs
```

---

## 必要なもの

| 項目 | 条件 |
|---|---|
| Qt | **6.8 LTS 以上**。`Core` / `Gui` / `Widgets` / `Concurrent` |
| Qt アドオン | **`qtimageformats`**（TIFF / WebP などを扱う場合。既定では入らない） |
| CMake | 3.24 以上 |
| ジェネレータ | Ninja |
| コンパイラ | C++20 対応（`std::expected` は C++23 のため使わない） |
| 対象 OS | macOS 13 以上 / Windows 10 以上（x64） |

> **`qtimageformats` は既定でインストールされない。** 入れないと TIFF / WebP などが
> 能力表から丸ごと消える（実際に CI で起きた）。詳細は [`docs/licenses.md`](docs/licenses.md) §4。

---

## ビルド

`CMakePresets.json` の `CMAKE_PREFIX_PATH` は `$env{QT_ROOT_DIR}` を読む。

```bash
export QT_ROOT_DIR=/path/to/Qt/6.8.3/macos   # Windows は .../msvc2022_64

cmake --preset dev
cmake --build --preset dev
```

成果物は **macOS が `build/dev/src/app/Katachi.app`**、**Windows が
`build/dev/src/app/Katachi.exe`**。

> **開発ビルドでも配布時と同じ形（macOS は `.app`、Windows は GUI サブシステム）で作る。**
> 配布時にしか通らない構成を作らないためである（`docs/adr/0015-packaging.md`）。
> macOS の実行本体は `Katachi.app/Contents/MacOS/Katachi` にある。

### テスト

```bash
ctest --preset dev --output-on-failure
```

### ASan + UBSan（macOS / Linux）

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

MSVC ではサニタイザプリセットは使えない（構成時に明示的に失敗する）。

---

## 品質ゲート

**警告ゼロを強制する**（`-Werror` / MSVC は `/WX`）。CI は macOS と Windows の両方で回す。

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev --output-on-failure
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')
clang-tidy -p build/dev $(git ls-files 'src/*.cpp')
cmake --preset asan && ctest --preset asan
```

**CI の Qt は 6.8 系 LTS に固定している。** ローカルが 6.11 でも CI は下限に合わせる。
6.8 以降に入った API を無自覚に使う事故を CI 側で検出するためであり、実際に検出された
（`docs/progress/phase2.md` の `QStringLiteral` の件）。

### 機械検査

規約を文書だけで守らせない。`ctest` の一部として次を検査する。

| 検査 | 内容 |
|---|---|
| INV1〜INV7 | 依存方向 / フォーマット名の文字列リテラル / core のファイルアクセス / 警告抑制 / ワーカースレッドからの `QWidget` 参照 など |
| ui1〜ui3 | アニメーション / 独自テーマ / 複数ウィンドウの禁止 |
| P1〜P9 | 配布物の検査：動的リンク / universal / 対象 OS の下限 / 画像プラグインの一致 / 権利表示 / 版 / 起動 / 依存解決（`release` プリセットでのみ走る） |

**すべて違反フィクスチャ付き**で、検査が空振りしていないことを機械で確かめている。

---

## 構成

**依存方向は `core → io → app` の一方向のみ。** 逆流させない。

| ディレクトリ | 役割 |
|---|---|
| `src/core` | 純粋関数コア。**ファイルアクセス・時刻取得・例外送出・グローバル可変状態を持たない** |
| `src/io` | ファイルの読み書き / 衝突解決 / バッチ実行 / メモリ予算 / Qt シグナルへの橋渡し |
| `src/app` | Qt Widgets の UI |
| `docs/` | 仕様・規約・Phase 定義・ADR・進捗記録 |
| `tests/` | Catch2 v3 の単体テストと機械検査 |

`concept` は層ごとに置く（`core/Concepts.hpp` / `io/IoConcepts.hpp`）。

---

## ドキュメント

| ファイル | 内容 |
|---|---|
| [`CLAUDE.md`](CLAUDE.md) | プロジェクトの決定事項・禁止事項・作業手順 |
| [`docs/spec-core.md`](docs/spec-core.md) | 型・能力表・アルファ・命名・並行・UI 要件 |
| [`docs/cpp-conventions.md`](docs/cpp-conventions.md) | C++20 規約と `concept` の方針 |
| [`docs/phases.md`](docs/phases.md) | Phase 分割・受け入れ基準・品質ゲート |
| [`docs/agent-protocol.md`](docs/agent-protocol.md) | 報告書式・サブエージェント・曖昧さの解決順序 |
| [`docs/licenses.md`](docs/licenses.md) | 本体と依存物のライセンス条件 |
| [`docs/release.md`](docs/release.md) | 配布物の作り方・署名・（任意の）公証・未公証時の開き方・検証の手順 |
| [`docs/adr/`](docs/adr/) | 設計判断の記録（ADR-0001〜0016） |
| [`docs/progress/`](docs/progress/) | Phase ごとの実施記録（**追記のみ**） |

---

## ライセンス

**本体は GNU General Public License v3.0 or later。** 全文は [`LICENSE`](LICENSE)。

```
Copyright (C) 2026 Silent Malachite

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

### Qt について

Qt 6 は **LGPL v3** で利用し、**動的リンクのみ**とする。静的リンクは行わない。
LGPLv3 §4 が求める「利用者がライブラリを差し替えて再リンクできること」を、
動的リンクであれば成果物の配布だけで満たせるためである。

**LGPLv3 は GPLv3 での再配布を許可しているため、本体の GPLv3-or-later と両立する。**
判断の記録は [`docs/adr/0012-license-gplv3.md`](docs/adr/0012-license-gplv3.md)。

Catch2 v3（Boost Software License 1.0）は**テストのみで使い、成果物には含まれない。**

Qt 同梱の画像フォーマットプラグインは libjpeg-turbo / libtiff / libwebp などを内部に含む。
追加コーデック（ON 時）は libavif / libjxl / LibRaw / kimageformats / ECM を使う。
いずれも GPLv3 と両立することを確認済み（[`docs/licenses.md`](docs/licenses.md) §5.1）。
配布時に同梱する第三者ライセンス文は Qt の SBOM から機械的に生成する（同 §4）。

---

## 配布物

**まだリリースしていません。** 作り方と検証の手順は [`docs/release.md`](docs/release.md)。

| OS | 形式 | 状態 |
|---|---|---|
| macOS 13 以上 | `.dmg`（universal: x86_64 + arm64） | **Developer ID 署名済み・未公証（公証は任意）** |
| Windows 10 以上 (x64) | ポータブル zip / インストーラ | **未署名**（SmartScreen の警告が出ます） |

### 未公証の macOS 配布物を開くとき

Gatekeeper は `Unnotarized Developer ID` と判定します。これは署名の失敗ではなく、
**公証していない Developer ID 配布として正しい状態**です。

1. Finder でアプリを **右クリック →「開く」**（ダブルクリックだけでは拒否されることがあります）
2. ダイアログで再度「開く」を選ぶ
3. それでも開かない場合:

```bash
xattr -d com.apple.quarantine /Applications/Katachi.app
```

詳細は [`docs/release.md`](docs/release.md) §3.0。

### 同梱するもの・しないもの

配布物には**アプリが実際に使う Qt のライブラリとプラグインだけ**を入れています。
`macdeployqt` / `windeployqt` の素の出力から、**使わないものを削って**います。

- **削るもの**: 仮想キーボードのプラグインと、それが引き込む `QtQuick` / `QtQml` /
  `QtNetwork` など 9 つ。**このアプリは Qt Quick を使わず**（[ADR-0001](docs/adr/0001-ui-toolkit.md)）、
  **ネットワーク通信も行いません。** 配布物の中身を、その決定と食い違わせないためです
- **戻すもの**: `macdeployqt` は SVG の画像プラグインを落とすため、明示的に戻しています。
  放置すると**配布物だけ SVG が読めなくなります**
- **入れないもの（Windows）**: MSVC のランタイム DLL。**個別の再頒布が許諾されていない**ため
  （Qt 公式ドキュメントの記載）。ポータブル zip は
  [Microsoft Visual C++ 再頒布可能パッケージ (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  を必要とします。インストーラ版は公式の再頒布パッケージを実行します

### 対応形式は OS で異なります

macOS は 21 形式、Windows は 18 形式です。差は `heic` / `heif` / `jp2` の 3 つで、
これらは macOS 固有の Qt プラグイン（Apple の Image I/O を使うもの）によるためです。

### 第三者コードの権利表示

成果物には `LICENSE`（本体の GPLv3）と `third_party_licenses.txt` を同梱します。
後者は **Qt が同梱する SPDX 2.3 の SBOM から機械的に生成**しており、
**法文が 1 つでも欠けていればビルドが止まります**。

**ただし、この一覧が完全であることは機械では確かめられません。** `QtCore` に静的に
取り込まれた第三者コードは配布物にファイルとして現れないためです。詳細は
[`docs/licenses.md`](docs/licenses.md) §4.3。

---

## 貢献

[`CONTRIBUTING.md`](CONTRIBUTING.md) を読んでください。**品質ゲートを自分で実行した結果を
添えない変更は受け付けません。**
