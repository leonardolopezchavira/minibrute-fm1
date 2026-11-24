#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The SMK-37's web installers: fm1-base's installer page (web/installer.html, as tools/make_site.py makes it) for
each SMK package, its words for the SMK-37, without what is the FM-1's only (its stock firmware's download,
m-bootloader). After SQ-80's (sq80-fm1 tools/make_smk_install.py); MB-1 has no install steps of its own.

    python3 tools/make_smk_install.py PKG_DIR VERSION SITE_DIR
      -> SITE_DIR/smk37/install/ (SMK-37 Pro, Starrykey 37), install-elite/, install-mke/; the packages in smk37/;
         with the site's backup/ page (fm1.toml backup), smk37/backup/, backup-elite/, backup-mke/ for their identities
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "fm1" / "base"
sys.path.insert(0, str(BASE / "tools"))
from make_site import product_of, read_conf, strip_module   # noqa: E402

MODELS = [("", "install"), ("-elite", "install-elite"), ("-mke", "install-mke")]
KEEP = ("FM-1_099", "FM-1.fwsc", "M-VAVE and FM-1 are trademarks")   # (the FM-1's own: left as they are)


def smk_words(html):
    """FM-1 -> SMK-37 in the page's words, the FM-1's own names kept"""
    for i, k in enumerate(KEEP):
        html = html.replace(k, f"\0{i}\0")
    html = html.replace("FM-1", "SMK-37")
    for i, k in enumerate(KEEP):
        html = html.replace(f"\0{i}\0", k)
    return html


def hide(html, start, end):
    """the part from start to end (both kept) in a hidden div: its elements stay for the page's script"""
    i, j = html.index(start), html.index(end)
    return html[:i] + '<div hidden>' + html[i:j] + '</div>\n  ' + html[j:]


def main(pkg_dir, version, site):
    pkg_dir, site = Path(pkg_dir), Path(site)
    conf = read_conf(ROOT / "app" / "fm1.toml")
    html0 = (BASE / "web" / "installer.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((BASE / "web" / f).read_text(encoding="utf-8"))
                    for f in ("fm1ports.js", "fm1pkg.js", "fm1ota.js"))
    app_js = "null"                                   # (no install steps of MB-1's own)
    out_pkg = site / "smk37"
    out_pkg.mkdir(parents=True, exist_ok=True)
    repo = conf.get("repo", "")
    for suffix, sub in MODELS:
        name = f"mb1-smk37{suffix}-{version}.fwsc"
        src = next(iter(pkg_dir.rglob(name)), None)
        if not src:
            raise SystemExit(f"{pkg_dir}: no {name}")
        if not (out_pkg / name).exists():
            shutil.copy(src, out_pkg / name)
        product = product_of(src.read_bytes())
        html = html0
        html = hide(html, '<p class="small"><span data-t="brick">', '<p class="small"><a href="https://www.m-vave.com/download"')
        html = hide(html, '<h2 data-t="stockh">', '<h2 data-t="reporth">')
        html = html.replace('<a class="baklink" href="../backup/"', '<a class="baklink" href="../#backup"')
        html = smk_words(html)
        for a, b in (('backuplink: "Back up {{NAME}}"', 'backuplink: "The keyboard\'s full backup (optional)"'),
                     ('confirmbak: "Installing can lose what {{NAME}} has saved."',
                      'confirmbak: "Installing replaces the keyboard\'s stock presets; a full backup keeps them."'),
                     ('backuplink: "{{NAME}} をバックアップする"', 'backuplink: "キーボードのフルバックアップ（任意）"'),
                     ('confirmbak: "書き込みで {{NAME}} に保存したものが消えることがあります。"',
                      'confirmbak: "書き込みでキーボードの純正プリセットは置き換わります。フルバックアップで残せます。"'),
                     ('bakh: "Back up first."', 'bakh: "Before you install."'),
                     ('bakh: "先にバックアップを。"', 'bakh: "インストールの前に。"')):
            if html.count(a) != 1:
                raise SystemExit(f"installer.html: {a!r} not found once")
            html = html.replace(a, b)
        meta = json.dumps({"version": version, "product": product, "pkg": "../" + name, "backup": True, "res": None,
                           "boot": None})
        for key, val in (("NAME", conf["name"]), ("REPO", repo), ("REPO_SHORT", re.sub(r"^https?://", "", repo)),
                         ("LICENSE", conf.get("licence", "MPL-2.0")),
                         ("CREDITS", " " + conf.get("credits", "") + " The SMK-37 layer after genkigenki's fm1-to-smk37 (GPL-3.0)."),
                         ("RES", "MB-1's data"), ("RES_JA", "MB-1 のデータ")):
            html = html.replace("{{" + key + "}}", val)
        html = html.replace("MB1", "MB-1").replace("<title>", "<title>SMK-37 · ", 1)
        page = html.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("/*APP*/", app_js)
        (site / "smk37" / sub).mkdir(parents=True, exist_ok=True)
        (site / "smk37" / sub / "index.html").write_text(page, encoding="utf-8")
        print(f"smk37/{sub}/: {product} ({name})")
        bak = site / "backup" / "index.html"   # the backup page (make_site.py's, backup = true), for this model's identity
        if bak.exists():
            page_b = bak.read_text(encoding="utf-8")
            fm_id = 'id: "' + conf.get("id_prefix", "") + '"'
            if page_b.count(fm_id) != 1:
                raise SystemExit(f"{bak}: {fm_id!r} not found once")
            prefix = product[:product.index("_") + 2]   # (SMK-37 Pro_4: the model and the first digit, as build.py)
            page_b = smk_words(page_b.replace(fm_id, 'id: "\0ID\0"')).replace("\0ID\0", prefix)
            page_b = page_b.replace("MB1", "MB-1").replace("<title>", "<title>SMK-37 · ", 1)
            sub_b = sub.replace("install", "backup")
            (site / "smk37" / sub_b).mkdir(parents=True, exist_ok=True)
            (site / "smk37" / sub_b / "index.html").write_text(page_b, encoding="utf-8")
            print(f"smk37/{sub_b}/: {prefix}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
