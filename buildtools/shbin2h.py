#!/usr/bin/env python3
import sys

if len(sys.argv) < 4:
    print(f"Usage: {sys.argv[0]} <shbin_file> <var_name> <header_file>")
    sys.exit(1)

shbin_path = sys.argv[1]
var_name = sys.argv[2]
header_path = sys.argv[3]

with open(shbin_path, 'rb') as f:
    data = f.read()

hdr_content = ""
try:
    with open(header_path, 'r') as f:
        hdr_content = f.read()
except FileNotFoundError:
    pass

lines = [
    "#pragma once",
    "#include <3ds/types.h>",
    "",
]

if hdr_content:
    for line in hdr_content.strip().splitlines():
        trimmed = line.strip()
        if trimmed != "#pragma once" and trimmed != "#include <3ds/types.h>":
            lines.append(line)
    lines.append("")

lines.append(f"static const u32 {var_name}_len = {len(data)};")
lines.append("")
lines.append(f"static const u8 {var_name}[] __attribute__((aligned(4))) = {{")

for i in range(0, len(data), 16):
    chunk = data[i:i + 16]
    lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")

lines.append("};")
lines.append("")

with open(header_path, 'w') as f:
    f.write("\n".join(lines))
