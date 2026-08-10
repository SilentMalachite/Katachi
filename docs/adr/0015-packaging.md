# ADR-0015: 配布物の作り方

- 状態: 承認
- 日付: 2026-08-11

---

## 背景

Phase 4 の受け入れ基準（`docs/phases.md` §4）は 5 項目である。

1. macOS: universal binary、`macdeployqt` 済み、署名・公証済み `.dmg`
2. Windows: `windeployqt` 済み、ポータブル zip + インストーラ
3. 成果物に `third_party_licenses.txt` が同梱されている
4. Qt を動的リンクしていることを確認済み
5. クリーンな環境（Qt 未インストール）で起動を確認済み

着手時点のリポジトリには **`install()` 規則も CPack 設定も 1 つも無かった。**
バージョンの出所は `project(VERSION 0.1.0)` だけで、実行ファイルは
バンドルですらなかった。ここから配布物を組み立てる方法を決める。

第三者ライセンス文の同梱（基準 3）は論点が独立しているため **ADR-0016** に分ける。

### 決定の前提とした実測（推測ではない）

| 事実 | 出所 |
|---|---|
| Qt 6.8.3 / macOS の `QtCore` は `x86_64 arm64`（universal） | CI run 31412328017 |
| `macdeployqt` は `-sign-for-notarization` `-hardened-runtime` `-timestamp` `-codesign` `-dmg` を持つ（6.8.3 / 6.11.1 とも） | 同上 + ローカル |
| **`qt_add_executable` は `MACOSX_BUNDLE` も `WIN32_EXECUTABLE` も既定では設定しない** | Qt 公式ドキュメント（引数は `add_executable` へそのまま渡される）+ 実測 |
| **Windows の実行ファイルは `subsystem 3 (Windows CUI)` だった** | CI run 31412328017 |
| `Developer ID Application (UZQNSQAUY2)` と `notarytool` がローカルにある | `security find-identity` / `xcrun --find` |

---

## 論点 1: パッケージングの道具

### 選択肢

**A. CPack**（`DragNDrop` / `NSIS` / `ZIP` ジェネレータ）
**B. `install()` 規則 + `cmake -P` で走るスクリプト**

### 決定: B

理由は 2 つある。

1. **macOS の手順が「順序を持つ連鎖」である。** `macdeployqt` → 署名 →
   `.dmg` 作成 → 公証 → staple の順に進み、**各段の入力が前段の出力である。**
   CPack のジェネレータはパッケージ生成を内側で完結させるため、この連鎖の
   途中に割り込む形にならない。
2. **Windows のインストーラに Inno Setup を選んだ**（論点 6）。CMake 3.27 未満では
   CPack に Inno Setup ジェネレータが無く、CPack を使う利点が ZIP だけになる。

加えて、Phase 3 の `cmake/CollectExtraCodecs.cmake` で
**「構成時には確定しない情報を、ビルド後に実際に探して扱う」形**を既に採っている。
配布も同じ性質を持つ（`macdeployqt` が何を入れたかは実行するまで分からない）。
**同じ形を使い回すほうが、読む人にとっての驚きが少ない。**

---

## 論点 2: `MACOSX_BUNDLE` / `WIN32_EXECUTABLE` の適用範囲

### 選択肢

**A. `release` プリセットでのみバンドル化する**
**B. `dev` / `asan` も含め常時バンドル化する**

### 決定: B（常時 ON）

**A を採ると、配布時にしか通らない構成ができる。**

このプロジェクトは同じ形の事故を既に一度起こしている。`qtimageformats` を
CI に入れ忘れた件（`docs/phases.md` §1.5.1）である。あのとき症状は
**「ローカルでは green、CI だけ落ちる」**という形で現れた。**構成が分岐すると、
分岐した先でしか出ない失敗が生まれる。** 日常のゲートで踏む道と、配布で踏む道を
同じにする。

**両方を明示的に書く。** `qt_add_executable` の既定に頼らない。
公式ドキュメントは「引数は `add_executable` へそのまま渡される」と述べており、
実測でも macOS は素の実行ファイル、Windows は `subsystem 3 (Windows CUI)` だった。
**既定に頼ると、Qt の版が変わったときに黙って形が変わりうる。**

---

## 論点 3: universal 化の適用範囲

### 決定: `release` プリセットのみ（`x86_64;arm64`）

論点 2 と逆の結論になる。理由を書く。

`compile_commands.json` に `-arch` が 2 つ並ぶと clang-tidy が扱えない
（`docs/phases.md` §3 の品質ゲート 4 が壊れる）。

> **2026-08-11 に実測した。恐れではなく事実である。**
> `release` プリセット（`x86_64;arm64`）の `compile_commands.json` に対して
> `clang-tidy -p build/release` を走らせると、対象 12 ファイルすべてが
> `error: unable to handle compilation, expected exactly one compiler job`
> で落ちる（exit 1）。1 つのエントリに x86_64 と arm64 の 2 ジョブが入るためである。
>
> **したがって品質ゲート 4 は `build/dev` を指したままにする。**
> `build/release` を指してはならない。`dev` は native 単独なので通る。

