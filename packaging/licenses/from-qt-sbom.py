#!/usr/bin/env python3
"""Qt の SBOM から、同梱する Qt コンポーネントが含む第三者コードを列挙する。

**ビルド依存ではない。** 同梱物の構成を変えたときに手元で走らせ、
qt-third-party.md を作り直すための道具である。

なぜ SBOM を典拠にするか:
    Qt のバイナリ配布物にはライセンス文が同梱されていないことを実測した
    （~/Qt/<ver>/<plat>/Licenses は Qt 自身の 5 ファイルのみ）。
    一方 Qt 6.8 以降は SPDX 2.3 形式の SBOM を同梱しており、
    第三者コードとそのライセンスが機械可読な形で入っている。
    **公式ドキュメントを人が読み写すより、こちらのほうが確実である。**

Qt 公式の方針（doc.qt.io/qt-6/licenses-used-in-qt.html）:
    "You only need to acknowledge and comply with the licenses of the
     third-party components that you are actually shipping with your
     application."
    → SHIPPED_TARGETS に「実際に同梱するもの」だけを書く。

**この列挙の粒度についての限界（偽らないために書く）:**
    SBOM の DEPENDS_ON を推移的にたどって集めている。したがって
    「Qt が SBOM に記載した範囲で」正しい。SBOM に記載が無い第三者コードは
    検出できない。**完全性は Qt の SBOM の正確さに依存する。**

使い方:
    ./from-qt-sbom.py [Qt のインストール接頭辞] > qt-third-party.md
"""
import collections
import glob
import json
import os
import sys

# T4 前半の実測で確定した同梱物（docs/progress/phase4.md）。
# macOS の 6 フレームワーク + 14 プラグインに対応する SBOM 上のターゲット名。
# Windows も同じ方針で削るため、プラットフォーム固有のものを両方入れてある。
SHIPPED_TARGETS = {
    # フレームワーク / DLL
    "Concurrent", "Core", "DBus", "Gui", "Svg", "Widgets",
    # 画像フォーマットのプラグイン
    "QGifPlugin", "QICNSPlugin", "QICOPlugin", "QJpegPlugin",
    "QMacHeifPlugin", "QMacJp2Plugin", "QSvgPlugin", "QTgaPlugin",
    "QTiffPlugin", "QWbmpPlugin", "QWebpPlugin",
    # アイコンエンジン / プラットフォーム / スタイル
    "QSvgIconPlugin",
    "QCocoaIntegrationPlugin", "QWindowsIntegrationPlugin",
    "QMacStylePlugin", "QModernWindowsStylePlugin",
}

# SBOM を読む Qt モジュール。同梱物がこの 3 つに収まることを実測している。
SBOM_MODULES = ("qtbase", "qtsvg", "qtimageformats")

# CMake の構成用ターゲット。**成果物には入らない**ため、経由したものは別扱いにする。
BUILD_ONLY_TARGETS = {"Platform", "PlatformModuleInternal", "PlatformCommonInternal"}


def is_qt_own(license_id: str) -> bool:
    """Qt 自身のライセンス、または記載なしか。"""
    return (not license_id
            or license_id == "NOASSERTION"
            or license_id.startswith("LicenseRef-Qt-Commercial"))


def load(prefix: str):
    packages: dict[str, dict] = {}
    edges: dict[str, set[str]] = collections.defaultdict(set)
    extracted: dict[str, str] = {}
    found_files = []

    for path in sorted(glob.glob(os.path.join(prefix, "sbom", "*.spdx.json"))):
        if os.path.basename(path).split("-")[0] not in SBOM_MODULES:
            continue
        found_files.append(os.path.basename(path))
        document = json.load(open(path, encoding="utf-8"))
        by_id = {p["SPDXID"]: p for p in document["packages"]}
        packages.update({p["name"]: p for p in document["packages"]})
        for relation in document.get("relationships", []):
            if relation["relationshipType"] != "DEPENDS_ON":
                continue
            source = by_id.get(relation["spdxElementId"])
            target = by_id.get(relation["relatedSpdxElement"])
            if source and target:
                edges[source["name"]].add(target["name"])
        # LicenseRef-* の法文は SPDX の一覧に無く、SBOM 自身が持っている。
        for info in document.get("hasExtractedLicensingInfos", []):
            if info.get("licenseId") and info.get("extractedText"):
                extracted[info["licenseId"]] = info["extractedText"]

    if not found_files:
        sys.exit(f"SBOM が見つからない: {prefix}/sbom/*.spdx.json\n"
                 "Qt 6.8 以降のインストールを指定すること。")
    return packages, edges, extracted, found_files


