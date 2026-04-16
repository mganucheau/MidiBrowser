"""
Mockup 3: Soft Gradient
Design principles: Depth through gradients and glassmorphism effects, floating card panels,
warm-cool gradient blends, softer corners, more visual breathing room, unified color harmony.
"""
from PIL import Image, ImageDraw, ImageFont
import random

W, H = 1280, 720
img = Image.new('RGB', (W, H), '#0f0f1a')
d = ImageDraw.Draw(img)

try:
    font_bold_18 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
    font_bold_14 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 14)
    font_bold_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 12)
    font_bold_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 11)
    font_bold_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 10)
    font_reg_12 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
    font_reg_11 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 11)
    font_reg_10 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 10)
    font_reg_9 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 9)
except:
    font_bold_18 = font_bold_14 = font_bold_12 = font_bold_11 = font_bold_10 = ImageFont.load_default()
    font_reg_12 = font_reg_11 = font_reg_10 = font_reg_9 = ImageFont.load_default()

# Colors - Soft gradient palette (teal + purple harmony)
bg = '#0f0f1a'
surface1 = '#181828'
surface2 = '#1e1e32'
surface3 = '#252540'
glass = '#ffffff08'
glass_border = '#ffffff15'
border = '#2a2a44'
border_light = '#3a3a5a'
text_primary = '#e8e8f8'
text_secondary = '#9898b8'
text_muted = '#5a5a78'
accent_teal = '#4dd0e1'
accent_purple = '#b388ff'
accent_pink = '#f48fb1'
accent_green = '#69f0ae'
accent_red = '#ff5252'
accent_amber = '#ffd54f'

def rounded_rect(draw, xy, fill, radius=8, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)

# ─── BACKGROUND GRADIENT GLOW (subtle) ───
# Simulated with colored rectangles
for i in range(20):
    alpha = max(5, 20 - i)
    # Top-left teal glow
    d.rectangle((0, i*4, 400, i*4+4), fill=f'#008080{alpha:02x}')
    # Top-right purple glow
    d.rectangle((W-400, i*4, W, i*4+4), fill=f'#400080{alpha:02x}')

# ─── TOP BAR - Floating glass panel ───
top_h = 50
margin = 8
rounded_rect(d, (margin, margin, W-margin, top_h+margin), surface2, radius=14, outline=glass_border)

# Logo
d.text((24, 20), "Pattern", fill=accent_teal, font=font_bold_18)
d.text((118, 20), "Flow", fill=accent_purple, font=font_bold_18)

# Separator glow line
d.line([(176, 16), (176, top_h)], fill=border_light, width=1)

# ─── Transport as floating chip group ───
tx = 192
# Record
d.ellipse((tx, 20, tx+20, 40), fill='#2a1020', outline=accent_red)
d.ellipse((tx+4, 24, tx+16, 36), fill=accent_red)

# Grid
rounded_rect(d, (tx+28, 16, tx+88, 44), glass, radius=10, outline=glass_border)
d.text((tx+36, 24), "Beat \u25be", fill=text_primary, font=font_reg_10)

# Loop - glowing active state
rounded_rect(d, (tx+96, 16, tx+146, 44), '#00606040', radius=10, outline=accent_teal+'60')
d.text((tx+107, 24), "Loop", fill=accent_teal, font=font_bold_10)

# Bars
rounded_rect(d, (tx+154, 16, tx+208, 44), glass, radius=10, outline=glass_border)
d.text((tx+162, 18), "BARS", fill=text_muted, font=font_reg_9)
d.text((tx+188, 24), "4", fill=text_primary, font=font_bold_11)

# Tools
for i, label in enumerate(["Step", "Extend", "Trim"]):
    bx = tx + 218 + i * 56
    rounded_rect(d, (bx, 18, bx+48, 42), glass, radius=8, outline=glass_border)
    d.text((bx+8, 24), label, fill=text_secondary, font=font_reg_10)

# ─── Scale as floating card ───
sx = 590
rounded_rect(d, (sx, 14, sx+340, 46), surface3, radius=12, outline=glass_border)

# Scale toggle - glowing
rounded_rect(d, (sx+6, 18, sx+54, 42), accent_teal+'20', radius=8)
d.text((sx+12, 24), "Scale", fill=accent_teal, font=font_bold_10)

d.text((sx+62, 24), "C#", fill=text_primary, font=font_bold_11)
d.text((sx+84, 28), "\u2022", fill=text_muted, font=font_reg_9)
d.text((sx+96, 24), "Harmonic Minor", fill=text_primary, font=font_reg_10)
d.text((sx+212, 28), "\u2022", fill=text_muted, font=font_reg_9)

