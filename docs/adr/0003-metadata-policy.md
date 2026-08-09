# ADR-0003: `MetadataPolicy` を `PreserveSupported` に改名し、保持範囲を定義する

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

`docs/spec-core.md` §2 は当初 `enum class MetadataPolicy { PreserveAll, StripAll }` を定めていた。
`docs/phases.md` §5.2 は、この `PreserveAll` を Phase 1 で実装するか Phase 3 に送るかを
**Phase 1 着手時に、使用する Qt バージョンの EXIF / ICC の実挙動を公式ドキュメントで
確認してから判断する。今は推測しない**としていた。

Phase 1 着手時に `doc.qt.io/qt-6.8` で確認した結果は次のとおり。

| 保持対象 | Qt 6.8 の API | 可否 |
|---|---|---|
| EXIF の**向き**のみ | `QImageReader::transformation()` / `QImageWriter::setTransformation()`。`QImageIOHandler::Transformations` の説明に "usually through **EXIF**" とある | 可 |
| テキスト metadata（key/value） | `QImageReader::text()` / `textKeys()` / `QImageWriter::setText()`。`QImageIOHandler::Description` 経由（"GIF and PNG ... allow embedding of text or comments"） | 可（形式依存） |
| ICC プロファイル | `QImage::colorSpace()` / `setColorSpace()`、`QColorSpace::fromIccProfile()` / `iccProfile()` | 可 |
| **EXIF 全体**（カメラ機種・GPS・撮影日時・レンズ情報等） | **該当する API が存在しない** | **不可** |

出典（Phase 1 着手時に参照）:
`https://doc.qt.io/qt-6.8/qimagereader.html` /
`https://doc.qt.io/qt-6.8/qimagewriter.html` /
`https://doc.qt.io/qt-6.8/qimage.html` /
`https://doc.qt.io/qt-6.8/qimageiohandler.html` /
`https://doc.qt.io/qt-6.8/qcolorspace.html`

**`PreserveAll` は Qt 単体では字義どおり実装できない。**

## 選択肢

### A. 列挙名を実態に合わせる（`PreserveSupported`）

名前と実装が一致する。`docs/spec-core.md` §2 の変更を伴う。

### B. `PreserveAll` の名前を保ったまま、実際の意味を ADR に記録する

指示書を変更しないが、名前が「全部保持する」と主張し続ける。
利用者も将来の実装者も、GPS や撮影日時が保持されると誤解する。

### C. EXIF ライブラリ（exiv2 / libexif 等）を Phase 1 で導入する

字義どおり実装できるが、**指示書にない新規依存**であり `CLAUDE.md` 停止条件 1 に該当する。
`docs/licenses.md` の更新（停止条件 7）も先に必要。Phase 1 の目的は
「GUI なしで変換ロジックが完成」であり、依存追加はスコープを膨らませる。

## 決定

**A を採る。`MetadataPolicy::PreserveAll` を `PreserveSupported` に改名する。**

`PreserveSupported` の意味を次のとおり定義する。

> **Qt が読み書きできる範囲のメタデータ、すなわち
> 「向き（EXIF orientation）」「テキスト key/value」「ICC プロファイル」を保持する。
> それ以外の EXIF 情報は保持しない。**

`ConversionSpec::metadata` の既定値も `PreserveSupported` とする（既定で情報を捨てない）。

**EXIF 全体の保持は Phase 3 に送る。** 追加コーデックのライセンス調査と同じタイミングで、
外部ライブラリの導入可否を判断する。導入するなら `docs/licenses.md` の更新が先。

## 帰結

- `docs/spec-core.md` §2 の `MetadataPolicy` 定義と `ConversionSpec::metadata` の既定値を変更した。
- `IccPolicy::Embed` / `Strip` は Qt 単体で実装可能なため、**Phase 1 のスコープに残す**。
  `Embed` は入力の `QColorSpace` を出力に引き継ぎ、`Strip` は `setColorSpace(QColorSpace{})` で落とす。
- テストは「向き・テキスト・ICC が `PreserveSupported` で保持され、`StripAll` で落ちる」ことを
  確認する。**保持できない EXIF 項目についてのテストは書かない**
  （書けば「実装が足りない」ではなく「仕様どおり」を検証することになり、意味がない）。
- **名前が実態を超えないことを優先した。** `PreserveAll` のままにすると、
  Phase 2 の UI で利用者に「すべて保持」と表示することになり、事実と食い違う。
- Phase 3 で EXIF ライブラリを導入した場合、`PreserveAll` を**別の列挙値として追加**する。
  `PreserveSupported` の意味は変えない（既存の変換結果の再現性を壊さないため）。
