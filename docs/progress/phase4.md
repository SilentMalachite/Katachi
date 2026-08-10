# Phase 4 進捗（配布）

**追記のみ。既存の記述は書き換えない**（`CLAUDE.md`）。
誤りを見つけた場合も遡って修正せず、新しい日付で訂正を追記する。

---

## 2026-08-11 — Phase 4 に着手。計画の承認を得た

### 実施内容

`main`（`d9c6ada`）から `phase4` ブランチを切り、Phase 4 の計画を提示して承認を得た。
本エントリはその計画と、計画の前提として**実測した事実**の記録である。

### 読んだ文書

`CLAUDE.md` / `docs/progress/phase3.md`（全エントリ）/ `docs/phases.md` /
`docs/licenses.md` / `docs/agent-protocol.md` / `docs/spec-core.md` §1〜2 /
`docs/adr/0012-license-gplv3.md` / `README.md`

コードと構成は `CMakeLists.txt` / `CMakePresets.json` / `src/app/CMakeLists.txt` /
`src/app/main.cpp` / `tests/CMakeLists.txt` / `cmake/QualityGates.cmake` /
`.github/workflows/ci.yml` を読んだ。

### 着手前に実測した事実（推測ではない）

| 事実 | 根拠 |
|---|---|
| ローカル Qt 6.11.1 の `QtCore` は **universal**（`x86_64 arm64`） | `lipo -archs ~/Qt/6.11.1/macos/lib/QtCore.framework/QtCore` |
| **`Developer ID Application: Hiroshi Annaka (UZQNSQAUY2)` が有効**。`notarytool` も存在する | `security find-identity -v -p codesigning` / `xcrun --find notarytool` |
| 現在の成果物は **`.app` バンドルではない**素の実行ファイルで、**arm64 単独** | `build/dev/src/app/katachi` が存在（`.app` は無い）/ `lipo -archs` が `arm64` |
| Qt は**すでに動的リンク**（`@rpath/QtCore.framework/...`）。受け入れ基準 4 の方向は正しい | `otool -L build/dev/src/app/katachi` |
| `macdeployqt` に **`-sign-for-notarization` / `-hardened-runtime` / `-timestamp` / `-dmg` がある** | `macdeployqt` のヘルプ出力（Qt 6.11.1） |
| **Qt のバイナリインストールに第三者ライセンス文は含まれない。** `~/Qt/Licenses/` は Qt 自身の 5 ファイル（`COPYING.txt` / `Copyright.txt` / `LICENSE` / `LICENSE.FDL` / `LICENSE.GPL3-EXCEPT`）のみ | `ls ~/Qt/Licenses` / `find ~/Qt/6.11.1/macos -maxdepth 2 -iname "*license*"` → **0 件** |
| `install()` 規則・CPack 設定が**リポジトリに 1 つも無い**。バージョンは `project(VERSION 0.1.0)` のみ | `CMakeLists.txt` 全体の grep |
| git タグは **0 件**（未リリース） | `git tag` |
| ローカルに入っている Qt は **6.11.1 のみ**（6.8 系は無い） | `ls ~/Qt` |
| ローカル環境は macOS 26.6.1 / Xcode 26.6 | `sw_vers` / `xcodebuild -version` |

**Windows 側の実測はまだ無い。** `windeployqt` の出力構成も、`WIN32_EXECUTABLE` 未設定で
コンソール窓が出るかも未確認である。**これらは推測であり、T1 で CI の実測に置き換える**
（Phase 3 の T0 → T1 と同じ運び）。

### 承認された決定

利用者の判断を得た 4 件（2026-08-11）。

| # | 論点 | 決定 |
|---|---|---|
| D1 | 署名・公証の実行場所 | **ローカルのみ。** 証明書と公証資格情報を GitHub に置かない。CI は未署名の成果物までを作る |
| D2 | Windows の検証環境 | **実機あり・コード署名なし。** SmartScreen 警告を README に明記する。Phase 2 から残る「Windows 実機起動 未確認」も T8 で解消する |
| D3 | 追加コーデックの同梱 | **同梱しない（既定 OFF のまま）。** brew の libavif / libjxl / LibRaw は arm64 単独で universal binary を壊す。Windows も未対応 |
| D4 | Windows インストーラ | **Inno Setup。** CMake の下限 3.24 を上げずに済む（`.iss` を自前で持つ） |

