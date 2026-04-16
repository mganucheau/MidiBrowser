"""
Mockup 1: Minimal Modern
Design principles: Generous whitespace, unified muted palette, clear visual hierarchy,
grouped controls in pill containers, consistent spacing rhythm.
"""
from PIL import Image, ImageDraw, ImageFont
import colorsys

W, H = 1280, 720
img = Image.new('RGB', (W, H), '#0d1117')
d = ImageDraw.Draw(img)

# Fonts
try:
    font_bold_18 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
    font_bold_14 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 14)
    font_bold_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 12)
    font_bold_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 11)
    font_reg_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
    font_reg_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 11)
    font_reg_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 10)
    font_reg_9 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 9)
except:
    font_bold_18 = font_bold_14 = font_bold_12 = font_bold_11 = ImageFont.load_default()
    font_reg_12 = font_reg_11 = font_reg_10 = font_reg_9 = ImageFont.load_default()

# Colors - unified cool palette
bg = '#0d1117'
surface1 = '#161b22'
surface2 = '#1c2128'
surface3 = '#21262d'
border = '#30363d'
border_light = '#484f58'
text_primary = '#e6edf3'
text_secondary = '#8b949e'
text_muted = '#656d76'
accent = '#58a6ff'
accent_dim = '#1f3a5f'
accent_hover = '#79c0ff'
green = '#3fb950'
yellow = '#d29922'
red = '#f85149'
clip_colors = ['#3a7ca5', '#4a9e6e', '#8b6fc0', '#c4724e', '#5e8b9e']

def rounded_rect(draw, xy, fill, radius=8, outline=None):
    x1, y1, x2, y2 = xy
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline)

# ─── UNIFIED TOP BAR (single row, 48px) ───
top_h = 48
rounded_rect(d, (0, 0, W, top_h), surface1)
d.line([(0, top_h), (W, top_h)], fill=border, width=1)

# Logo
d.text((20, 14), "PatternFlow", fill=accent, font=font_bold_18)

# Transport group - pill container
tx = 180
rounded_rect(d, (tx, 8, tx+320, 40), surface2, radius=16, outline=border)
# Record dot
d.ellipse((tx+12, 17, tx+26, 31), fill='#3a1f1f', outline=red)
d.ellipse((tx+15, 20, tx+23, 28), fill=red)
# Grid snap
d.text((tx+36, 16), "Beat", fill=text_primary, font=font_reg_11)
d.text((tx+68, 16), "|", fill=text_muted, font=font_reg_11)
# Loop toggle
rounded_rect(d, (tx+80, 13, tx+120, 35), accent_dim, radius=10)
d.text((tx+88, 16), "Loop", fill=accent, font=font_bold_11)
d.text((tx+130, 16), "|", fill=text_muted, font=font_reg_11)
# Bars
d.text((tx+142, 16), "Bars", fill=text_secondary, font=font_reg_11)
d.text((tx+172, 16), "4", fill=text_primary, font=font_bold_11)
d.text((tx+190, 16), "|", fill=text_muted, font=font_reg_11)
# Step / Extend / Trim
for i, label in enumerate(["Step", "Extend", "Trim"]):
    bx = tx + 202 + i * 42
    d.text((bx, 16), label, fill=text_secondary, font=font_reg_11)

# Scale group - pill container
sx = 530
rounded_rect(d, (sx, 8, sx+340, 40), surface2, radius=16, outline=border)
rounded_rect(d, (sx+8, 13, sx+56, 35), accent_dim, radius=10)
d.text((sx+15, 16), "Scale", fill=accent, font=font_bold_11)
d.text((sx+66, 16), "C#", fill=text_primary, font=font_bold_11)
d.text((sx+90, 16), "|", fill=text_muted, font=font_reg_11)
d.text((sx+102, 16), "Harmonic Minor", fill=text_primary, font=font_reg_11)
d.text((sx+218, 16), "|", fill=text_muted, font=font_reg_11)
rounded_rect(d, (sx+230, 13, sx+290, 35), surface3, radius=10)
d.text((sx+237, 16), "Transp.", fill=text_secondary, font=font_reg_11)
d.text((sx+300, 16), "C0", fill=text_secondary, font=font_reg_11)