rounded_rect(d, (sx+224, 18, sx+288, 42), glass, radius=8, outline=glass_border)
d.text((sx+232, 24), "Transp.", fill=text_secondary, font=font_reg_10)
d.text((sx+296, 24), "C0", fill=text_muted, font=font_bold_10)

# ─── Comp as floating card ───
cx = 960
rounded_rect(d, (cx, 14, cx+270, 46), surface3, radius=12, outline=glass_border)

rounded_rect(d, (cx+6, 18, cx+56, 42), glass, radius=8, outline=glass_border)
d.text((cx+12, 24), "Comp", fill=text_secondary, font=font_reg_10)

d.text((cx+64, 24), "6", fill=text_primary, font=font_bold_11)

rounded_rect(d, (cx+82, 18, cx+146, 42), accent_purple+'20', radius=8)
d.text((cx+90, 24), "Random", fill=accent_purple, font=font_reg_10)

rounded_rect(d, (cx+154, 18, cx+210, 42), glass, radius=8, outline=glass_border)
d.text((cx+166, 24), "Swap", fill=text_secondary, font=font_reg_10)

# Settings
d.text((W-36, 22), "\u2699", fill=text_muted, font=font_bold_14)

# ─── BROWSER PANEL - Floating card ───
browser_w = 200
browser_top = top_h + margin + 8
browser_bottom = H - margin

rounded_rect(d, (margin, browser_top, browser_w + margin, browser_bottom), surface1, radius=14, outline=glass_border)

# Browser header
d.text((24, browser_top + 12), "BROWSER", fill=text_muted, font=font_bold_10)

# Folder
rounded_rect(d, (20, browser_top + 32, browser_w, browser_top + 52), surface2, radius=8, outline=border)
d.text((28, browser_top + 36), "Select a folder \u25be", fill=text_secondary, font=font_reg_10)

# Files
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
    fy = browser_top + 60 + i * 22
    if fy > browser_bottom - 40:
        break
    if i == 9:
        rounded_rect(d, (16, fy-2, browser_w+2, fy+18), accent_teal+'15', radius=6)
        d.rectangle((16, fy-2, 20, fy+18), fill=accent_teal)
        d.text((26, fy), f, fill=accent_teal, font=font_reg_10)
    else:
        d.text((26, fy), f, fill=text_secondary, font=font_reg_10)

# Preview at bottom
d.line([(20, browser_bottom - 36), (browser_w, browser_bottom - 36)], fill=border, width=1)
d.ellipse((24, browser_bottom - 28, 36, browser_bottom - 16), fill=accent_amber)
d.text((42, browser_bottom - 28), "Preview active", fill=text_muted, font=font_reg_9)

# ─── ARRANGEMENT VIEW - Floating card ───
arr_left = browser_w + margin + 8
arr_top = browser_top
header_w = 100
timeline_x = arr_left + header_w
timeline_w = W - margin - timeline_x

rounded_rect(d, (arr_left, arr_top, W - margin, browser_bottom), surface1, radius=14, outline=glass_border)

# Ruler
ruler_h = 28
d.rectangle((timeline_x, arr_top + 4, W - margin - 4, arr_top + ruler_h), fill=surface2)

total_bars = 5
bar_w = timeline_w / total_bars
for bar in range(total_bars):
    bx = timeline_x + bar * bar_w
    d.text((bx + 6, arr_top + 10), str(bar + 1), fill=text_muted, font=font_bold_10)
    d.line([(bx, arr_top + ruler_h), (bx, browser_bottom - margin)], fill=border, width=1)
    for beat in range(1, 4):
        sbx = bx + beat * (bar_w / 4)
        d.line([(sbx, arr_top + ruler_h), (sbx, browser_bottom - margin)], fill='#1a1a30', width=1)

# ─── COMP LANE ───
comp_top = arr_top + ruler_h + 2
comp_h = 36

d.rectangle((arr_left + 4, comp_top, arr_left + header_w - 4, comp_top + comp_h), fill=surface2)
rounded_rect(d, (arr_left + 4, comp_top, arr_left + header_w - 4, comp_top + comp_h), surface2, radius=8)
d.text((arr_left + 14, comp_top + 11), "COMP", fill=text_muted, font=font_bold_10)

# Comp blocks with gradient feel
comp_segs = [
    (0.0, 0.8, accent_teal), (0.8, 1.6, accent_green),
    (1.6, 2.4, accent_purple), (2.4, 3.2, accent_teal),
    (3.2, 4.0, accent_pink), (4.0, 5.0, accent_green),
]
for start, end, color in comp_segs:
    cx1 = timeline_x + int(start * bar_w)
    cx2 = timeline_x + int(end * bar_w)
    rounded_rect(d, (cx1+1, comp_top+4, cx2-1, comp_top+comp_h-4), color+'18', radius=4)
    d.rectangle((cx1+1, comp_top+4, cx2-1, comp_top+8), fill=color+'35')