計画時に案として提示し、併せて承認を得た決定（ADR に書く）。

| # | 論点 | 決定 | 記録先 |
|---|---|---|---|
| D5 | パッケージング方式 | **`install()` 規則 + `cmake -P` スクリプト。CPack を使わない。** macOS は macdeployqt → 署名 → dmg → 公証 → staple の順序に割り込む必要があり、Windows は D4 で Inno Setup を選んだため CPack の必然性が無い。Phase 3 の `CollectExtraCodecs.cmake`（構成時に確定しない情報をビルド後に実測する形）を踏襲する | ADR-0015 |
| D6 | `MACOSX_BUNDLE` の適用範囲 | **dev / asan も含め常時 ON。** 配布形と開発形を分けると、リリース時にしか通らない構成ができる。**`qtimageformats` の事故（ローカル green / CI だけ落ちる）と同じ形**を作らない | ADR-0015 |
| D7 | universal 化の適用範囲 | **`release` プリセットのみ**（`x86_64;arm64`）。代わりに **CI へ `package` ジョブを足して release 構成を毎 PR で通す**ことで「リリース時だけ通る構成」を作らない | ADR-0015 |
| D8 | 第三者ライセンス文の集め方 | **同梱物から機械的に列挙する。** Qt のバイナリに法文が無いことを実測したため、Qt 公式の Third-Party Code 文書と各上流リポジトリ（一次情報）から集めて `packaging/licenses/` に置き、**「成果物に入っているもの」⊆「法文がある項目」を機械検査する**（欠けたら失敗） | ADR-0016 |
| D9 | GPLv3 §6 の対応ソース提供 | **リリースにソース tarball（`git archive`）を添付し、`third_party_licenses.txt` と README に入手先を書く。** 公開リポジトリへの URL だけに頼らない | ADR-0016 |
| D10 | LGPLv3 §4 の再リンク可能性 | 動的リンクで満たす。加えて **Qt の版・入手先・「利用者は Qt を差し替えられる」旨**を同梱文書に書く。**署名済み `.app` はフレームワーク差し替えで署名が壊れるため、再署名手順も併記する** | ADR-0016 |
| D11 | バージョンの出所 | **`project(VERSION)` を単一の出所**とし、`Info.plist` / `.rc` / `.iss` / 成果物名をすべてそこから生成する。一致を機械検査する（P7） | ADR-0015 |

### 判断を仰いだ 6 点と、承認の読み取り方（**申告**）

計画では「承認時に判断を仰ぎたい 6 点」を挙げ、それぞれに推奨を添えた。
利用者の返答は「**承認します**」であり、6 点を個別に指定したものではない。
**推奨のとおりに読み取ったことを明示して記録する。** 読み違えていれば指摘を受けたい。

| # | 事項 | 読み取った結論 | 着手時期 |
|---|---|---|---|
| 1 | アプリアイコン（`.icns` / `.ico`） | 用意する方向。ただし**素材が無く、作り方の相談が要る**。**ここだけは未決のまま残る** | T2 |
| 2 | `docs/release.md` の新設（`CLAUDE.md` の参照表が 6 文書になる） | **新設する** | T5 |
| 3 | T4 の差分行数（コード 150 行 + 法文 1,500〜2,500 行） | **法文は 400 行の停止条件の対象外として進めてよい** | T4 |
| 4 | `MACOSX_BUNDLE` を dev でも ON にする | **ON にする**（`README.md` の実行パス記述を更新する） | T2 |
| 5 | bundle identifier を `com.silentmalachite.katachi` とする | **この値で進める** | T2 |
| 6 | `v0.1.0` タグと GitHub Release を実際に作るか | **作らない。** 明示の指示があるまで実行しない | — |

