"""
Mockup 2: Pro Studio
Design principles: Professional DAW aesthetic, visual hierarchy through contrast,
clear section separation, prominent transport, warm accent colors, Ableton/Bitwig inspired.
"""
from PIL import Image, ImageDraw, ImageFont
import random

W, H = 1280, 720
img = Image.new('RGB', (W, H), '#1a1a2e')
d = ImageDraw.Draw(img)

try:
    font_bold_20 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 20)
    font_bold_14 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 14)
    font_bold_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 12)
    font_bold_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 11)
    font_bold_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 10)
    font_reg_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
    font_reg_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 11)
    font_reg_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 10)
    font_reg_9 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 9)
except:
    font_bold_20 = font_bold_14 = font_bold_12 = font_bold_11 = font_bold_10 = ImageFont.load_default()
    font_reg_12 = font_reg_11 = font_reg_10 = font_reg_9 = ImageFont.load_default()

# Colors - Warm studio palette
bg_dark = '#16162a'
bg = '#1a1a2e'
surface1 = '#222240'
surface2 = '#2a2a48'
surface3 = '#323258'
border = '#3a3a5a'
border_light = '#4a4a6a'
text_primary = '#e8e8f0'
text_secondary = '#a0a0b8'
text_muted = '#6a6a80'
accent_warm = '#ff7b54'     # Warm orange accent
accent_cool = '#6c63ff'     # Cool purple secondary
accent_green = '#4ade80'
accent_yellow = '#fbbf24'
accent_red = '#f43f5e'

def rounded_rect(draw, xy, fill, radius=8, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)

# ─── TOP BAR - Split into left transport & right tools ───
top_h = 52

# Full top bar background
d.rectangle((0, 0, W, top_h), fill=surface1)

# Logo with warm accent
d.text((16, 14), "Pattern", fill=text_primary, font=font_bold_20)
d.text((110, 14), "Flow", fill=accent_warm, font=font_bold_20)

# Separator
d.line([(168, 8), (168, top_h-8)], fill=border, width=1)

# Transport section - centered and prominent
transport_x = 180

# Record button
d.ellipse((transport_x, 14, transport_x+24, 38), fill='#3a1520', outline=accent_red, width=2)
d.ellipse((transport_x+5, 19, transport_x+19, 33), fill=accent_red)

# Grid dropdown
rounded_rect(d, (transport_x+34, 12, transport_x+100, 40), surface2, radius=6, outline=border)
d.text((transport_x+42, 19), "Beat \u25be", fill=text_primary, font=font_reg_10)

# Loop button - active state
rounded_rect(d, (transport_x+110, 12, transport_x+162, 40), accent_cool+'40', radius=6, outline=accent_cool)
d.text((transport_x+121, 19), "Loop", fill='#b0a8ff', font=font_bold_10)

# Bars
d.text((transport_x+174, 13), "BARS", fill=text_muted, font=font_reg_9)
rounded_rect(d, (transport_x+172, 24, transport_x+210, 42), surface2, radius=6, outline=border)
d.text((transport_x+186, 27), "4 \u25be", fill=text_primary, font=font_bold_11)

# Separator
d.line([(transport_x+224, 8), (transport_x+224, top_h-8)], fill=border, width=1)

# Arrangement tools
tool_x = transport_x + 238
for i, label in enumerate(["Step", "Extend", "Trim"]):
    tx = tool_x + i * 60
    rounded_rect(d, (tx, 14, tx+52, 38), surface2, radius=6, outline=border)
    d.text((tx+8, 19), label, fill=text_secondary, font=font_reg_10)

# RIGHT SIDE - Scale & Comp
# Scale section
scale_x = 640
d.line([(scale_x - 12, 8), (scale_x - 12, top_h-8)], fill=border, width=1)

# Scale label chip
rounded_rect(d, (scale_x, 14, scale_x+52, 38), accent_warm+'30', radius=6)
d.text((scale_x+8, 19), "Scale", fill=accent_warm, font=font_bold_10)

rounded_rect(d, (scale_x+60, 14, scale_x+96, 38), surface2, radius=6, outline=border)
d.text((scale_x+70, 19), "C#", fill=text_primary, font=font_bold_11)

rounded_rect(d, (scale_x+104, 14, scale_x+224, 38), surface2, radius=6, outline=border)
d.text((scale_x+112, 19), "Harmonic Min \u25be", fill=text_primary, font=font_reg_10)

rounded_rect(d, (scale_x+232, 14, scale_x+300, 38), surface3, radius=6, outline=border)
d.text((scale_x+240, 19), "Transp.", fill=text_secondary, font=font_reg_10)

rounded_rect(d, (scale_x+308, 14, scale_x+340, 38), surface3, radius=6, outline=border)
d.text((scale_x+316, 19), "C0", fill=text_secondary, font=font_bold_10)

# Comp section
comp_x = 1000
d.line([(comp_x - 12, 8), (comp_x - 12, top_h-8)], fill=border, width=1)

