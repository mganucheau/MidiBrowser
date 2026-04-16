"""
Mockup 4: Nordic Clean
Design principles: Scandinavian-inspired minimalism, high contrast with soft elements,
lots of whitespace on dark, clear grid alignment, monochromatic with single accent,
strong typography hierarchy, icon-reduced interface.
"""
from PIL import Image, ImageDraw, ImageFont
import random

W, H = 1280, 720
img = Image.new('RGB', (W, H), '#111115')
d = ImageDraw.Draw(img)

try:
    font_bold_20 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 20)
    font_bold_14 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 14)
    font_bold_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 12)
    font_bold_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 11)
    font_bold_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 10)
    font_bold_9 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 9)
    font_reg_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
    font_reg_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 11)
    font_reg_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 10)
    font_reg_9 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 9)
    font_light_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-ExtraLight.ttf', 10)
except:
    font_bold_20 = font_bold_14 = font_bold_12 = font_bold_11 = font_bold_10 = font_bold_9 = ImageFont.load_default()
    font_reg_12 = font_reg_11 = font_reg_10 = font_reg_9 = font_light_10 = ImageFont.load_default()

# Nordic palette - Monochromatic with ice blue accent
bg = '#111115'
surface1 = '#18181d'
surface2 = '#1f1f25'
surface3 = '#26262e'
surface4 = '#2e2e38'
border = '#2a2a34'
border_subtle = '#222228'
text_primary = '#f0f0f5'
text_secondary = '#8888a0'
text_muted = '#505068'
text_dim = '#3a3a50'
accent = '#88c0d0'          # Nordic ice blue
accent_dim = '#88c0d020'
accent_hover = '#a3d4e0'
warm = '#d08770'            # Warm highlight (sparingly)
green_muted = '#a3be8c'
red_muted = '#bf616a'
yellow_muted = '#ebcb8b'

def rounded_rect(draw, xy, fill, radius=6, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)

# ─── LAYOUT: Two toolbar rows merged into one clean row + tabbed sections ───

# ─── TOP BAR - Ultra clean, single row ───
top_h = 44

# Subtle top bar
d.rectangle((0, 0, W, top_h), fill=surface1)

# Logo - clean typography
d.text((20, 11), "PATTERNFLOW", fill=text_primary, font=font_bold_14)

# Version dot
d.text((158, 11), "1.0", fill=text_dim, font=font_reg_9)

# Ultra thin separator
d.line([(188, 10), (188, top_h - 10)], fill=border, width=1)

# ─── Transport: minimal, aligned ───
tx = 200

# Record - minimal dot
d.ellipse((tx, 16, tx+14, 30), fill=red_muted)

# Divider dot
d.text((tx+22, 16), "\u00b7", fill=text_dim, font=font_reg_12)

# Grid
d.text((tx+34, 12), "GRID", fill=text_dim, font=font_bold_9)
d.text((tx+34, 24), "Beat", fill=text_secondary, font=font_reg_10)

d.text((tx+78, 16), "\u00b7", fill=text_dim, font=font_reg_12)

# Loop
d.text((tx+90, 12), "LOOP", fill=text_dim, font=font_bold_9)
rounded_rect(d, (tx+90, 22, tx+120, 36), accent_dim, radius=4)
d.text((tx+96, 24), "ON", fill=accent, font=font_bold_10)

d.text((tx+130, 16), "\u00b7", fill=text_dim, font=font_reg_12)

# Bars
d.text((tx+142, 12), "BARS", fill=text_dim, font=font_bold_9)
d.text((tx+142, 24), "4", fill=text_primary, font=font_bold_11)

d.text((tx+166, 16), "\u00b7", fill=text_dim, font=font_reg_12)

# Arrange tools - text only, minimal
for i, label in enumerate(["Step", "Extend", "Trim"]):
    bx = tx + 180 + i * 52
    d.text((bx, 16), label, fill=text_secondary, font=font_reg_10)

# Separator
d.line([(tx+336, 10), (tx+336, top_h-10)], fill=border, width=1)

# ─── Scale section ───
sx = tx + 350
d.text((sx, 12), "SCALE", fill=text_dim, font=font_bold_9)

# Scale active indicator
rounded_rect(d, (sx, 22, sx+26, 36), accent_dim, radius=4)
d.text((sx+4, 24), "C#", fill=accent, font=font_bold_10)

d.text((sx+34, 24), "Harmonic Minor", fill=text_secondary, font=font_reg_10)

d.text((sx+152, 16), "\u00b7", fill=text_dim, font=font_reg_12)

# Transpose
d.text((sx+164, 24), "Transpose", fill=text_secondary, font=font_reg_10)
d.text((sx+232, 24), "C0", fill=text_muted, font=font_reg_10)

