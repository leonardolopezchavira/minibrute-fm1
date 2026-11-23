#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""MB-1's factory bank (app/factory/MB-1_bank.syx: SysEx frames 01, as the patch editor saves a bank) as
app/factory_bank.h: bank A's patches, each a name and its 76 values; a slot past them, and banks B .. D, start as INIT.
patch_factory (app/mbpatch.h) reads it.

    python3 tools/make_factory.py
"""
from pathlib import Path

NP = 76
ROOT = Path(__file__).resolve().parents[1]
raw = (ROOT / "app" / "factory" / "MB-1_bank.syx").read_bytes()
slots = {}
for part in raw.split(b"\xf7"):
    i = part.find(b"\xf0")
    if i < 0:
        continue
    f = part[i:]
    if f[1:4] == b"\x7dMB" and f[4] == 1 and len(f) == 14 + NP:
        slots[f[5]] = (f[6:14].decode("ascii", "replace").rstrip(), list(f[14:14 + NP]))
named = [s for s in sorted(slots) if s < 32 and slots[s][0] != "INIT"]
n = max(named) + 1 if named else 0
out = ["/* SPDX-License-Identifier: MIT",
       " * MB-1's factory bank: made by tools/make_factory.py from app/factory/MB-1_bank.syx (do not edit). Bank A's",
       " * first slots; the rest, and banks B .. D, start as INIT */",
       "static const struct {", "    char name[9];", "    uint8_t v[76];", "} FACTORY[] = {"]
for s in range(n):
    name, v = slots.get(s, ("INIT", None))
    if v is None:
        raise SystemExit(f"slot {s + 1}: missing")
    out.append(f'    {{"{name}", {{' + ", ".join(str(x) for x in v) + "}},")
out.append("};")
(ROOT / "app" / "factory_bank.h").write_text("\n".join(out) + "\n")
print(f"factory  app/factory_bank.h: {n} patches")