# Comp group
cx = 900
rounded_rect(d, (cx, 8, cx+240, 40), surface2, radius=16, outline=border)
rounded_rect(d, (cx+8, 13, cx+58, 35), surface3, radius=10)
d.text((cx+14, 16), "Comp", fill=text_secondary, font=font_reg_11)
d.text((cx+68, 16), "6", fill=text_primary, font=font_bold_11)
d.text((cx+86, 16), "|", fill=text_muted, font=font_reg_11)
d.text((cx+98, 16), "Random", fill=text_secondary, font=font_reg_11)
d.text((cx+158, 16), "|", fill=text_muted, font=font_reg_11)
d.text((cx+172, 16), "Swap", fill=text_secondary, font=font_reg_11)

# Settings gear
d.text((W-40, 16), "\u2699", fill=text_secondary, font=font_bold_14)

# ─── BROWSER PANEL ───
browser_w = 200
browser_x = 0
browser_top = top_h
rounded_rect(d, (browser_x, browser_top, browser_w, H), surface1)
d.line([(browser_w, browser_top), (browser_w, H)], fill=border, width=1)

# Browser header
d.text((16, browser_top + 12), "BROWSER", fill=text_muted, font=font_bold_11)

# Folder selector
rounded_rect(d, (12, browser_top + 32, browser_w - 12, browser_top + 54), surface2, radius=8, outline=border)
d.text((20, browser_top + 37), "Select a folder", fill=text_secondary, font=font_reg_10)

# File list
files = [
    "1. Bass 124 bpm G.mid",
    "10. Bass 124 bpm C#.mid",
    "11. Bass 124 bpm A.mid",
    "12. Bass 124 bpm F.mid",
    "13. Bass 124 bpm C#.mid",
    "14. Bass 124 bpm A.mid",
    "15. Bass 124 bpm D.mid",
    "16. Bass 124 bpm Cm.mid",
    "17. Bass 124 bpm C.mid",
    "18. Bass 124 bpm C.mid",
    "19. Bass 124 bpm G.mid",
    "20. Bass 124 bpm A#.mid",
    "21. Bass 124 bpm C.mid",
    "22. Bass 124 bpm C#.mid",
    "23. Bass 124 bpm G.mid",
    "24. Bass 124 bpm A.mid",
]

for i, f in enumerate(files):
    fy = browser_top + 64 + i * 22
    if fy > H - 30:
        break
    if i == 9:  # selected
        rounded_rect(d, (8, fy-2, browser_w-8, fy+18), accent_dim, radius=6)
        d.text((16, fy), f, fill=accent, font=font_reg_10)
    else:
        d.text((16, fy), f, fill=text_secondary, font=font_reg_10)

# ─── ARRANGEMENT VIEW ───
arr_x = browser_w + 1
arr_top = top_h
header_w = 100
timeline_x = arr_x + header_w
timeline_w = W - timeline_x

# Timeline ruler
ruler_h = 28
rounded_rect(d, (arr_x, arr_top, W, arr_top + ruler_h), surface2)
d.line([(arr_x, arr_top + ruler_h), (W, arr_top + ruler_h)], fill=border, width=1)

# Beat numbers
beats_per_bar = 4
total_bars = 5
bar_w = timeline_w / total_bars
for bar in range(total_bars):
    bx = timeline_x + bar * bar_w
    d.text((bx + 4, arr_top + 8), str(bar), fill=text_muted, font=font_reg_10)
    d.line([(bx, arr_top), (bx, H)], fill=border, width=1)
    # Sub-beats
    for beat in range(1, 4):
        sbx = bx + beat * (bar_w / 4)
        d.line([(sbx, arr_top), (sbx, H)], fill='#1a1f27', width=1)

# ─── COMP LANE (combined at top) ───
lane_top = arr_top + ruler_h
comp_h = 44

# Comp header
rounded_rect(d, (arr_x, lane_top, arr_x + header_w, lane_top + comp_h), surface2)
d.text((arr_x + 12, lane_top + 14), "COMP", fill=text_muted, font=font_bold_11)

# Comp content - subtle gradient blocks
comp_segments = [
    (0.0, 0.8, '#1a3a5a'),
    (0.8, 1.6, '#1a4a3a'),
    (1.6, 2.4, '#3a2a4a'),
    (2.4, 3.2, '#1a3a5a'),
    (3.2, 4.0, '#2a3a2a'),
    (4.0, 5.0, '#1a4a3a'),
]
for start, end, color in comp_segments:
    cx1 = timeline_x + int(start * bar_w)
    cx2 = timeline_x + int(end * bar_w)
    rounded_rect(d, (cx1+1, lane_top+4, cx2-1, lane_top+comp_h-4), color, radius=4)