**代わりに、分岐した構成を放置しない手当てを打つ。** CI に `package` ジョブを足し、
`release` プリセットのビルドと配布物の機械検査（P1〜P8）を**毎 PR で通す。**
論点 2 で避けたかったのは「分岐すること」ではなく「**分岐した先が検証されないこと**」
であり、それは CI で塞げる。

---

## 論点 4: 署名・公証をどこで実行するか

### 決定: ローカルのみ

証明書（`.p12`）と App Store Connect の資格情報を GitHub secrets に置かない。
CI は**未署名の成果物までを作る。**

**帰結として、CI が green でも「署名済み成果物が正しい」ことの根拠にはならない。**
署名・公証の検証（`spctl --assess` / `stapler validate`）はローカルで実行し、
**その出力を `docs/progress/phase4.md` に貼る。**

---

## 論点 5: Windows のコード署名

### 決定: 署名しない

コード署名証明書（OV / EV）を持っていない。**未署名で配布し、
SmartScreen の警告が出ることを `README.md` に明記する。**

**「警告が出ない」と書かない。出るものは出ると書く。**

---

## 論点 6: Windows インストーラの道具

### 決定: Inno Setup

- CMake の下限 3.24（Phase 0 の決定）を上げずに済む。CPack の Inno Setup
  ジェネレータは CMake 3.27 以上を要求する
- CI の runner に choco で入る
- ライセンスは**ビルド時のみの依存**であり、成果物に含まれない
  （`docs/licenses.md` §3 と同じ扱い）

WiX（MSI）は .NET と WiX Toolset を要し、個人配布のアプリには重い。

---

## 論点 7: 追加コーデックを同梱するか

### 決定: 同梱しない（`KATACHI_EXTRA_CODECS` は既定 OFF のまま）

1. **universal binary を壊す。** Homebrew の libavif / libjxl / LibRaw は
   arm64 単独でビルドされる。同梱するには両アーキテクチャ分を自前でビルドする
   必要があり、Phase 4 の範囲を超える
2. **Windows は未対応である**（ADR-0013 / Phase 3 T3 で申告済み）
3. ライセンス文の収集範囲が Qt 同梱分に収まる（ADR-0016）

**`README.md` に「ソースからビルドすれば使える」と書く。** 使えないとは書かない。

---

## 論点 8: バージョンの出所

### 決定: `project(VERSION)` を単一の出所とする

`Info.plist` の `CFBundleShortVersionString` / `CFBundleVersion`、Windows の
`FileVersion` / `ProductVersion`、インストーラ、成果物のファイル名を
**すべて `configure_file(@ONLY)` で生成する。**

同じ数字を 2 か所に書くと、片方だけ古くなる。**一致は機械で検査する**（P7）。

---

## 帰結

### 実装したもの（T2 時点）

| ファイル | 内容 |
|---|---|
| `CMakeLists.txt` | `CMAKE_OSX_DEPLOYMENT_TARGET=13.0`（`project()` より前）。警告オプションを `$<$<COMPILE_LANGUAGE:CXX>:…>` で括る |
| `src/app/CMakeLists.txt` | `OUTPUT_NAME Katachi` / `MACOSX_BUNDLE` / `WIN32_EXECUTABLE` / `Info.plist` / アイコン / `.rc` |
| `packaging/macos/Info.plist.in` | バンドルのメタデータ。値は `project()` から展開する |
| `packaging/windows/katachi.rc.in` | アイコンとバージョン資源 |
| `packaging/icons/` | 原版 SVG と `.icns` / `.ico`（別途作成済み） |

**警告オプションを `COMPILE_LANGUAGE:CXX` で括った理由を残す。**
CMake はターゲットの `COMPILE_OPTIONS` を**リソースコンパイラにも渡す**ため、
括らないと `rc.exe` が `/W4 /WX` を受け取って落ちる。
**全ソースが CXX なので、既存ターゲットの警告設定は何も変わらない。**

### まだ実装していないもの

`install()` 規則と `release` プリセット（T3）、`macdeployqt` / 署名 / 公証（T5）、
`windeployqt` / zip / Inno Setup（T6）、機械検査 P1〜P8（T7）。

### 未確認のまま残ること

**削った配布物（`Qt6Network` 等を除いた木）でアプリが起動するか**は未確認である。
T6 で削り、T7 の P8 が機械で確かめる。**確かめるまで「起動する」と書かない。**

### 変わらないもの

- **Qt は LGPLv3 で動的リンクのみ**（`docs/licenses.md` §2.1、ADR-0012）
- **アプリ実行時のネットワーク通信は行わない**（`CLAUDE.md`）。
  ただし `windeployqt` が `Qt6Network.dll` を配置することが T1 で分かった。
  その扱いは **ADR-0016 と T6** で決める