# Separator
d.line([(sx+260, 10), (sx+260, top_h-10)], fill=border, width=1)

# ─── Comp section ───
comp_sx = sx + 274
d.text((comp_sx, 12), "COMP", fill=text_dim, font=font_bold_9)
d.text((comp_sx, 24), "6", fill=text_primary, font=font_bold_11)

d.text((comp_sx+24, 16), "\u00b7", fill=text_dim, font=font_reg_12)

d.text((comp_sx+36, 24), "Random", fill=text_secondary, font=font_reg_10)
d.text((comp_sx+96, 24), "Swap", fill=text_secondary, font=font_reg_10)

# Settings - minimal
d.text((W-30, 16), "\u2699", fill=text_muted, font=font_reg_12)

# Top bar bottom line - single pixel
d.line([(0, top_h), (W, top_h)], fill=border, width=1)

# ─── BROWSER - Cleaner, more space ───
browser_w = 190
browser_top = top_h + 1

d.rectangle((0, browser_top, browser_w, H), fill=surface1)
d.line([(browser_w, browser_top), (browser_w, H)], fill=border, width=1)

# Section label
d.text((16, browser_top + 14), "BROWSER", fill=text_dim, font=font_bold_9)

# Search/filter field
rounded_rect(d, (12, browser_top + 32, browser_w - 12, browser_top + 50), surface3, radius=6)
d.text((20, browser_top + 36), "Select folder...", fill=text_dim, font=font_reg_10)

# File list - clean, more spacing
files = [
    "1. Bass 124 G",
    "10. Bass 124 C#",
    "11. Bass 124 A",
    "12. Bass 124 F",
    "13. Bass 124 C#",
    "14. Bass 124 A",
    "15. Bass 124 D",
    "16. Bass 124 Cm",
    "17. Bass 124 C",
    "18. Bass 124 C",
    "19. Bass 124 G",
    "20. Bass 124 A#",
    "21. Bass 124 C",
    "22. Bass 124 C#",
    "23. Bass 124 G",
    "24. Bass 124 A",
    "25. Bass 174 A#",
]

for i, f in enumerate(files):
    fy = browser_top + 60 + i * 24
    if fy > H - 40:
        break
    if i == 9:
        d.rectangle((8, fy - 2, browser_w - 8, fy + 20), fill=accent_dim)
        d.rectangle((8, fy - 2, 10, fy + 20), fill=accent)
        d.text((18, fy + 2), f, fill=accent, font=font_reg_10)
    else:
        d.text((18, fy + 2), f, fill=text_secondary, font=font_reg_10)

# ─── ARRANGEMENT VIEW ───
arr_x = browser_w + 1
arr_top = top_h + 1
header_w = 90
timeline_x = arr_x + header_w
timeline_w = W - timeline_x

# Timeline ruler - minimal
ruler_h = 24
d.rectangle((arr_x, arr_top, W, arr_top + ruler_h), fill=surface1)

total_bars = 5
bar_w = timeline_w / total_bars
for bar in range(total_bars):
    bx = timeline_x + bar * bar_w
    d.text((bx + 4, arr_top + 6), str(bar + 1), fill=text_dim, font=font_bold_10)
    d.line([(bx, arr_top + ruler_h), (bx, H)], fill=border_subtle, width=1)
    for beat in range(1, 4):
        sbx = bx + beat * (bar_w / 4)
        d.line([(sbx, arr_top + ruler_h), (sbx, H)], fill='#1a1a20', width=1)

d.line([(arr_x, arr_top + ruler_h), (W, arr_top + ruler_h)], fill=border, width=1)

# ─── COMP LANE - thin, minimal ───
comp_top = arr_top + ruler_h + 1
comp_h = 30

d.rectangle((arr_x, comp_top, arr_x + header_w, comp_top + comp_h), fill=surface2)
d.text((arr_x + 10, comp_top + 9), "COMP", fill=text_dim, font=font_bold_9)

# Comp segments - monochromatic, using opacity
comp_segs = [
    (0.0, 0.8, 0), (0.8, 1.6, 1), (1.6, 2.4, 2),
    (2.4, 3.2, 0), (3.2, 4.0, 3), (4.0, 5.0, 1),
]
comp_fills = [accent+'15', accent+'25', accent+'10', accent+'20']
for start, end, ci in comp_segs:
    cx1 = timeline_x + int(start * bar_w)
    cx2 = timeline_x + int(end * bar_w)
    rounded_rect(d, (cx1+1, comp_top+3, cx2-1, comp_top+comp_h-3), comp_fills[ci], radius=3)

d.line([(arr_x, comp_top + comp_h), (W, comp_top + comp_h)], fill=border, width=1)