def collect(packages, edges):
    """同梱ターゲットから DEPENDS_ON を推移的にたどり、第三者コードを集める。"""
    visited: set[str] = set()
    stack = list(SHIPPED_TARGETS)
    result: dict[tuple, set[str]] = {}

    while stack:
        node = stack.pop()
        if node in visited:
            continue
        visited.add(node)
        for name in edges.get(node, ()):
            package = packages.get(name, {})
            license_id = (package.get("licenseConcluded")
                          or package.get("licenseDeclared") or "")
            if not is_qt_own(license_id):
                display = (name.split("_Attribution_", 1)[1]
                           if "_Attribution_" in name else name)
                key = (display, package.get("versionInfo", "unknown"), license_id)
                result.setdefault(key, set()).add(node)
            stack.append(name)
    return result


LIMIT_NOTE = (
    "**この列挙の限界。** SBOM の `DEPENDS_ON` を推移的にたどった結果であり、"
    "**Qt が SBOM に記載した範囲でのみ正しい。** 記載の無い第三者コードは検出できない。"
    "「機械で確認済み」と言えるのはここまでである。")


def main() -> None:
    prefix = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
        "~/Qt/6.11.1/macos")
    packages, edges, extracted, files = load(prefix)
    found = collect(packages, edges)

    shipped, build_only = {}, {}
    for key, via in found.items():
        (build_only if via <= BUILD_ONLY_TARGETS else shipped)[key] = via

    qt_version = os.path.basename(os.path.dirname(prefix.rstrip("/")))
    here = os.path.dirname(os.path.abspath(__file__))

    def entry(key, via):
        name, version, license_id = key
        package = packages.get(name) or next(
            (p for n, p in packages.items() if n.endswith("_Attribution_" + name)), {})
        return {
            "name": name,
            "version": version,
            "license": license_id,
            "via": sorted(via),
            "copyright": (package.get("copyrightText") or "").strip(),
            "download": package.get("downloadLocation", ""),
        }

    # 機械可読。cmake/ThirdPartyLicenses.cmake が読む。
    document = {
        "qtVersion": qt_version,
        "sbomFiles": files,
        "limitation": LIMIT_NOTE,
        "shipped": [entry(k, v) for k, v in sorted(shipped.items())],
        "buildOnly": [entry(k, v) for k, v in sorted(build_only.items())],
    }
    with open(os.path.join(here, "qt-third-party.json"), "w", encoding="utf-8") as out:
        json.dump(document, out, ensure_ascii=False, indent=2)
        out.write("\n")

    # 人が読む用。
    lines = ["<!-- from-qt-sbom.py が生成する。手で編集しない。 -->",
             f"# 同梱する Qt が含む第三者コード（Qt {qt_version} の SBOM より）", "",
             f"読んだ SBOM: {', '.join(files)}", "",
             f"**成果物に入るもの: {len(shipped)} 件**", "",
             "| 第三者コード | 版 | ライセンス (SPDX) | 経由 |", "|---|---|---|---|"]
    for key, via in sorted(shipped.items()):
        lines.append(f"| `{key[0]}` | {key[1]} | `{key[2]}` | {', '.join(sorted(via))} |")
    if build_only:
        lines += ["", f"**ビルド構成のみで、成果物に入らないもの: {len(build_only)} 件**", "",
                  "| 第三者コード | 版 | ライセンス (SPDX) | 経由 |", "|---|---|---|---|"]
        for key, via in sorted(build_only.items()):
            lines.append(f"| `{key[0]}` | {key[1]} | `{key[2]}` | {', '.join(sorted(via))} |")
    lines += ["", "---", "", LIMIT_NOTE, ""]
    with open(os.path.join(here, "qt-third-party.md"), "w", encoding="utf-8") as out:
        out.write("\n".join(lines))

    # 同梱物が使う LicenseRef-* の法文を spdx/ へ書き出す。
    # 標準の SPDX 識別子は fetch-spdx-texts.py が取得し、こちらは SBOM 内の本文を使う。
    needed_refs = set()
    for key in shipped:
        for token in key[2].replace(" AND ", "|").replace(" OR ", "|").split("|"):
            token = token.strip()
            if token.startswith("LicenseRef-"):
                needed_refs.add(token)

    spdx_dir = os.path.join(here, "spdx")
    os.makedirs(spdx_dir, exist_ok=True)
    written = []
    for license_id in sorted(needed_refs):
        text = extracted.get(license_id)
        if not text:
            sys.exit(f"SBOM に {license_id} の本文が無い。手当てが要る。")
        with open(os.path.join(spdx_dir, license_id + ".txt"), "w",
                  encoding="utf-8") as out:
            out.write(text.rstrip() + "\n")
        written.append(license_id)

    print(f"成果物に入るもの {len(shipped)} 件 / ビルド構成のみ {len(build_only)} 件")
    print(f"書き出した: {here}/qt-third-party.json, qt-third-party.md")
    print(f"SBOM 内の法文を書き出した ({len(written)} 種): {', '.join(written)}")


if __name__ == "__main__":
    main()