**1 は T2 に入る前にもう一度確認する。** 素材の用意は利用者にお願いする項目であり、
推測で図案を決めない（`CLAUDE.md` 停止条件 1）。

### タスク分割

| # | 内容 | 主な変更先 | 差分見込み |
|---|---|---|---|
| T0 | 着手記録（本エントリ） | `docs/progress/phase4.md`（新規）/ `docs/phases.md` §5.5 | 文書のみ |
| T1 | **実測で推測を潰す。** ① CI の Qt 6.8.3 macOS が universal か ② 6.8.3 の `macdeployqt` に署名オプションがあるか ③ Windows の `windeployqt` 出力構成 ④ コンソール窓の有無 | `.github/workflows/ci.yml`（一時的な実測ステップ） | 〜30 行 |
| T2 | バンドル化とメタデータ。`MACOSX_BUNDLE` / `WIN32_EXECUTABLE` を**既定に頼らず明示**、`Info.plist`、Windows `.rc`、`CMAKE_OSX_DEPLOYMENT_TARGET=13.0` | `src/app/CMakeLists.txt` / `packaging/macos/Info.plist.in`（新規）/ `packaging/windows/katachi.rc.in`（新規）/ `CMakeLists.txt` / `README.md` | 100〜150 行 |
| T3 | `install()` 規則と `release` プリセット | `src/app/CMakeLists.txt` / `CMakeLists.txt` / `CMakePresets.json` | 80〜120 行 |
| T4 | 第三者ライセンス文の収集と `third_party_licenses.txt` の生成（**同梱より先**） | `packaging/licenses/*`（新規）/ `packaging/licenses/SOURCES.md`（新規）/ `cmake/ThirdPartyLicenses.cmake`（新規）/ `docs/licenses.md` §4 | コード 150 行 + 法文 1,500〜2,500 行 |
| T5 | macOS: macdeployqt → 署名 → dmg → 公証 → staple | `cmake/PackageMacOS.cmake`（新規）/ `docs/release.md`（新規） | 150〜250 行 |
| T6 | Windows: windeployqt → ポータブル zip + Inno Setup | `cmake/PackageWindows.cmake`（新規）/ `packaging/windows/katachi.iss.in`（新規） | 150〜250 行 |
| T7 | 機械検査 P1〜P8 を ctest / CI に載せる（**違反フィクスチャ付き**） | `tests/packaging/scan_package.cmake`（新規）/ `tests/packaging/fixtures/violations/`（新規）/ `cmake/PackageChecks.cmake`（新規）/ `.github/workflows/ci.yml` | 250〜350 行 |
| T8 | クリーン環境での起動確認（受け入れ基準 5）。macOS は **Qt を持たない別ユーザーアカウント**、Windows は実機 | `docs/progress/phase4.md` | 記録のみ |
| T9 | 受け入れ基準の検証・文書更新・PR | `docs/phases.md` §4 / `README.md` / `docs/progress/phase4.md` | 文書のみ |

### T7 で追加するテストと期待値（実装後に決めない）

`tests/packaging/` に置き、既存の不変条件スキャナと同じ形（`cmake -P` + 違反フィクスチャ）で
登録する。`KATACHI_PACKAGE=ON`（`release` プリセット）でのみ有効。

