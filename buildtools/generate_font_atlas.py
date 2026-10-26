#!/usr/bin/env python3
"""
Atlas TrueType Pre-Rasterizado de Alto Contraste para Nintendo 3DS (GameYob)
Genera 'source/platform/3ds/ui_font_data.h' a partir de una fuente Sans-Serif del sistema.
"""

import os
import sys

try:
    from PIL import Image, ImageFont, ImageDraw
except ImportError:
    print("Error: Pillow (PIL) no está instalado.", file=sys.stderr)
    sys.exit(1)

FONT_CANDIDATES = [
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/google-noto-vf/NotoSans[wght].ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
]

def find_font():
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return path
    raise FileNotFoundError("No se encontró ninguna fuente Sans-Serif compatible del sistema.")

SPECIAL_MAP = {
    0x80: '•', # U+2022 bullet
    0x81: '►', # U+25BA right triangle
    0x82: '◄', # U+25C4 left triangle
    0x83: '▲', # U+25B2 up triangle
    0x84: '▼', # U+25BC down triangle
    0x85: '★', # U+2605 star
    0xA1: '¡', # Inverted exclamation
    0xAB: '«', # Left guillemet
    0xBB: '»', # Right guillemet
    0xBF: '¿', # Inverted question
    0xC1: 'Á', 0xC9: 'É', 0xCD: 'Í', 0xD1: 'Ñ', 0xD3: 'Ó', 0xDA: 'Ú', 0xDC: 'Ü',
    0xE1: 'á', 0xE9: 'é', 0xED: 'í', 0xF1: 'ñ', 0xF3: 'ó', 0xFA: 'ú', 0xFC: 'ü',
}

