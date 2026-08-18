# リリース手順

配布物を作り、署名し、（任意で）公証し、検証するまでの手順。

**署名はローカルでのみ行う**（ADR-0015 論点 4）。証明書を GitHub に置かない。
**CI は未署名の成果物までを作る。CI の成功を、署名済み成果物が正しいことの根拠として
報告しない。**

**公証は任意である**（2026-08-17 の決定。ADR-0015 追記）。受け入れ基準 1 は
Developer ID 署名までを必須とし、公証は行わなくてよい。公証なしで配るときの受け手向け
手順は §3.0。公証を行う場合の手順は §3.1 以降に残す。

Phase 4 の受け入れ基準は `docs/phases.md` §4。実施の記録は `docs/progress/phase4.md`。

---

## 0. 用意するもの

| 項目 | 必須か | 確認のしかた |
|---|---|---|
| Developer ID Application 証明書 | **必須**（macOS 配布） | `security find-identity -v -p codesigning` |
| Qt 6.8 以上 | **必須** | `qmake6 -query QT_INSTALL_PREFIX` |
| `notarytool`（Xcode 同梱） | 公証する場合のみ | `xcrun --find notarytool` |
| 公証の資格情報 | 公証する場合のみ | 下記 §3.1 でキーチェーンに保存する |

---

## 1. ビルドと据え付け

```bash
export QT_ROOT_DIR=/path/to/Qt/6.8.3/macos

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure

rm -rf build/release/dist
cmake --install build/release --prefix build/release/dist
```

`release` プリセットは macOS を universal（`x86_64;arm64`）にする。

> **配布用の置き場を `dist` にする理由（実測した落とし穴）。**
> `ctest --preset release` の `package.build` は、**署名なしで**組み立てを
> `build/release/package` へ行う（CI で署名しない設計上、正しい動作）。
> 同じ置き場を使うと、テストを走らせた瞬間に署名済みの成果物が
> 未署名で上書きされる。**実際に一度そうなった。** 置き場を分ける。

---

## 2. 配布物を組み立てる

```bash
cmake -DKATACHI_STAGE_DIR="$PWD/build/release/dist" \
      -DKATACHI_SOURCE_DIR="$PWD" \
      -DKATACHI_QT_PREFIX="$(qmake6 -query QT_INSTALL_PREFIX)" \
      -DKATACHI_APP_VERSION=0.1.0 \
      -DKATACHI_QT_VERSION=6.8.3 \
      -DKATACHI_SIGN_IDENTITY="Developer ID Application: 名前 (TEAMID)" \
      -DKATACHI_OUTPUT_DMG="$PWD/build/release/Katachi-0.1.0.dmg" \
      -P cmake/PackageMacOS.cmake
```

このスクリプトが行うこと。

1. `macdeployqt` を走らせる
2. **使わないものを削る**（仮想キーボードと、それだけが引く 9 フレームワーク。ADR-0015 D12）
3. **`macdeployqt` が落とす `imageformats/libqsvg` を戻す**（放置すると配布物だけ SVG が読めなくなる）
4. `third_party_licenses.txt` を作り、`LICENSE` を複製して `Contents/Resources/` へ置く
5. 削り忘れ・画像プラグインの欠落・同梱物の不足を検査する（欠けたら止まる）
6. 署名する（`KATACHI_SIGN_IDENTITY` を渡したとき）
7. `.dmg` を作る（`KATACHI_OUTPUT_DMG` を渡したとき）

### 2.1 **署名はキーチェーンの許可を求める**

**初回の署名で、秘密鍵へのアクセスを許可するダイアログが出る。**
非対話のシェル（CI、エディタの統合端末、エージェント）からは進まず、そのまま止まる
（**実測。`--timestamp` を外しても止まる**ため、タイムスタンプサーバの問題ではない）。

**対処**: 端末から手で 1 度実行し、ダイアログで「**常に許可**」を選ぶ。以後は非対話でも通る。

```bash
codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: 名前 (TEAMID)" /path/to/なにか小さいファイル
```

---

## 3. 公証

**公証は任意である。** 受け入れ基準 1 は Developer ID 署名までで足りる。
公証しない場合は §3.0 だけを行い、§3.1 以降は飛ばす。