| # | テスト | 期待値 |
|---|---|---|
| P1 | `package.qt_is_dynamic` | macOS: `otool -L` に `@rpath/QtCore.framework/...` が現れる。Windows: `dumpbin /dependents` に `Qt6Core.dll` `Qt6Gui.dll` `Qt6Widgets.dll` `Qt6Concurrent.dll` が現れる。**受け入れ基準 4 の機械化** |
| P2 | `package.is_universal`（macOS） | バンドル内の**全 Mach-O** が `x86_64` と `arm64` の**両方**を含む。1 つでも単一アーキなら失敗 |
| P3 | `package.deployment_target`（macOS） | 実行ファイルの `LC_BUILD_VERSION` の `minos` が **13.0 以下**（`CLAUDE.md` の対象 OS） |
| P4 | `package.imageformats_complete` | 成果物の `imageformats` プラグイン集合 == Qt インストールの同集合（**完全一致**）。Phase 1 の「静かに形式が消える」事故を機械で塞ぐ |
| P5 | `package.licenses_cover_bundle` | 成果物に入っている Qt フレームワーク / プラグインから作った「必要な項目」集合 ⊆ `third_party_licenses.txt` の項目集合。**欠けたら失敗** |
| P6 | `package.licenses_required_texts` | `third_party_licenses.txt` に GPLv3 / LGPLv3 の全文、ソース入手先 URL、Qt の版が含まれる |
| P7 | `package.version_matches_project` | 成果物名・`CFBundleShortVersionString`・Windows の `FileVersion` が `project(VERSION)` と一致 |
| P8 | `package.starts_without_system_qt` | Qt 関連の環境変数を除いた環境で `QT_QPA_PLATFORM=offscreen` 起動 → **5 秒生存し SIGTERM で終了。標準エラーに `Library not loaded` / `could not find or load the Qt platform plugin` が出ない** |

**P1〜P8 それぞれに違反フィクスチャを付ける**（`invariant.*.detects_violation` と同じ方式）。
スキャナが空振りしていても「全部 green」に見える事故を防ぐため。

**P8 は真のクリーン環境の代替であり、置き換えではない。** 実機確認は T8 で別途行う。

### 通す品質ゲート

`CLAUDE.md` の 6 ゲート（既定 OFF 構成）に加えて、T3 以降は次も通す。

- `cmake --preset release && cmake --build --preset release` → 警告ゼロ
- `ctest --preset release` → 既存 172 本 + P1〜P8 とその違反フィクスチャ
- ローカル（署名後）: `spctl -a -vvv -t open --context context:primary-signature` と `xcrun stapler validate`
- CI: 既存 5 ジョブ + **`package` ジョブ（macOS / Windows、未署名まで）**

**署名・公証は D1 によりローカル実行のため、CI ゲートには含まれない。**
**CI の成功を署名済み成果物の根拠として報告しない。**

### 今回やらないこと

- **追加コーデックの同梱**（D3）
- **Windows のコード署名**（D2。SmartScreen 警告は README に明記する）
- **タグ push での自動署名リリース**（D1 によりローカル署名）
- **Linux 向けパッケージ**（`CLAUDE.md` の対象 OS に無い）／ **App Store 配布**／
  **自動更新**（アプリ実行時のネットワーク通信禁止のため原理的に不可）
- **Phase 2 の未達「キーボードのみでの実操作確認」。** T8 で実機に触れるが、
  Phase 2 の項目なので混ぜない（`docs/agent-protocol.md` §6「ついでにを禁止」）
- `docs/format-matrix.md` の生成先がツリー間で共有される件（Phase 3 からの提案のまま）
- `QStringLiteral` 禁止スキャナ（`phase2.md` 末尾の提案。未実装のまま）

### 停止条件の事前申告

| 停止条件 | 該当箇所 |
|---|---|
| 1（指示書にない依存・機能） | アイコン（**未決のまま。T2 で確認する**）、bundle identifier、Inno Setup の導入 |
| 3（Qt API の挙動に確信が持てない） | Qt 6.8.3 の `macdeployqt` の署名オプション、`windeployqt` の出力構成。**T1 で実測に置き換える。ローカル 6.11.1 は実測済み** |
| 7（ライセンス判断） | T4 の同梱一覧と法文。**確定を提示して承認を得てから T5 / T6 に進む**（Phase 3 T2 と同じ運び） |
| 8（1 タスク 400 行超） | T4（法文。上記の読み取り 3）と T7（250〜350 行）。**T7 が超えそうなら分割して報告する** |

### 変更ファイル

- 追加: `docs/progress/phase4.md`（本ファイル）
- 変更: `docs/phases.md`（§5.5 を追加）

### 追加・変更したテスト

**なし。** T0 は文書のみ。

### 品質ゲートの実行結果