rounded_rect(d, (comp_x, 14, comp_x+52, 38), surface3, radius=6, outline=border)
d.text((comp_x+7, 19), "Comp", fill=text_secondary, font=font_reg_10)

# Comp slider region count
d.text((comp_x+62, 13), "Regions", fill=text_muted, font=font_reg_9)
rounded_rect(d, (comp_x+60, 24, comp_x+110, 40), surface2, radius=6, outline=border)
d.text((comp_x+80, 27), "6", fill=text_primary, font=font_bold_11)

# Random / Swap as icon buttons
rounded_rect(d, (comp_x+120, 14, comp_x+178, 38), accent_cool+'25', radius=6, outline=accent_cool+'60')
d.text((comp_x+126, 19), "Rand.", fill='#9a94ff', font=font_reg_10)

rounded_rect(d, (comp_x+186, 14, comp_x+236, 38), surface3, radius=6, outline=border)
d.text((comp_x+194, 19), "Swap", fill=text_secondary, font=font_reg_10)

# Settings gear
d.text((W-36, 18), "\u2699", fill=text_muted, font=font_bold_14)

# Bottom border
d.line([(0, top_h), (W, top_h)], fill=accent_warm+'40', width=2)

# ─── BROWSER PANEL ───
browser_w = 210
browser_top = top_h + 2

# Browser bg
d.rectangle((0, browser_top, browser_w, H), fill=bg_dark)
d.line([(browser_w, browser_top), (browser_w, H)], fill=border, width=1)

# Browser header with icon
rounded_rect(d, (0, browser_top, browser_w, browser_top + 32), surface1)
d.text((12, browser_top + 8), "\u25bc BROWSER", fill=text_secondary, font=font_bold_11)

# Folder selector
rounded_rect(d, (8, browser_top + 40, browser_w - 8, browser_top + 60), surface2, radius=6, outline=border)
d.text((16, browser_top + 44), "Select a folder \u25be", fill=text_secondary, font=font_reg_10)

# File list with alternating rows
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
    "25. Bass 174 hrn A#.mid",
]

for i, f in enumerate(files):
    fy = browser_top + 68 + i * 22
    if fy > H - 40:
        break
    if i % 2 == 0:
        d.rectangle((4, fy-2, browser_w-4, fy+18), fill=surface1+'30')
    if i == 9:
        rounded_rect(d, (4, fy-2, browser_w-4, fy+18), accent_warm+'25', radius=4)
        d.text((12, fy), f, fill=accent_warm, font=font_reg_10)
    else:
        d.text((12, fy), f, fill=text_secondary, font=font_reg_10)

# Scrollbar
scrollbar_x = browser_w - 6
d.rectangle((scrollbar_x, browser_top + 68, scrollbar_x+4, browser_top + 68 + 120), fill=border_light, outline=None)

# Preview indicator at bottom
d.rectangle((0, H - 36, browser_w, H), fill=surface1)
d.ellipse((12, H-28, 24, H-16), fill=accent_yellow)
d.text((30, H-28), "Preview: Bass 124 bpm C.mid", fill=text_muted, font=font_reg_9)

# ─── ARRANGEMENT VIEW ───
arr_x = browser_w + 1
arr_top = top_h + 2
header_w = 110
timeline_x = arr_x + header_w
timeline_w = W - timeline_x

# Timeline ruler
ruler_h = 26
d.rectangle((arr_x, arr_top, W, arr_top + ruler_h), fill=surface1)
d.line([(arr_x, arr_top + ruler_h), (W, arr_top + ruler_h)], fill=border, width=1)

# Beat numbers with bar markers
total_bars = 5
bar_w = timeline_w / total_bars
for bar in range(total_bars):
    bx = timeline_x + bar * bar_w
    d.text((bx + 6, arr_top + 7), str(bar + 1), fill=text_muted, font=font_bold_10)
    d.line([(bx, arr_top + ruler_h), (bx, H)], fill=border, width=1)
    for beat in range(1, 4):
        sbx = bx + beat * (bar_w / 4)
        d.line([(sbx, arr_top + ruler_h), (sbx, H)], fill='#222238', width=1)

# ─── COMP LANE ───
comp_lane_top = arr_top + ruler_h + 1
comp_h = 38

d.rectangle((arr_x, comp_lane_top, arr_x + header_w, comp_lane_top + comp_h), fill=surface2)
d.text((arr_x + 10, comp_lane_top + 12), "COMP", fill=text_muted, font=font_bold_10)

# Comp segments with colored indicators
comp_segs = [
    (0.0, 0.8, 0), (0.8, 1.6, 1), (1.6, 2.4, 2),
    (2.4, 3.2, 0), (3.2, 4.0, 3), (4.0, 5.0, 1),
]
seg_colors = ['#3a5080', '#408060', '#604080', '#806040']
for start, end, ci in comp_segs:
    cx1 = timeline_x + int(start * bar_w)
    cx2 = timeline_x + int(end * bar_w)
    rounded_rect(d, (cx1+1, comp_lane_top+3, cx2-1, comp_lane_top+comp_h-3), seg_colors[ci], radius=3)