d.line([(arr_left+4, comp_top + comp_h), (W-margin-4, comp_top + comp_h)], fill=border, width=1)

# ─── LANES ───
lane_h = 94
lane_gap = 3
lane_names = ["Lane 1", "Lane 2", "Lane 3", "Lane 4", "Lane 5"]
lane_colors = [accent_teal, accent_green, accent_purple, accent_pink, accent_amber]

lane_clips = [
    [(0.0, 1.8, "Bass 124 G"), (2.5, 4.2, "Bass 124 C#")],
    [(0.5, 2.0, "Bass 124 C#"), (3.0, 4.5, "Bass 124 A")],
    [(0.0, 1.5, "Bass 124 D"), (1.8, 3.5, "Bass 124 F")],
    [(0.5, 2.5, "Bass 124 Cm"), (3.2, 5.0, "Bass 124 G")],
    [(1.0, 3.0, "Bass 124 bpm")],
]

for li, (name, clips) in enumerate(zip(lane_names, lane_clips)):
    ly = comp_top + comp_h + lane_gap + li * (lane_h + lane_gap)
    if ly + lane_h > browser_bottom - 40:
        break

    lc = lane_colors[li]

    # Lane header as mini card
    rounded_rect(d, (arr_left+4, ly+2, arr_left+header_w-4, ly+lane_h-2), surface2, radius=8)

    # Color bar
    rounded_rect(d, (arr_left+4, ly+2, arr_left+8, ly+lane_h-2), lc, radius=4)

    d.text((arr_left+16, ly+10), name, fill=text_primary, font=font_bold_11)

    # M/S - circular buttons
    d.ellipse((arr_left+16, ly+32, arr_left+34, ly+50), fill=surface3, outline=border)
    d.text((arr_left+21, ly+35), "M", fill=text_muted, font=font_bold_10)
    d.ellipse((arr_left+40, ly+32, arr_left+58, ly+50), fill=surface3, outline=border)
    d.text((arr_left+45, ly+35), "S", fill=text_muted, font=font_bold_10)

    # Clips with glow effect
    for start, end, label in clips:
        cx1 = timeline_x + int(start * bar_w) + 3
        cx2 = timeline_x + int(end * bar_w) - 3

        # Subtle glow behind clip
        rounded_rect(d, (cx1-1, ly+6, cx2+1, ly+lane_h-6), lc+'10', radius=10)

        # Clip body
        rounded_rect(d, (cx1, ly+8, cx2, ly+lane_h-8), lc+'12', radius=10, outline=lc+'30')

        # Top gradient bar
        rounded_rect(d, (cx1, ly+8, cx2, ly+26), lc+'25', radius=10)
        d.rectangle((cx1+6, ly+20, cx2-6, ly+26), fill=lc+'25')

        # MIDI notes
        random.seed(hash(label))
        for n in range(12):
            ny = ly + 28 + n * 4
            if ny > ly + lane_h - 12:
                break
            nx = cx1 + 8 + random.randint(0, 25)
            nw = random.randint(10, 40)
            if nx + nw > cx2 - 8:
                nw = cx2 - 8 - nx
            if nw > 5:
                rounded_rect(d, (nx, ny, nx+nw, ny+2), lc+'50', radius=1)

        d.text((cx1 + 10, ly + 11), label, fill=text_primary, font=font_reg_9)

    d.line([(arr_left+4, ly+lane_h), (W-margin-4, ly+lane_h)], fill=border+'60', width=1)

# Playhead with glow
ph_x = timeline_x + int(1.4 * bar_w)
# Glow
for g in range(4, 0, -1):
    d.line([(ph_x-g, arr_top+ruler_h), (ph_x-g, browser_bottom-margin)], fill=accent_red+f'{g*10:02x}', width=1)
    d.line([(ph_x+g, arr_top+ruler_h), (ph_x+g, browser_bottom-margin)], fill=accent_red+f'{g*10:02x}', width=1)
d.line([(ph_x, arr_top+ruler_h), (ph_x, browser_bottom-margin)], fill=accent_red, width=2)
d.polygon([(ph_x-6, arr_top+ruler_h-2), (ph_x+6, arr_top+ruler_h-2), (ph_x, arr_top+ruler_h+8)], fill=accent_red)

# ─── ANNOTATION ───
d.rectangle((0, H-28, W, H), fill='#08081040')
d.text((20, H-22), "CONCEPT 3: SOFT GRADIENT", fill=text_muted, font=font_bold_12)
d.text((240, H-22), "Floating card panels  |  Glassmorphism borders  |  Ambient glow effects  |  Teal/Purple color harmony  |  Generous breathing room", fill=text_muted, font=font_reg_10)

img.save('/home/user/PatternFlow/mockup3_soft_gradient.png', 'PNG')
print("Mockup 3 saved")
