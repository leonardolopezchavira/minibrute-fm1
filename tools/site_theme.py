#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""MB-1's look on its web pages, after the Brute line's panels and the MiniBrute patch sheets: grey panels, white
sections with tabbed titles, black ink, condensed capitals (Barlow Condensed, SIL OFL 1.1: app/site/fonts), one
orange. Light by default, or dark (the sheet inverted): a button in the menu switches every page, and the choice is
kept in the browser (localStorage "mb1-theme", the editor's too). Laid over the pages fm1-base's tools/make_site.py
makes (the installer, the manual, the emulator) and the front page and editor (tools/make_home.py,
tools/build_editor.sh), with a menu across the top of each.

    python3 tools/site_theme.py SITE_DIR      every .html page under it, themed in place
"""
import re
import sys
from pathlib import Path

BASE = "/minibrute-fm1/"
NAV = [("", "Home"), ("install/", "Install"), ("edit/", "Patch editor"), ("manual/", "Manual"), ("emu/", "Emulator")]
ORANGE = "#f06028"

# the theme before the first paint: the one picked before, else the system's
INIT_JS = """<script id="mb1-theme-init">
try { var t = localStorage.getItem("mb1-theme"); document.documentElement.dataset.theme = t || (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light"); } catch (e) { document.documentElement.dataset.theme = "light"; }
</script>
"""

FONTS = """<style id="mb1-fonts">
  @font-face { font-family: "Barlow Condensed"; font-weight: 500; src: url("{base}fonts/barlow-condensed-latin-500-normal.woff2") format("woff2"); font-display: swap; }
  @font-face { font-family: "Barlow Condensed"; font-weight: 600; src: url("{base}fonts/barlow-condensed-latin-600-normal.woff2") format("woff2"); font-display: swap; }
  @font-face { font-family: "Barlow Condensed"; font-weight: 700; src: url("{base}fonts/barlow-condensed-latin-700-normal.woff2") format("woff2"); font-display: swap; }
</style>
"""

# the colours, light and dark, through every page's own variables (the installer's, the emulator's, the manual's)
PALETTE = f"""<style id="mb1-theme">
  html:root:root, html:root:root[data-theme="light"] {{
    --mb-page: #e9e9e7; --mb-body: #c4c4c4; --mb-face: #ffffff; --mb-ink: #000000; --mb-mute: #555555; --mb-hi: {ORANGE};
    --mb-hi-ink: #b23a0c; --mb-line: #b5b5b5;
    --bg: #e9e9e7; --fg: #000; --dim: #555; --accent: #000; --on-accent: #fff; --bad: #c42b1c; --focus: {ORANGE};
    --ground: #e9e9e7; --ink: #000; --muted: #555; --rule: #c4c4c4; --teal: #b23a0c; --card: #fff;
    --paper: #e9e9e7; --sheet: #ffffff; --hot: {ORANGE}; --hot-t: #fbd9c9; --hot-ink: #b23a0c; --cool: #000; --cool-t: #ffffff;
    color-scheme: light; }}
  html:root:root[data-theme="dark"] {{
    --mb-page: #0b0b0b; --mb-body: #3b3b3b; --mb-face: #000000; --mb-ink: #ffffff; --mb-mute: #a6a6a6; --mb-hi: {ORANGE};
    --mb-hi-ink: #ff8a57; --mb-line: #3b3b3b;
    --bg: #0b0b0b; --fg: #fff; --dim: #a6a6a6; --accent: #fff; --on-accent: #000; --bad: #ff8a73; --focus: {ORANGE};
    --ground: #0b0b0b; --ink: #fff; --muted: #a6a6a6; --rule: #3b3b3b; --teal: #ff8a57; --card: #000;
    --paper: #0b0b0b; --sheet: #161616; --hot: {ORANGE}; --hot-t: #3a1a0c; --hot-ink: #ff8a57; --cool: #fff; --cool-t: #000;
    color-scheme: dark; }}
  html body {{ background: var(--mb-page); color: var(--mb-ink); }}
  /* the type: condensed capitals for what labels a thing, as a panel's are printed */
  html body h1, html body h2, html body h3, html body th, html body button, html body .btn, html body summary h2 {{
    font-family: "Barlow Condensed", "Arial Narrow", sans-serif; }}
  html body h1 {{ text-transform: uppercase; letter-spacing: .04em; font-weight: 700; }}
  html body h2, html body h3 {{ text-transform: uppercase; letter-spacing: .1em; font-weight: 700; }}
  html body button:not(.mb1-theme-btn) {{ border-radius: 3px; font-weight: 600; letter-spacing: .08em; text-transform: uppercase;
    background: var(--mb-ink); color: var(--mb-face); border: 2px solid var(--mb-ink); }}
  html body button:not(.mb1-theme-btn):hover:not(:disabled) {{ background: {ORANGE}; border-color: {ORANGE}; color: #000; }}
  html body button.plain {{ background: transparent; color: var(--mb-ink); border: 2px solid var(--mb-ink); }}
  html body button:disabled {{ opacity: .4; }}
  html body :focus-visible {{ outline: 3px solid {ORANGE}; outline-offset: 2px; }}
  html body a {{ color: var(--mb-ink); text-decoration-color: {ORANGE}; text-decoration-thickness: 2px; text-underline-offset: 3px; }}
  html body progress {{ accent-color: {ORANGE}; }}
  html body dialog {{ background: var(--mb-face); color: var(--mb-ink); border: 2px solid var(--mb-ink); border-radius: 8px; }}
  html body .backup {{ border-color: {ORANGE}; border-radius: 8px; }}
  html body .backup strong {{ color: var(--mb-hi-ink); }}
  html body code, html body pre {{ background: var(--mb-face); }}
</style>
"""

# the installer: its card a white section on a grey panel, the title in a tab
INSTALL_CSS = """<style id="mb1-install">
  html body main { background: var(--mb-body); border-radius: 14px; padding: 34px 22px 22px; margin-top: 18px; }
  html body main > * { position: relative; }
  html body main section, html body main > form, html body main > div:not(.mb1-tab) { background: var(--mb-face); border-radius: 10px; }
  html body main h1 { font-size: 30px; }
</style>
"""

# the manual (fm1-base's manual_html.py): the page as a sheet, the headings as a panel's section titles
PAPER_CSS = """<style id="mb1-paper">
  html body header.site { display: none; }   /* (the menu at the top has these links) */
  html body main { background: var(--mb-face); border-radius: 14px; padding: 28px 32px 40px; box-shadow: 0 0 0 14px var(--mb-body); margin-top: 32px; margin-bottom: 32px; }
  html body h1 { font-size: 2.6rem; }
  html body h2 { border-top: 0; display: inline-block; background: var(--mb-ink); color: var(--mb-face); padding: .2rem .8rem;
    border-radius: 6px; font-size: 1.05rem; margin-top: 2.6rem; }
  html body h2 .num { color: #f06028; }
  html body th { border-bottom-color: var(--mb-ink); color: var(--mb-mute); }
  html body li::marker { color: #f06028; }
  html body figure.shot img { box-shadow: 0 0 0 6px var(--mb-body); border-radius: 4px; }
  html body figcaption { color: var(--mb-mute); }
</style>
"""

# the emulator (fm1-base's web/emu): its own variables carry the colours; its accents orange
EMU_CSS = """<style id="mb1-emu">
  html body .start button { background: #f06028; color: #000; border-color: #f06028; }
  html body .bar button[aria-pressed="true"] { background: #f06028; color: #000; border-color: #f06028; }
  html body .speak { border-radius: 10px; background: var(--mb-face); }
  html body .held .btnface, html body .held .keyface { stroke: #f06028; }
  html body kbd { background: var(--mb-face); color: var(--mb-ink); }
  html body > nav.mb1-nav { margin: -28px -16px 22px; }   /* (the page's body has padding: the menu across it) */
</style>
"""

# the menu: a black strip, the wordmark, the pages, the light/dark switch
NAV_CSS = """<style id="mb1-nav">
  nav.mb1-nav { display: flex; flex-wrap: wrap; gap: 4px 2px; align-items: center; padding: 8px 18px; background: #000; color: #fff;
    font: 600 15px/1.2 "Barlow Condensed", "Arial Narrow", sans-serif; letter-spacing: .12em; text-transform: uppercase;
    position: relative; z-index: 10; border-bottom: 3px solid #f06028; }
  nav.mb1-nav a { color: #fff; text-decoration: none; padding: 6px 10px; border-radius: 3px; }
  nav.mb1-nav a:hover { color: #f06028; }
  nav.mb1-nav a[aria-current="page"] { color: #000; background: #f06028; }
  nav.mb1-nav a.mb1-mark { font: 700 26px/1 "Barlow Condensed", "Arial Narrow", sans-serif; letter-spacing: .02em; padding: 2px 14px 2px 0;
    margin-right: 8px; border-right: 2px solid #3b3b3b; border-radius: 0; background: none; color: #fff; }
  nav.mb1-nav .sp { flex: 1; }
  nav.mb1-nav button.mb1-theme-btn { font: 600 13px/1 "Barlow Condensed", sans-serif; letter-spacing: .12em; text-transform: uppercase;
    background: transparent; color: #fff; border: 2px solid #fff; border-radius: 3px; padding: 6px 10px; cursor: pointer; }
  nav.mb1-nav button.mb1-theme-btn:hover { background: #fff; color: #000; }
  html body > nav.mb1-nav { margin-left: 0; margin-right: 0; }
</style>
"""

NAV_JS = """<script id="mb1-nav-js">
(() => {
  const b = document.querySelector("button.mb1-theme-btn"); if (!b) return;
  const label = () => { b.textContent = document.documentElement.dataset.theme === "dark" ? "Light" : "Dark"; b.setAttribute("aria-label", "Switch to the " + b.textContent.toLowerCase() + " look"); };
  b.addEventListener("click", () => {
    const t = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
    document.documentElement.dataset.theme = t; try { localStorage.setItem("mb1-theme", t); } catch (e) { /* */ }
    label(); window.dispatchEvent(new Event("mb1-theme"));
  });
  window.addEventListener("mb1-theme", label); label();
})();
</script>
"""


def nav(current):
    items = "".join(f'<a href="{BASE}{href}"' + (' aria-current="page"' if href == current else "") + f">{label}</a>"
                    for href, label in NAV)
    return (f'<nav class="mb1-nav" aria-label="MB-1 pages"><a class="mb1-mark" href="{BASE}" aria-label="MB-1 home">MB-1</a>'
            f'{items}<span class="sp"></span><button type="button" class="mb1-theme-btn">Dark</button></nav>')


def theme(html, extra="", current=""):
    if 'id="mb1-theme"' in html:
        return html
    if extra == "editor":   # (the editor and the front page have the look already: the menu only)
        head = NAV_CSS + '<style id="mb1-theme">html body .grp:has(> #theme) { display: none; }</style>\n'
    else:
        head = INIT_JS + FONTS.replace("{base}", BASE) + PALETTE + NAV_CSS + extra
    html = html.replace("</head>", head + "</head>", 1)
    html = re.sub(r"(<body[^>]*>)", lambda k: k.group(1) + nav(current), html, count=1)
    return html.replace("</body>", NAV_JS + "</body>", 1)


def main():
    root = Path(sys.argv[1])
    fonts = Path(__file__).resolve().parents[1] / "app" / "site" / "fonts"
    (root / "fonts").mkdir(exist_ok=True)
    for f in fonts.iterdir():
        (root / "fonts" / f.name).write_bytes(f.read_bytes())
    for p in sorted(root.rglob("*.html")):
        rel = p.relative_to(root).parts
        if rel[0] in ("words", "firmware") or (rel[0] == "emu" and rel[-1] != "index.html"):
            continue
        sect = rel[0] if len(rel) > 1 else ""
        extra = {"install": INSTALL_CSS, "manual": PAPER_CSS, "emu": EMU_CSS, "edit": "editor", "": "editor"}.get(sect, "")
        p.write_text(theme(p.read_text(encoding="utf-8"), extra, sect + "/" if sect else ""), encoding="utf-8")
        print("themed", p.relative_to(root))


if __name__ == "__main__":
    main()
