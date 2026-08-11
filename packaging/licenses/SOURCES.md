# 法文の出所

`spdx/` に置いた法文の取得元と、取得した時点の内容。**これらは第三者の著作物であり、
本体のライセンスの対象外である**（`docs/licenses.md` §1.3）。改変しない。

## 出所は 2 系統ある

| 系統 | 取得元 | 取得する道具 |
|---|---|---|
| 標準の SPDX 識別子 | SPDX 公式が管理する法文の正本 `github.com/spdx/license-list-data` の `text/<ID>.txt` | `fetch-spdx-texts.py` |
| `LicenseRef-*` | **Qt が同梱する SBOM（SPDX 2.3）の `hasExtractedLicensingInfos`**。SPDX の一覧に無いため | `from-qt-sbom.py` |

## OR 式の選択

ライセンス式に `OR` があるときは、**本体の GPLv3-or-later と両立する側を選ぶ**。
選ばなかった側の法文は同梱しない。選択は `cmake/ThirdPartyLicenses.cmake` に定義があり、
生成される `third_party_licenses.txt` の各項目にも「どちらを選んだか」が印字される。

| ライセンス式 | 選択 | 理由 |
|---|---|---|
| `FTL OR GPL-2.0-only` | **FTL** | **GPL-2.0-only は本体の GPLv3 と両立しない**（LGPLv3 の Qt から v2 へは下げられない。ADR-0012） |
| `AFL-2.1 OR GPL-2.0-or-later` | **GPL-2.0-or-later** | **AFL-2.1 は GPL と両立しない。** or-later 側は v3 を含む |
| `CC0-1.0 OR Apache-2.0` | **CC0-1.0** | どちらも両立する。より制約の少ない側を選ぶ |

## 取得した法文

取得日: 2026-08-11 / 合計 22 種

| SPDX 識別子 | バイト数 | sha256 |
|---|---:|---|
| `Apache-2.0` | 10280 | `074e6e32c86a4c0ef8b3ed25b721ca23…` |
| `BSD-2-Clause` | 1267 | `f32fb3b417a194167cfad068223fc975…` |
| `BSD-3-Clause` | 1460 | `5a93d5831e1297ab10fe643e1a631e83…` |
| `CC0-1.0` | 7048 | `a2010f343487d3f7618affe54f789f54…` |
| `FTL` | 5979 | `ced6622122ce451cb1ea0c3c3f507a64…` |
| `GPL-2.0-or-later` | 17337 | `aaf135472f81c5b4a0dca9367e5bb5e9…` |
| `HPND-sell-variant` | 1102 | `235abc578371bf9861e3d6eee0a9ad16…` |
| `HPND` | 1187 | `4d9ae4a36816338dfa153d1d6f92645c…` |
| `IJG` | 4244 | `7658542977bfdced9e1059a6c934ce42…` |
| `Imlib2` | 2002 | `19d21c4402df7020d631511274d99139…` |
| `LGPL-3.0-only` | 42098 | `996af0513df21f7496288951c41428a0…` |
| `libpng-2.0` | 1551 | `f207d0f7375b977f76ce2437962c1c12…` |
| `Libpng` | 4218 | `7667a8c88c7a63690244988d626bcddd…` |
| `libtiff` | 1139 | `a6ecaa20c8c1b7a8215ed05e5f58764f…` |
| `LicenseRef-BSD-3-Clause-with-PCRE2-Binary-Like-Packages-Exception` | 1852 | `0526625fcc746a42d3f19a0ab357d3dc…` |
| `LicenseRef-ICC-License` | 463 | `9e34ff47b3a44814e183342e7e198d12…` |
| `LicenseRef-SHA1-Public-Domain` | 144 | `65d86b8ec6b1cd8b8f6e64fd8710274b…` |
| `MIT-open-group` | 1136 | `d5942d51b7e079b0eab4d6f9e06f1b86…` |
| `MIT` | 1078 | `b05785f9f18e6716bab63424b1145451…` |
| `Unicode-3.0` | 1995 | `f7db81051789b729fea528a63ec4c938…` |
| `X11` | 1338 | `e1963c9958b93bc2578b34f8ae22cfe2…` |
| `Zlib` | 838 | `bfb1112d49db5b1daecdfef24bd7e2f3…` |

**照合のしかた**: `./fetch-spdx-texts.py --check` は取得済みの法文の大きさと
sha256 を表示する（ネットワークを使わない）。上の表と食い違えば、
どこかで改変されたということである。