文書のみの変更のため、コンパイル系のゲートは対象外。
着手時点のベースラインとして次を実行した（macOS 26.6.1 / arm64、Qt 6.11.1）。

| # | コマンド | 結果 |
|---|---|---|
| 1 | `cmake --build --preset dev` | `ninja: no work to do.`（**再コンパイルは発生していない。この実行を「警告ゼロ」の根拠にはしない**） |
| 2 | `ctest --preset dev` | **172 / 172 pass**（31.56 秒） |

### 推測で埋めた箇所

**5 件ある。いずれも T1 以降で実測に置き換える。**

1. **Windows の `windeployqt` の出力構成**（どの DLL / プラグインが入るか）。未実測 → T1
2. **`WIN32_EXECUTABLE` 未設定の Windows ビルドでコンソール窓が出るか。** 未実測 → T1。
   なお T2 では既定に頼らず明示するため、結論がどちらでも実装方針は変わらない
3. **Qt 6.8.3 の `macdeployqt` が 6.11.1 と同じ署名オプションを持つこと。**
   ヘルプ出力を実測したのは**ローカルの 6.11.1 のみ**である → T1
4. **aqtinstall の Qt 6.8.3（macOS）が universal であること。** 実測したのは
   ローカルの 6.11.1 のみ → T1
5. **「`compile_commands.json` に `-arch` が 2 つ並ぶと clang-tidy が扱えない恐れがある」**
   （D7 の根拠の一部）。これは一般的な慣習からの推論であり
   （`docs/agent-protocol.md` §1 の 5）、**実際に universal 構成で clang-tidy を
   走らせて確かめてはいない。** T3 で release プリセットを作る際に確かめる

加えて、T8 の前提としている「**ローカルの Qt は `~/Qt` 配下にしか無い**」は、
`ls ~/Qt` で公式インストーラ版を確認しただけであり、**Homebrew 版 Qt の有無は
確認していない。** T8 の着手時に確認する。

### 残課題 / 次にやること

1. **アイコンの素材**（読み取り 1）。T2 に入る前に利用者へ確認する
2. T1: CI で 4 点を実測し、上記「推測で埋めた箇所」1〜4 を置き換える
3. T2 以降は T1 の結果を見てから着手する
4. `.serena/` は追跡済み（`e299a94`）。Phase 3 の残課題 6 はここで解消している

---

## 2026-08-11 — アプリアイコンを作成した。**判断 1 が解消した**

### 実施内容

T0 で「未決のまま残る」と書いたアプリアイコン（判断 1）について、利用者から
「新たに作成してほしい」との指示があったため作成した。**`src/` と CMake は触っていない。**
配布物への接続は T2 で行う。

### 意匠と、その判断の経緯（**却下した案も残す**）

**採った意匠**: 角丸の四角を斜めに断ち、二片を断面に沿ってずらしたもの。
地は藍の階調、図は紙白。四角が「かたち」、断ちとずれが「別のかたちへ組み直される瞬間」を表す。
**このアプリがしていることそのもの**であり、同時に単純な図形なので小さい寸法でも輪郭が残る。

**却下した案 1: 「彡」を三本の払いにしたもの**（第 1 稿）。
「形」の部首なので名に直結し、抽象図形として日本語話者以外にも通じると考えた。
**実際に描いて 16〜128px で確認したところ、「テキスト整列」や「ハンバーガーメニュー」に見えた。**
払いが長く・寝すぎ・太さが一定だったため、部首ではなく UI アイコンの語彙になっていた。

**却下した案 2: 同じ「彡」を短く・急に・先細りにしたもの**（第 2 稿）。
棒には見えなくなったが、今度は「引っかき傷」に見え、16px では雑音になった。
**部首を単独で図案にするのは、この寸法では成立しないと判断した。**

**ずれ幅の決定**: 断面の直交方向 ±19 / 断面に沿って ∓38 に落ち着いた。
3 通りを描いて実寸で比べた結果である。

