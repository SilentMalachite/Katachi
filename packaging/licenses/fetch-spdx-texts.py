#!/usr/bin/env python3
"""同梱物が必要とする標準 SPDX ライセンスの法文を、正本から取得する。

**ビルド依存ではない。** 同梱物の構成が変わったときに手元で走らせる道具である。
取得した法文はリポジトリに置き、ビルド時にネットワークを使わない。

典拠: SPDX license-list-data（SPDX 公式が管理する法文の正本）
      https://github.com/spdx/license-list-data

**OR 式の選択について。** ライセンス式に OR があるときは、
**本体の GPLv3-or-later と両立する側を選ぶ**（docs/licenses.md §5 の表）。
選択の理由は SOURCES.md に記録する。選ばなかった側の法文は取得しない。

使い方:
    ./fetch-spdx-texts.py          # 取得して SOURCES.md を更新する
    ./fetch-spdx-texts.py --check  # 取得済みの法文の照合のみ（ネットワークを使わない）
"""
import hashlib
import os
import subprocess
import sys

RAW = "https://raw.githubusercontent.com/spdx/license-list-data/main/text/{}.txt"

# packaging/licenses/qt-third-party.md の 43 件から、OR を解決して得た 18 種。
# 「選んだ理由」は OR 式を持つものにだけ書く。
NEEDED = {
    "Apache-2.0": "",
    "BSD-2-Clause": "",
    "BSD-3-Clause": "",
    "CC0-1.0": "blake2 の `CC0-1.0 OR Apache-2.0` はこちらを選ぶ（より制約が少ない）",
    "FTL": "freetype / grayraster の `FTL OR GPL-2.0-only` はこちらを選ぶ。"
           "**GPL-2.0-only は本体の GPLv3 と両立しない**（LGPLv3 の Qt から v2 へは下げられない。ADR-0012）",
    "GPL-2.0-or-later": "libdbus-1-headers の `AFL-2.1 OR GPL-2.0-or-later` はこちらを選ぶ。"
                        "**AFL-2.1 は GPL と両立しない**",
    "HPND": "",
    "HPND-sell-variant": "",
    "IJG": "",
    "Imlib2": "",
    # Qt 本体の条件（docs/licenses.md §2）。第三者コードの一覧とは別に、
    # third_party_licenses.txt の冒頭で Qt の条件を示すために要る。
    # LGPLv3 は GPLv3 を参照するが、GPLv3 の全文は成果物に同梱する LICENSE
    # （本体のライセンス。改変しない）が持つため、ここでは取得しない。
    "LGPL-3.0-only": "Qt 6 を LGPL v3 で利用する（動的リンクのみ。ADR-0012 / docs/licenses.md §2.1）",
    "Libpng": "",
    "libpng-2.0": "",
    "MIT": "",
    "MIT-open-group": "",
    "Unicode-3.0": "",
    "X11": "",
    "Zlib": "",
    "libtiff": "",
}

HERE = os.path.dirname(os.path.abspath(__file__))
TEXT_DIR = os.path.join(HERE, "spdx")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    check_only = "--check" in sys.argv
    os.makedirs(TEXT_DIR, exist_ok=True)
    results, failed = [], []

    for spdx_id in sorted(NEEDED, key=str.lower):
        path = os.path.join(TEXT_DIR, spdx_id + ".txt")
        if check_only:
            if not os.path.exists(path):
                failed.append((spdx_id, "未取得"))
                continue
            data = open(path, "rb").read()
        else:
            # curl を使う。python.org 版の Python は CA 証明書を持たないことがあり、
            # urllib だと CERTIFICATE_VERIFY_FAILED になる（実測）。
            # curl はシステムの信頼ストアを使うため、両 OS で同じように動く。
            url = RAW.format(spdx_id)
            done = subprocess.run(["curl", "-fsSL", "--max-time", "30", url],
                                  capture_output=True, check=False)
            if done.returncode != 0 or not done.stdout:
                failed.append((spdx_id, f"curl exit {done.returncode}: "
                                        f"{done.stderr.decode(errors='replace')[:80]}"))
                continue
            data = done.stdout
            with open(path, "wb") as out:
                out.write(data)
        results.append((spdx_id, len(data), digest(data)))

    for spdx_id, length, sha in results:
        print(f"  {spdx_id:22s} {length:7d} bytes  sha256:{sha[:16]}…")
    if failed:
        print("\n取得できなかったもの:", file=sys.stderr)
        for spdx_id, reason in failed:
            print(f"  {spdx_id}: {reason}", file=sys.stderr)
        sys.exit(1)

    print(f"\n{len(results)} 種を {TEXT_DIR} に置いた。")


if __name__ == "__main__":
    main()