d.line([(arr_x, comp_lane_top + comp_h), (W, comp_lane_top + comp_h)], fill=border, width=1)

# ─── LANES ───
lane_h = 100
lane_gap = 2
lane_names = ["Lane 1", "Lane 2", "Lane 3", "Lane 4", "Lane 5"]
lane_accent_colors = ['#58a6ff', '#4ade80', '#a78bfa', '#fb923c', '#67e8f9']

lane_clips = [
    [(0.0, 1.8, "Bass 124 G"), (2.5, 4.2, "Bass 124 C#")],
    [(0.5, 2.0, "Bass 124 C#"), (3.0, 4.5, "Bass 124 A")],
    [(0.0, 1.5, "Bass 124 D"), (1.8, 3.5, "Bass 124 F")],
    [(0.5, 2.5, "Bass 124 Cm"), (3.2, 5.0, "Bass 124 G")],
    [(1.0, 3.0, "Bass 124 bpm")],
]

for li, (name, clips) in enumerate(zip(lane_names, lane_clips)):
    ly = comp_lane_top + comp_h + lane_gap + li * (lane_h + lane_gap)
    if ly + lane_h > H - 36:
        break

    lane_color = lane_accent_colors[li]

    # Lane header with color stripe
    d.rectangle((arr_x, ly, arr_x + header_w, ly + lane_h), fill=surface2)
    d.rectangle((arr_x, ly, arr_x + 4, ly + lane_h), fill=lane_color)

    d.text((arr_x + 12, ly + 10), name, fill=text_primary, font=font_bold_12)

    # M/S with lane color tint
    rounded_rect(d, (arr_x + 12, ly + 34, arr_x + 36, ly + 50), surface3, radius=4)
    d.text((arr_x + 18, ly + 36), "M", fill=text_muted, font=font_bold_10)
    rounded_rect(d, (arr_x + 42, ly + 34, arr_x + 66, ly + 50), surface3, radius=4)
    d.text((arr_x + 48, ly + 36), "S", fill=text_muted, font=font_bold_10)

    # Lane color dot
    d.ellipse((arr_x + 76, ly + 36, arr_x + 90, ly + 50), fill=lane_color + '60', outline=lane_color)

    # Lane content background
    d.rectangle((timeline_x, ly, W, ly + lane_h), fill=bg + '80')

    # Clips
    for start, end, label in clips:
        cx1 = timeline_x + int(start * bar_w) + 2
        cx2 = timeline_x + int(end * bar_w) - 2

        # Clip shadow
        rounded_rect(d, (cx1+2, ly+10, cx2+2, ly+lane_h-8), '#00000040', radius=8)

        # Clip body
        rounded_rect(d, (cx1, ly+8, cx2, ly+lane_h-10), lane_color + '20', radius=8, outline=lane_color + '50')

        # Clip header stripe
        rounded_rect(d, (cx1, ly+8, cx2, ly+28), lane_color + '35', radius=8)
        d.rectangle((cx1+4, ly+20, cx2-4, ly+28), fill=lane_color + '35')

        # MIDI visualization
        random.seed(hash(label))
        for n in range(14):
            ny = ly + 30 + n * 4
            if ny > ly + lane_h - 14:
                break
            nx = cx1 + 8 + random.randint(0, 30)
            nw = random.randint(10, 45)
            if nx + nw > cx2 - 8:
                nw = cx2 - 8 - nx
            if nw > 5:
                rounded_rect(d, (nx, ny, nx+nw, ny+2), lane_color + '70', radius=1)

        # Label
        d.text((cx1 + 8, ly + 11), label, fill=text_primary, font=font_reg_9)

    d.line([(arr_x, ly + lane_h), (W, ly + lane_h)], fill=border + '80', width=1)

# Playhead
ph_x = timeline_x + int(1.3 * bar_w)
d.line([(ph_x, arr_top), (ph_x, H)], fill=accent_red, width=2)
d.polygon([(ph_x-6, arr_top), (ph_x+6, arr_top), (ph_x, arr_top+10)], fill=accent_red)

# ─── BOTTOM STATUS BAR ───
d.rectangle((0, H-28, W, H), fill=surface1)
d.line([(0, H-28), (W, H-28)], fill=accent_warm+'30', width=1)
d.text((16, H-22), "CONCEPT 2: PRO STUDIO", fill=text_muted, font=font_bold_12)
d.text((220, H-22), "Warm orange accent  |  Lane color coding  |  Clear hierarchy  |  Elevated clips with shadow  |  Professional DAW feel", fill=text_muted, font=font_reg_10)

img.save('/home/user/PatternFlow/mockup2_pro_studio.png', 'PNG')
print("Mockup 2 saved")
