# リリース手順

配布物を作り、署名し、公証し、検証するまでの手順。

**署名と公証はローカルでのみ行う**（ADR-0015 論点 4）。証明書と公証の資格情報を
GitHub に置かない。**CI は未署名の成果物までを作る。CI の成功を、署名済み成果物が
正しいことの根拠として報告しない。**

Phase 4 の受け入れ基準は `docs/phases.md` §4。実施の記録は `docs/progress/phase4.md`。

---

## 0. 用意するもの

| 項目 | 確認のしかた |
|---|---|
| Developer ID Application 証明書 | `security find-identity -v -p codesigning` |
| `notarytool`（Xcode 同梱） | `xcrun --find notarytool` |
| 公証の資格情報 | 下記 §3.1 でキーチェーンに保存する |
| Qt 6.8 以上 | `qmake6 -query QT_INSTALL_PREFIX` |

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

### 3.1 資格情報をキーチェーンに保存する（初回のみ）

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

### 3.2 提出して待つ

```bash
xcrun notarytool submit build/release/Katachi-0.1.0.dmg \
  --keychain-profile katachi-notary --wait
```

失敗したら理由を取り出す。

```bash
xcrun notarytool log <submission-id> --keychain-profile katachi-notary
```

### 3.3 staple する

```bash
xcrun stapler staple build/release/Katachi-0.1.0.dmg
```

**staple しないと、ネットワークが無い環境で Gatekeeper が判断できない。**

---

## 4. 検証（**結果を `docs/progress/phase4.md` に貼る**）

```bash
# 公証の事前条件（**提出前にこれを確かめる。落ちてから理由を読むより速い**）
#   Developer ID / flags=0x10000(runtime) / Timestamp= の 3 つが全バイナリに要る
for f in build/release/dist/Katachi.app/Contents/Frameworks/*.framework \
         build/release/dist/Katachi.app/Contents/PlugIns/*/*.dylib \
         build/release/dist/Katachi.app/Contents/MacOS/Katachi \
         build/release/dist/Katachi.app; do
  codesign -dv --verbose=2 "$f" 2>&1 | grep -qE "flags=0x10000\(runtime\)" || echo "NG: $f"
done
# get-task-allow が付いていると公証は拒否される（0 件であること）
codesign -d --entitlements - build/release/dist/Katachi.app 2>&1 | grep -c get-task-allow

# 署名が正しいか
codesign --verify --deep --strict --verbose=2 build/release/dist/Katachi.app

# Gatekeeper が受け入れるか
spctl --assess --type execute --verbose=2 build/release/dist/Katachi.app

# staple が効いているか
xcrun stapler validate build/release/Katachi-0.1.0.dmg

# Qt が動的リンクか（受け入れ基準 4）
otool -L build/release/dist/Katachi.app/Contents/MacOS/Katachi | grep '@rpath/Qt'

# universal か（受け入れ基準 1）
lipo -archs build/release/dist/Katachi.app/Contents/MacOS/Katachi
```

**実行していない検証を「通った」と書かない。**

---

## 5. クリーンな環境での起動確認（受け入れ基準 5）

**CI の成功や自動テストの結果を、実機起動の根拠にしない。**

macOS は **Qt を持たない別のユーザーアカウント**で確かめる。Qt が `$HOME` 配下にしか
無いことを先に確認する（Homebrew 版 Qt の有無も見る）。

```bash
ls ~/Qt                 # 公式インストーラ版
brew list 2>/dev/null | grep -i '^qt'   # Homebrew 版
```

確認する項目。

- `.dmg` を開いて Applications へ引き込み、起動する
- **日本語入力ができる**（`platforminputcontexts` を削っているため。仮想キーボードは
  外したが macOS の IME は `cocoa` プラグインが担う、というのが削った根拠であり、
  **実機で確かめるまで確証はない**）
- 画像を D&D して変換できる
- 対応形式の一覧に svg / svgz がある（`libqsvg` を戻した効果）

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