def main():
    font_path = find_font()
    print(f"[*] Utilizando fuente TrueType: {font_path}")
    font = ImageFont.truetype(font_path, 11)

    glyph_width = [0.0] * 256
    glyph_advance = [0.0] * 256
    atlas = [0] * (256 * 256)

    # 1. Rasterizar los 256 glifos
    for c in range(256):
        if c == 32:
            glyph_width[c] = 0.0
            glyph_advance[c] = 4.0
            continue

        ch = SPECIAL_MAP.get(c)
        if ch is None:
            if (33 <= c <= 126) or (160 <= c <= 255):
                ch = chr(c)

        if ch is None:
            continue

        im = Image.new('L', (32, 32), 0)
        draw = ImageDraw.Draw(im)
        draw.text((8, 11), ch, fill=255, font=font, anchor='ls')

        minX, maxX, minY, maxY = 32, -1, 32, -1
        for y in range(32):
            for x in range(32):
                if im.getpixel((x, y)) > 0:
                    minX = min(minX, x)
                    maxX = max(maxX, x)
                    minY = min(minY, y)
                    maxY = max(maxY, y)

        if maxX < 0:
            continue

        activeW = maxX - minX + 1
        glyph_width[c] = float(activeW + 1)   # Ancho visible real + 1px de sombra
        glyph_advance[c] = float(activeW + 1) # activeWidth + 1px de tracking limpio

        cell_glyph = [[0] * 16 for _ in range(16)]
        for y in range(minY, maxY + 1):
            for x in range(minX, maxX + 1):
                val = im.getpixel((x, y))
                dx = 1 + (x - minX) # Desplazar a X = 1
                dy = y              # Baseline alineado a Y = 11
                if 0 <= dx < 16 and 0 <= dy < 16:
                    cell_glyph[dy][dx] = val

        # Hornear sombra nítida en (px + 1, py + 1) con RGBA(0, 0, 0, 150)
        # SOLO donde píxel destino tenga alpha == 0 y origen tenga final_alpha >= 140
        cell_shadow = [[0] * 16 for _ in range(16)]
        for py in range(16):
            for px in range(16):
                if cell_glyph[py][px] >= 140:
                    spx = px + 1
                    spy = py + 1
                    if spx < 16 and spy < 16:
                        if cell_glyph[spy][spx] == 0:
                            cell_shadow[spy][spx] = 150

        cellCol = c & 0x0F
        cellRow = (c >> 4) & 0x0F
        cellX = cellCol * 16
        cellY = cellRow * 16

        for py in range(16):
            for px in range(16):
                ax = cellX + px
                ay = cellY + py
                g_val = cell_glyph[py][px]
                s_val = cell_shadow[py][px]
                if g_val > 0:
                    # Citro3D GPU_RGBA8: ((alpha << 24) | 0x00FFFFFF)
                    atlas[ay * 256 + ax] = (g_val << 24) | 0x00FFFFFF
                elif s_val > 0:
                    # Citro3D GPU_RGBA8: ((150 << 24) | 0x00000000)
                    atlas[ay * 256 + ax] = (s_val << 24) | 0x00000000

    # 2. VERIFICACIÓN EN TERMINAL
    print("[*] Ejecutando verificaciones de calidad tipográfica...")
    # Huecos internos de 'e', 'a', 'o'
    for ch in ['e', 'a', 'o']:
        c = ord(ch)
        cellCol = c & 0x0F
        cellRow = (c >> 4) & 0x0F
        cellX = cellCol * 16
        cellY = cellRow * 16

        minX, maxX, minY, maxY = 16, -1, 16, -1
        for py in range(16):
            for px in range(16):
                val = (atlas[(cellY + py) * 256 + (cellX + px)] >> 24) & 0xFF
                if val > 0:
                    minX = min(minX, px)
                    maxX = max(maxX, px)
                    minY = min(minY, py)
                    maxY = max(maxY, py)

        zeros = 0
        for py in range(minY + 1, maxY):
            for px in range(minX + 1, maxX):
                val = (atlas[(cellY + py) * 256 + (cellX + px)] >> 24) & 0xFF
                if val == 0:
                    zeros += 1

        assert zeros > 0, f"Error: Hueco interno de '{ch}' cerrado!"
        print(f"  [OK] '{ch}' hueco interno abierto ({zeros} píxeles en 0)")

    # Completitud y simetría de 'M', 'V', 'X', 'T', 'S'
    for ch in ['M', 'V', 'X', 'T', 'S']:
        c = ord(ch)
        w = glyph_width[c]
        assert w >= 6.0, f"Error: Letra '{ch}' demasiado estrecha (w={w})!"
        print(f"  [OK] '{ch}' completo con ancho visible w={w}")

    # 3. Exportar source/platform/3ds/ui_font_data.h
    output_path = os.path.join(os.path.dirname(__file__), "..", "source", "platform", "3ds", "ui_font_data.h")
    output_path = os.path.abspath(output_path)
    print(f"[*] Exportando datos a: {output_path}")

    with open(output_path, "w") as f:
        f.write("/*\n")
        f.write(" * Atlas TrueType Antialiased de Alto Contraste para Nintendo 3DS\n")
        f.write(" * Archivo auto-generado por buildtools/generate_font_atlas.py. NO EDITAR DIRECTAMENTE.\n")
        f.write(" */\n\n")
        f.write("#ifndef UI_FONT_DATA_H\n")
        f.write("#define UI_FONT_DATA_H\n\n")
        f.write("#include <stdint.h>\n\n")

        # Anchos de glifos
        f.write("static const float s_ttfGlyphWidth[256] = {\n")
        for i in range(256):
            f.write(f"    {glyph_width[i]:.1f}f,")
            if (i + 1) % 8 == 0:
                f.write(f" // 0x{i-7:02X}..0x{i:02X}\n")
        f.write("};\n\n")

        # Avances de glifos
        f.write("static const float s_ttfGlyphAdvance[256] = {\n")
        for i in range(256):
            f.write(f"    {glyph_advance[i]:.1f}f,")
            if (i + 1) % 8 == 0:
                f.write(f" // 0x{i-7:02X}..0x{i:02X}\n")
        f.write("};\n\n")

        # Pixeles del Atlas 256x256 (65536 u32 RGBA8 Citro3D)
        f.write("static const uint32_t s_fontAtlasPixels[65536] = {\n")
        for i in range(65536):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{atlas[i]:08X},")
            if (i + 1) % 16 == 0:
                f.write("\n")
        f.write("};\n\n")

        f.write("#endif // UI_FONT_DATA_H\n")

    print("[*] Generación de atlas completada con éxito.")

if __name__ == "__main__":
    main()