| 案 | 直交 / 沿い | 見え方 |
|---|---|---|
| 弱 | ±15 / ∓17 | 輪郭が素の四角のままで、**ずれが伝わらない**（縞の入った四角に見える） |
| **中（採用）** | **±19 / ∓38** | **輪郭に食い違いが出て「一つのかたちが断たれてずれた」と読める。32px でも成立** |
| 強 | ±22 / ∓56 | ずれは明快だが、**二つの平行四辺形に分離して見える** |

**装飾は足していない。** 地の階調のみで、光沢も影も入れていない
（`docs/spec-core.md` §7 の「アニメーション / 独自テーマを持たない」という
アプリの性格に合わせた）。小さい寸法では断面が閉じて素の四角に戻るが、それでよいと判断した。

### 追加ファイル

| ファイル | 役割 |
|---|---|
| `packaging/icons/katachi.svg` | **原版。唯一の出所。** 全面 1024、角丸 230、版面 492、断ち -27 度。寸法の根拠をコメントで持たせた |
| `packaging/icons/katachi.icns` | macOS 用。**10 エントリ**（16 / 32 / 128 / 256 / 512 とそれぞれの @2x）。Big Sur 以降の格子に合わせ 1024 の中に 824 の版面を置いた |
| `packaging/icons/katachi.ico` | Windows 用。**7 エントリ**（16 / 24 / 32 / 48 / 64 / 128 / 256）。全面 |
| `packaging/icons/build.sh` | 原版から 2 つを作り直す。**ビルド依存ではない**（CMake から呼ばない） |

**生成物をリポジトリに置く判断について申告する。** テストフィクスチャは
「生成物はコミットせずビルド時に作る」方針だが（`tests/CMakeLists.txt`）、アイコンは
**それに倣わなかった。** ビルドする側に `rsvg-convert` / ImageMagick / Pillow を要求すると、
`docs/phases.md` §1.5 の依存表に無いビルド依存が 3 つ増えるためである。
代わりに `build.sh` を残し、**原版から作り直せることを実際に確かめた**（下記）。

### 追加・変更したテスト

**なし。** アイコンは `src/` の外にあり、CMake にも接続していない。
配布物に正しく載ることの検査は **T7 の P7 の範囲**で扱う（成果物の検査に含める）。

### 品質ゲートの実行結果

| # | 実行 | 結果 |
|---|---|---|
| 1 | `ctest --preset dev`（回帰確認） | **172 / 172 pass**（28.98 秒） |
| 2 | `.icns` を `iconutil -c iconset` で展開 | **10 エントリ**、各 PNG の実寸が名前と一致 |
| 3 | `.ico` を `magick identify` | **7 エントリ**、16〜256 |
| 4 | **再現性**: `build.sh` を 2 回走らせて MD5 を比較 | **`.icns` / `.ico` とも一致**（`40ed8298…` / `5dd3b3aa…`） |
| 5 | 明背景・暗背景で 16 / 32 / 48 / 64 / 128px を目視 | 32px まで断面が読める。16px では素の四角に戻る（想定どおり） |

コンパイル系のゲート（clang-format / clang-tidy / asan）は、`src/` にも
`*.cpp` / `*.hpp` にも変更が無いため対象外。

### 推測で埋めた箇所

**1 件ある。**

**`.ico` の全エントリを PNG 圧縮で格納しなかった判断。** 256 のみ PNG、
それ以下は BMP とした（Pillow の既定）。全サイズを PNG にすればファイルは
さらに小さくなるが、**古い読み手や Inno Setup が PNG エントリを扱えるかを
一次情報で確認していない。** 対象は Windows 10 以上なので技術的には通る見込みだが、
**確認していないので広く通る側を選んだ。** 19,812 バイトであり、大きさは問題にならない。

### 残課題 / 次にやること

1. **T2 でアイコンを配布物に接続する**（`MACOSX_BUNDLE_ICON_FILE` と Windows の `.rc`）
2. T1（CI での実測 4 点）は未着手のまま。**次はこれに戻る**
3. 判断 1 が解消したため、**T0 で挙げた未決事項は無くなった**