### 3.0 公証なしで配る場合（**既定の経路**）

Developer ID で署名しただけの `.dmg` / `.app` は、Gatekeeper が次のように判定する
（**実測済み**。`docs/progress/phase4.md`）。

```text
spctl --assess ... → Unnotarized Developer ID
```

これは署名失敗ではなく、**未公証の Developer ID 配布として正しい状態**である。
受け手がダウンロードしたアプリを開くとき、macOS は警告を出すことがある。

#### 受け手に伝えること（README にも同趣旨を書く）

1. **Finder で右クリック →「開く」**を選ぶ（ダブルクリックだけでは拒否されることがある）
2. ダイアログで再度「開く」を選ぶ
3. それでも開かない場合は、隔離属性を外す（ターミナル）:

```bash
xattr -d com.apple.quarantine /Applications/Katachi.app
# または .dmg からコピーした直後のパス
```

4. `codesign --verify --deep --strict` が通っていることだけは、配布者が事前に確かめる
   （§4 の必須検証）。`stapler validate` と公証済みの `spctl` 合格は求めない

#### 配布者がやってはいけないこと

- 未公証であることを隠す
- `spctl` の `Unnotarized Developer ID` を「失敗」として受け入れ基準に落とす
- 受け手に「公証済みと同じ安全性」と説明する

公証を後から足す場合は、同じ `.dmg` に対して §3.1 以降を実行すればよい
（署名のやり直しは不要。ただし再パッケージしたら再署名が要る）。

### 3.1 資格情報をキーチェーンに保存する（公証する場合・初回のみ）

App Store Connect の API キーを使う方法（**推奨**。Apple ID と違い 2 要素認証の影響を受けない）。

```bash
xcrun notarytool store-credentials katachi-notary \
  --key /path/to/AuthKey_XXXXXXXXXX.p8 \
  --key-id XXXXXXXXXX \
  --issuer 00000000-0000-0000-0000-000000000000
```

Apple ID を使う方法（アプリ用パスワードが要る）。

```bash
xcrun notarytool store-credentials katachi-notary \
  --apple-id you@example.com --team-id TEAMID --password アプリ用パスワード
```

### 3.2 提出して待つ（公証する場合）

```bash
xcrun notarytool submit build/release/Katachi-0.1.0.dmg \
  --keychain-profile katachi-notary --wait
```

失敗したら理由を取り出す。

```bash
xcrun notarytool log <submission-id> --keychain-profile katachi-notary
```

### 3.3 staple する（公証する場合）

```bash
xcrun stapler staple build/release/Katachi-0.1.0.dmg
```

**staple しないと、ネットワークが無い環境で Gatekeeper が公証の有無を判断できない。**

---

## 4. 検証（**結果を `docs/progress/phase4.md` に貼る**）

### 4.1 必須（受け入れ基準 1・4）

```bash
# Developer ID / flags=0x10000(runtime) / Timestamp= の 3 つが全バイナリに要る
for f in build/release/dist/Katachi.app/Contents/Frameworks/*.framework \
         build/release/dist/Katachi.app/Contents/PlugIns/*/*.dylib \
         build/release/dist/Katachi.app/Contents/MacOS/Katachi \
         build/release/dist/Katachi.app; do
  codesign -dv --verbose=2 "$f" 2>&1 | grep -qE "flags=0x10000\(runtime\)" || echo "NG: $f"
done

# 署名が正しいか
codesign --verify --deep --strict --verbose=2 build/release/dist/Katachi.app

# Qt が動的リンクか（受け入れ基準 4）
otool -L build/release/dist/Katachi.app/Contents/MacOS/Katachi | grep '@rpath/Qt'

# universal か（受け入れ基準 1）
lipo -archs build/release/dist/Katachi.app/Contents/MacOS/Katachi
```

公証していない場合、次は **失敗してよい**（むしろ未公証の正しい表示）。

```bash
spctl --assess --type execute --verbose=2 build/release/dist/Katachi.app
# 期待: Unnotarized Developer ID
```

### 4.2 公証した場合のみ