# ─── LANES - Clean, consistent ───
lane_h = 102
lane_gap = 1
lane_names = ["Lane 1", "Lane 2", "Lane 3", "Lane 4", "Lane 5"]
# Monochromatic palette - all variations of the ice blue accent
lane_intensities = [
    (136, 192, 208),  # Base ice blue
    (163, 190, 140),  # Sage green
    (180, 142, 173),  # Muted purple
    (208, 135, 112),  # Warm orange
    (235, 203, 139),  # Warm yellow
]

lane_clips = [
    [(0.0, 1.8, "Bass 124 G"), (2.5, 4.2, "Bass 124 C#")],
    [(0.5, 2.0, "Bass 124 C#"), (3.0, 4.5, "Bass 124 A")],
    [(0.0, 1.5, "Bass 124 D"), (1.8, 3.5, "Bass 124 F")],
    [(0.5, 2.5, "Bass 124 Cm"), (3.2, 5.0, "Bass 124 G")],
    [(1.0, 3.0, "Bass 124 bpm")],
]

for li, (name, clips) in enumerate(zip(lane_names, lane_clips)):
    ly = comp_top + comp_h + lane_gap + li * (lane_h + lane_gap)
    if ly + lane_h > H - 28:
        break

    r, g, b = lane_intensities[li]
    lc_hex = f'#{r:02x}{g:02x}{b:02x}'

    # Lane header
    d.rectangle((arr_x, ly, arr_x + header_w, ly + lane_h), fill=surface2)

    # Thin color indicator line (left edge)
    d.rectangle((arr_x, ly, arr_x + 3, ly + lane_h), fill=lc_hex)

    # Lane name - clean
    d.text((arr_x + 12, ly + 8), name, fill=text_primary, font=font_bold_11)

    # M/S - extremely minimal
    d.text((arr_x + 12, ly + 30), "M", fill=text_dim, font=font_bold_10)
    d.text((arr_x + 28, ly + 30), "S", fill=text_dim, font=font_bold_10)

    # Color swatch
    rounded_rect(d, (arr_x + 52, ly + 28, arr_x + 68, ly + 42), lc_hex+'40', radius=4, outline=lc_hex+'80')

    # Lane bg
    d.rectangle((timeline_x, ly, W, ly + lane_h), fill=bg)

    # Clips - clean rectangles with subtle style
    for start, end, label in clips:
        cx1 = timeline_x + int(start * bar_w) + 2
        cx2 = timeline_x + int(end * bar_w) - 2

        # Clip body - subtle fill
        rounded_rect(d, (cx1, ly+6, cx2, ly+lane_h-6), f'#{r:02x}{g:02x}{b:02x}10', radius=6, outline=f'#{r:02x}{g:02x}{b:02x}30')

        # Top accent bar
        d.rectangle((cx1+1, ly+6, cx2-1, ly+10), fill=f'#{r:02x}{g:02x}{b:02x}40')
        rounded_rect(d, (cx1, ly+6, cx2, ly+10), f'#{r:02x}{g:02x}{b:02x}40', radius=6)

        # Label
        d.text((cx1 + 8, ly + 14), label, fill=f'#{r:02x}{g:02x}{b:02x}cc', font=font_reg_9)

        # MIDI note preview
        random.seed(hash(label) + li)
        for n in range(15):
            ny = ly + 28 + n * 4
            if ny > ly + lane_h - 10:
                break
            nx = cx1 + 6 + random.randint(0, 30)
            nw = random.randint(8, 40)
            if nx + nw > cx2 - 6:
                nw = cx2 - 6 - nx
            if nw > 4:
                d.rectangle((nx, ny, nx+nw, ny+2), fill=f'#{r:02x}{g:02x}{b:02x}35')

    # Lane divider
    d.line([(arr_x, ly + lane_h), (W, ly + lane_h)], fill=border_subtle, width=1)

# Playhead - clean red line
ph_x = timeline_x + int(1.5 * bar_w)
d.line([(ph_x, arr_top + ruler_h), (ph_x, H - 28)], fill=red_muted, width=2)
# Minimal playhead marker
rounded_rect(d, (ph_x-4, arr_top+ruler_h-4, ph_x+4, arr_top+ruler_h+4), red_muted, radius=2)

# ─── BOTTOM STATUS ───
d.rectangle((0, H - 24, W, H), fill=surface1)
d.line([(0, H-24), (W, H-24)], fill=border, width=1)
d.text((16, H - 18), "CONCEPT 4: NORDIC CLEAN", fill=text_muted, font=font_bold_12)
d.text((230, H - 18), "Scandinavian minimalism  |  Monochromatic + ice blue accent  |  Label-above-value pattern  |  Maximum breathing room  |  Typography-led hierarchy", fill=text_dim, font=font_reg_10)

img.save('/home/user/PatternFlow/mockup4_nordic_clean.png', 'PNG')
print("Mockup 4 saved")