d.line([(arr_x, lane_top + comp_h), (W, lane_top + comp_h)], fill=border, width=1)

# ─── LANES ───
lane_h = 88
lane_names = ["Lane 1", "Lane 2", "Lane 3", "Lane 4", "Lane 5"]
lane_clips = [
    # (start_bar, end_bar, color_idx, label)
    [(0.0, 1.8, 0, "Bass 124 G"), (2.5, 4.2, 0, "Bass 124 C#")],
    [(0.5, 2.0, 1, "Bass 124 C#"), (3.0, 4.5, 1, "Bass 124 A")],
    [(0.0, 1.5, 2, "Bass 124 D"), (1.8, 3.5, 2, "Bass 124 F")],
    [(0.5, 2.5, 3, "Bass 124 Cm"), (3.2, 5.0, 3, "Bass 124 G")],
    [(1.0, 3.0, 4, "Bass 124 bpm")],
]

for li, (name, clips) in enumerate(zip(lane_names, lane_clips)):
    ly = lane_top + comp_h + 1 + li * (lane_h + 1)
    if ly + lane_h > H:
        break

    # Lane header
    rounded_rect(d, (arr_x, ly, arr_x + header_w, ly + lane_h), surface2)
    d.text((arr_x + 12, ly + 10), name, fill=text_primary, font=font_bold_12)

    # M/S buttons - minimal style
    rounded_rect(d, (arr_x + 12, ly + 32, arr_x + 34, ly + 48), surface3, radius=6)
    d.text((arr_x + 17, ly + 34), "M", fill=text_muted, font=font_bold_11)
    rounded_rect(d, (arr_x + 38, ly + 32, arr_x + 60, ly + 48), surface3, radius=6)
    d.text((arr_x + 43, ly + 34), "S", fill=text_muted, font=font_bold_11)

    # Color indicator (thin line on left of lane)
    colors_list = ['#58a6ff', '#3fb950', '#a371f7', '#d29922', '#5e8b9e']
    indicator_color = colors_list[li % len(colors_list)]
    d.rectangle((arr_x + header_w - 3, ly, arr_x + header_w, ly + lane_h), fill=indicator_color)

    # Clips
    for start, end, ci, label in clips:
        cx1 = timeline_x + int(start * bar_w) + 2
        cx2 = timeline_x + int(end * bar_w) - 2
        clip_color = clip_colors[ci % len(clip_colors)]

        # Clip body with subtle gradient effect
        rounded_rect(d, (cx1, ly + 6, cx2, ly + lane_h - 6), clip_color + '40', radius=8)
        rounded_rect(d, (cx1, ly + 6, cx2, ly + 26), clip_color + '80', radius=8)
        # bottom half overlay to create fade
        d.rectangle((cx1+4, ly+18, cx2-4, ly+26), fill=clip_color + '80')

        # MIDI note visualization (small horizontal bars)
        import random
        random.seed(hash(label))
        for n in range(12):
            ny = ly + 28 + n * 4
            if ny > ly + lane_h - 10:
                break
            nx = cx1 + 6 + random.randint(0, 20)
            nw = random.randint(15, 50)
            if nx + nw > cx2 - 6:
                nw = cx2 - 6 - nx
            if nw > 5:
                d.rectangle((nx, ny, nx+nw, ny+2), fill=clip_color + 'a0')

        # Clip label
        d.text((cx1 + 8, ly + 9), label, fill=text_primary, font=font_reg_9)

    # Lane divider
    d.line([(arr_x, ly + lane_h), (W, ly + lane_h)], fill=border, width=1)

# Playhead
ph_x = timeline_x + int(1.2 * bar_w)
d.line([(ph_x, arr_top), (ph_x, H)], fill=red, width=2)
# Playhead triangle
d.polygon([(ph_x-5, arr_top), (ph_x+5, arr_top), (ph_x, arr_top+8)], fill=red)

# ─── DESIGN ANNOTATION ───
# Title card at bottom
d.rectangle((0, H-32, W, H), fill='#0a0d12')
d.text((20, H-24), "CONCEPT 1: MINIMAL MODERN", fill=text_muted, font=font_bold_12)
d.text((260, H-24), "Single unified toolbar  |  Pill-grouped controls  |  Wider spacing  |  Muted palette with accent highlights", fill=text_muted, font=font_reg_10)

img.save('/home/user/PatternFlow/mockup1_minimal_modern.png', 'PNG')
print("Mockup 1 saved")
