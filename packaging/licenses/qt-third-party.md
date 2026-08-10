<!-- from-qt-sbom.py が生成する。手で編集しない。 -->
# 同梱する Qt が含む第三者コード（Qt 6.11.1 の SBOM より）

読んだ SBOM: qtbase-6.11.1.spdx.json, qtimageformats-6.11.1.spdx.json, qtsvg-6.11.1.spdx.json

**成果物に入るもの: 43 件**

| 第三者コード | 版 | ライセンス (SPDX) | 経由 |
|---|---|---|---|
| `BundledFreetype` | 2.14.3 | `FTL OR GPL-2.0-only` | Gui |
| `BundledHarfbuzz` | 14.2.0 | `MIT` | Gui |
| `BundledLibjpeg` | 3.1.4 | `IJG AND BSD-3-Clause` | QJpegPlugin |
| `BundledLibpng` | 1.6.58 | `Libpng AND libpng-2.0` | BundledFreetype, Gui |
| `BundledPcre2` | 10.47 | `LicenseRef-BSD-3-Clause-with-PCRE2-Binary-Like-Packages-Exception` | Core |
| `aglfn` | 1.7 | `BSD-3-Clause` | Gui |
| `blake2` | ed1974ea83433eba7b2d95c5dcd9ac33cb847913 | `CC0-1.0 OR Apache-2.0` | Core |
| `cocoa-platform-plugin` | unknown | `BSD-3-Clause` | QCocoaIntegrationPlugin |
| `doubleconversion` | 3.4.0 | `BSD-3-Clause` | Core |
| `easing` | unknown | `BSD-3-Clause` | Core |
| `emoji-segmenter` | 0.4.0 | `Apache-2.0` | Gui |
| `forkfd` | unknown | `MIT` | Core |
| `freetype-bdf` | 2.14.3 | `MIT` | BundledFreetype |
| `freetype-pcf` | 2.14.3 | `MIT AND MIT-open-group` | BundledFreetype |
| `freetype-zlib` | 2.14.3 | `Zlib` | BundledFreetype |
| `grayraster` | unknown | `FTL OR GPL-2.0-only` | Gui |
| `icc-srgb-color-profile` | unknown | `LicenseRef-ICC-License` | Gui |
| `libdbus-1-headers` | 1.13.12 | `AFL-2.1 OR GPL-2.0-or-later` | DBus |
| `libtiff` | 4.7.1 | `libtiff` | QTiffPlugin |
| `libwebp` | 1.6.0 | `BSD-3-Clause` | QWebpPlugin |
| `md4` | unknown | `CC0-1.0` | Core |
| `md4c` | 0.5.2 | `MIT` | Gui |
| `md5` | unknown | `CC0-1.0` | Core |
| `opengl-es2-headers` | Revision 27673 | `MIT` | Gui |
| `opengl-headers` | Revision 27684 | `MIT` | Gui |
| `pcre2-sljit` | 10.47 | `BSD-2-Clause` | BundledPcre2 |
| `qeventdispatcher_cf` | unknown | `BSD-3-Clause` | Core |
| `rfc6234` | unknown | `BSD-3-Clause` | Core |
| `rhi-miniengine-d3d12-mipmap` | 0aa79bad78992da0b6a8279ddb9002c1753cb849 | `MIT` | Gui |
| `sha1` | unknown | `LicenseRef-SHA1-Public-Domain` | Core |
| `sha3_endian` | 1.0.0 | `BSD-2-Clause` | Core |
| `sha3_keccak` | 3.2 | `CC0-1.0` | Core |
| `siphash` | unknown | `CC0-1.0` | Core |
| `smooth-scaling-algorithm` | unknown | `BSD-2-Clause AND Imlib2` | Gui |
| `tika-mimetypes` | 408c26e1e03e018a623e732dff6fb047a2fb8e19 | `Apache-2.0` | Core |
| `tinycbor` | 7.0 | `MIT` | Core |
| `tlexpected` | 41d3e1f48d682992a2230b2a715bca38b848b269 | `CC0-1.0` | Core |
| `unicode-character-database` | 36 | `Unicode-3.0` | Core |
| `unicode-cldr` | v48.1 | `Unicode-3.0` | Core |
| `vulkanmemoryallocator` | 3.2.1 | `MIT` | Gui |
| `webgradients` | unknown | `MIT` | Gui |
| `xserverhelper` | unknown | `X11 AND HPND` | Gui |
| `xsvg` | unknown | `HPND-sell-variant` | Svg |

**ビルド構成のみで、成果物に入らないもの: 2 件**

| 第三者コード | 版 | ライセンス (SPDX) | 経由 |
|---|---|---|---|
| `extra-cmake-modules` | 5.84.0 | `BSD-3-Clause` | Platform |
| `kwin` | 5.13.4 | `BSD-3-Clause` | Platform |

---

**この列挙の限界。** SBOM の `DEPENDS_ON` を推移的にたどった結果であり、
**Qt が SBOM に記載した範囲でのみ正しい。** 記載の無い第三者コードは検出できない。
「機械で確認済み」と言えるのはここまでである。