```bash
# 公証の事前条件（提出前に確かめる）
codesign -d --entitlements - build/release/dist/Katachi.app 2>&1 | grep -c get-task-allow
# get-task-allow が付いていると公証は拒否される（0 件であること）

spctl --assess --type execute --verbose=2 build/release/dist/Katachi.app
xcrun stapler validate build/release/Katachi-0.1.0.dmg
```

**実行していない検証を「通った」と書かない。**

---

## 5. クリーンな環境での起動確認（受け入れ基準 5）

### 5.1 macOS（**実機が必須**）

**macOS については、CI の成功や自動テストの結果を実機起動の根拠にしない。**

**Qt を持たない別のユーザーアカウント**で確かめる。Qt が `$HOME` 配下にしか
無いことを先に確認する（Homebrew 版 Qt の有無も見る）。

```bash
ls ~/Qt                 # 公式インストーラ版
brew list 2>/dev/null | grep -i '^qt'   # Homebrew 版
```

確認する項目。

- `.dmg` を開いて Applications へ引き込み、起動する
  （未公証なら §3.0 の「右クリック → 開く」経路も確認する）
- **日本語入力ができる**（`platforminputcontexts` を削っているため。仮想キーボードは
  外したが macOS の IME は `cocoa` プラグインが担う、というのが削った根拠であり、
  **実機で確かめるまで確証はない**）
- 画像を D&D して変換できる
- 対応形式の一覧に svg / svgz がある（`libqsvg` を戻した効果）

確認結果は `docs/progress/phase4.md` の「T8 実機確認（記入用）」へ**追記のみ**で残す。
結果を書いたあと、§5.3 のクローズ手順を実行する。

### 5.2 Windows（**CI の機械検査で足りる**）

Windows 実機は必須としない（2026-08-17 の決定）。受け入れ基準 5 の Windows 側は、
`release` プリセットの **P9（依存解決）** と `package` ジョブの成功をもって達成とする。

実機が手元に無いことの帰結として、次は **未確認のまま残す**（「通った」と書かない）。

- GPU ドライバの無い環境での `opengl32sw.dll` の要否
- SmartScreen を実際にクリックして進めた経験
- インストーラ / ポータブル zip の人手による起動感

手順の詳細は `docs/progress/phase4.md` の T6・T7 の記録を参照。

### 5.3 Phase 4 クローズ手順（**§5.1 が全部 OK になったあと**）

**この節を読むだけではクローズしない。** §5.1 の実機結果を `phase4.md` に書いたあと、
次をこの順で行う。どれか 1 つでも未記入なら止め、基準 5 を `[x]` にしない。

1. `docs/progress/phase4.md` の「T8 実機確認（記入用）」をすべて埋める  
   （日付・OS 版・アカウントが Qt 無しであること・チェック項目の可否）
2. 同ファイル末尾に「Phase 4 クローズ」節を**追記**する（既存文は消さない）
3. `docs/phases.md` §4 Phase 4 の基準 5 を `- [x]` にし、根拠表の行 5 を「達成」と実機根拠に直す
4. `README.md` の「状態」を Phase 4 完了に直し、未確認一覧から **macOS クリーン環境**と
   **仮想キーボード / 日本語入力**のうち実機で確認したものを外す（確認していない項目は残す）
5. コミットメッセージ例: `docs: Phase 4 をクローズする（macOS 実機確認済み）`

**クローズしても「未確認のまま残してよい」もの**（必須ではない）:

- 公証（任意。§3.1 以降）
- Windows 実機 / GPU 無し環境 / SmartScreen の人手操作
- キーボードのみでの開始・キャンセル（Phase 2 からの継続）
- Windows での追加コーデック

---

## 6. GPLv3 §6: 対応するソースの提供

**配布物と同じタグのソースを添える**（ADR-0016 D9）。公開リポジトリへの URL だけに頼らない。

```bash
git archive --format=tar.gz --prefix=katachi-0.1.0/ -o build/release/katachi-0.1.0-src.tar.gz v0.1.0
```

---

## 7. Windows

`docs/progress/phase4.md` の T6 の記録を参照。**未署名で配布する**（ADR-0015 論点 5）。
SmartScreen の警告が出ることを `README.md` に明記してある。

**ポータブル zip は Microsoft Visual C++ 再頒布可能パッケージ (x64) を要求する**
（ADR-0016 D13。個々の CRT DLL は再頒布が許諾されていない）。
