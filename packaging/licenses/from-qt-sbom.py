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

    if not found_files:
        sys.exit(f"SBOM が見つからない: {prefix}/sbom/*.spdx.json\n"
                 "Qt 6.8 以降のインストールを指定すること。")
    return packages, edges, found_files


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


def main() -> None:
    prefix = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
        "~/Qt/6.11.1/macos")
    packages, edges, files = load(prefix)
    found = collect(packages, edges)

    shipped, build_only = {}, {}
    for key, via in found.items():
        (build_only if via <= BUILD_ONLY_TARGETS else shipped)[key] = via

    qt_version = os.path.basename(os.path.dirname(prefix.rstrip("/")))

    print("<!-- from-qt-sbom.py が生成する。手で編集しない。 -->")
    print(f"# 同梱する Qt が含む第三者コード（Qt {qt_version} の SBOM より）\n")
    print(f"読んだ SBOM: {', '.join(files)}\n")
    print(f"**成果物に入るもの: {len(shipped)} 件**\n")
    print("| 第三者コード | 版 | ライセンス (SPDX) | 経由 |")
    print("|---|---|---|---|")
    for (name, version, license_id), via in sorted(shipped.items()):
        print(f"| `{name}` | {version} | `{license_id}` | {', '.join(sorted(via))} |")

    if build_only:
        print(f"\n**ビルド構成のみで、成果物に入らないもの: {len(build_only)} 件**\n")
        print("| 第三者コード | 版 | ライセンス (SPDX) | 経由 |")
        print("|---|---|---|---|")
        for (name, version, license_id), via in sorted(build_only.items()):
            print(f"| `{name}` | {version} | `{license_id}` | {', '.join(sorted(via))} |")

    print("\n---\n")
    print("**この列挙の限界。** SBOM の `DEPENDS_ON` を推移的にたどった結果であり、")
    print("**Qt が SBOM に記載した範囲でのみ正しい。** 記載の無い第三者コードは検出できない。")
    print("「機械で確認済み」と言えるのはここまでである。")


if __name__ == "__main__":
    main()
