# Audio DAW Professional Style Guide

This document provides comprehensive styling details for creating a professional Digital Audio Workstation (DAW) interface. Use these specifications to apply consistent DAW-style theming to any application.

---

## COLOR PALETTE

### Primary Background Colors
- **Main Background**: `#1a1a1a` - Primary dark background for main content areas
- **Secondary Background**: `#1e1e1e` - Slightly lighter, used for panels and tracks
- **Tertiary Background**: `#252525` - Used for headers, sidebars, and elevated elements
- **Elevated Background**: `#2a2a2a` - Used for hover states and master sections

### Border & Separator Colors
- **Primary Border**: `#2d2d2d` - Main border color for all separations
- **Secondary Border**: `#333` - Lighter borders for inputs and interactive elements
- **Hover Border**: `#444` - Border color on hover states

### Text Colors
- **Primary Text**: `white` or `#ffffff` - Main text and labels
- **Secondary Text**: `#a0a0a0` - Less important information
- **Muted Text**: `#717182` - Very subtle labels
- **Gray Text**: `#999` and `#666` - Helper text at various levels

### Accent Colors
- **Red (Critical/Record)**: `#ef4444`, `#dc2626` - Record buttons, clip indicators
- **Yellow (Warning)**: `#facc15`, `#eab308`, `#f9ca24` - Mute state, warning levels
- **Green (Active/Safe)**: `#22c55e`, `#16a34a` - Active states, safe levels
- **Blue (Selection/Info)**: `#3b82f6`, `#60a5fa` - Selection, master channel, info
- **Orange (Mid-Warning)**: `#f59e0b` - Mid-level warnings

### Track Color Palette (for color-coding tracks)
- Vocals: `#ff6b6b`
- Guitar: `#4ecdc4`
- Bass: `#45b7d1`
- Drums: `#f9ca24`
- Keys: `#a29bfe`
- Synth: `#ff9ff3`

---

## SPACING & LAYOUT

### Component Heights
- **Toolbar**: `48px` (h-12)
- **Transport Controls**: `56px` (h-14)
- **Panel Headers**: `40px` (h-10)
- **Track Height**: `96px` (h-24)
- **Timeline Ruler**: `32px` (h-8)

### Component Widths
- **Left Sidebar**: `256px` (w-64)
- **Track Header**: `224px` (w-56)
- **Mixer Panel**: `440px` (w-[440px])
- **Mixer Channel**: `80px` (w-20)
- **Master Channel**: `96px` (w-24)

### Padding & Margins
- **Panel Padding**: `0.75rem` (p-3) or `0.5rem` (p-2)
- **Button Padding**: `0.5rem` (p-2) for icons, `0.75rem px-3` for text buttons
- **Header Padding**: `0.75rem px-3`
- **Content Gap**: `0.375rem` (gap-1.5), `0.5rem` (gap-2), `0.75rem` (gap-3)

### Border Widths
- **Standard Border**: `1px`
- **Emphasis Border**: `2px` (for master channel, special sections)

---

## TYPOGRAPHY

### Font Sizes
- **Extra Small**: `9px` (text-[9px]) - VU meter labels, pan indicators
- **Tiny**: `10px` (text-[10px]) - Small labels, track info
- **Extra Small Standard**: `12px` (text-xs) - Buttons, tabs, general UI
- **Small**: `14px` (text-sm) - Panel headers
- **Base**: `16px` (text-base) - Default, not commonly used in DAW UI

### Font Weights
- **Normal**: `400` - Standard text
- **Medium**: `500` - Headers, labels, buttons

### Icon Sizes
- **Tiny Icons**: `12px` (size-3) - Small indicators in sidebar
- **Small Icons**: `16px` (size-4) - Toolbar icons, standard buttons
- **Medium Icons**: `20px` (size-5) - Larger controls

---

## COMPONENT-SPECIFIC STYLES

### Buttons
```
Base Ghost Button:
- Height: 32px (h-8)
- Padding: 8px (px-2)
- Background: transparent
- Hover: bg-[#2a2a2a]
- Text: text-xs

Icon-Only Button:
- Size: 32px × 32px (h-8 w-8)
- Padding: p-0
- Icon: size-4

Active States:
- Play Active: bg-green-600/20 with hover:bg-green-600/30
- Record Active: bg-red-900/30
- Mute Active: bg-yellow-900/30 text-yellow-500
- Solo Active: bg-blue-900/30 text-blue-500
```

### Input Fields
```
Background: #1a1a1a
Padding: px-2 py-1
Border: 1px solid #333
Border Radius: rounded
Text Size: text-xs
Focus State: border-[#444], outline-none
```

### Sliders
```
Track Background: #1a1a1a with rounded corners
Thumb: Rounded, draggable, typically 12-16px
Range: Uses track color or gradient
```

### VU Meters
```
Container:
- Width: 32px (w-8) for channels, 40px (w-10) for master
- Height: 128px (h-32)
- Background: #1a1a1a
- Border: 1px solid #333 (or #3b82f6/30 for master)
- Border Radius: rounded

Meter Fill:
- Bottom-aligned (absolute bottom-0)
- Gradient from track/accent color to transparent version
- Transition: duration-75
- Scale Markers: 6 horizontal lines (0, -6, -12, -18, -24, -30 dB)

Color Gradients:
- Green Zone: linear-gradient(to top, #22c55e, #16a34a)
- Yellow Zone: linear-gradient(to top, #facc15, #eab308)
- Red Zone: linear-gradient(to top, #ef4444, #dc2626)
```

### Track Components
```
Track Container:
- Height: 96px (h-24)
- Border Bottom: 1px solid #2d2d2d
- Display: flex

Track Header:
- Width: 224px (w-56)
- Background: #252525
- Border Right: 1px solid #2d2d2d
- Padding: 0.5rem (p-2)

Track Content Area:
- Flex: 1
- Background: #1e1e1e
- Contains waveform canvas or empty state
```

### Waveform Visualization
```
Canvas-based drawing with:
- Background: #1e1e1e
- Center Line: #333, 1px stroke
- Waveform Color: Track's assigned color
- Line Width: 1.5px
- Amplitude: Dynamic based on audio data or randomized for demo
```

### Timeline
```
Background: #1a1a1a
Height: 32px (h-8)
Grid Lines: Vertical repeating pattern, #252525
Time Markers: White text, text-[10px]
Playhead: 
- Width: 2px (w-0.5)
- Color: #ef4444 (red-500)
- Shadow: 0 0 8px rgba(239,68,68,0.5)
- Z-Index: 10
```

### Sidebar
```
Width: 256px (w-64)
Background: #1e1e1e
Border Right: 1px solid #2d2d2d

Tabs:
- Tab List Background: #252525
- Active Tab: bg-[#333]
- Tab Height: 40px (h-10)
- Text: text-xs

File/Instrument Items:
- Padding: 0.5rem (p-2)
- Hover Background: #252525
- Hover Border: 1px solid #333
- Border Radius: rounded
- Draggable: true
```

### Toolbar
```
Height: 48px (h-12)
Background: #1e1e1e
Border Bottom: 1px solid #2d2d2d
Padding: 0.75rem px-3

Separators:
- Width: 1px (w-px)
- Height: 24px (h-6)
- Color: #2d2d2d
- Margin: 0.25rem mx-1

Button Groups:
- Gap: 0.125rem (gap-0.5) for icon buttons
- Gap: 0.25rem (gap-1) for text buttons
```

### Transport Controls
```
Height: 56px (h-14)
Background: #1e1e1e
Border Bottom: 1px solid #2d2d2d
Centered content with flex layout

Buttons:
- Size: 40px × 40px (h-10 w-10)
- Icon Size: size-5
- Rounded: rounded-full for play/record
- Gap between buttons: gap-2

Time Display:
- Background: #1a1a1a
- Border: 1px solid #333
- Padding: px-4 py-2
- Font: Monospace (font-mono)
- Text Size: text-sm
```

### Mixer Panel
```
Width: 440px
Background: #1e1e1e
Border Left: 1px solid #2d2d2d

Header:
- Height: 40px (h-10)
- Background: #252525
- Border Bottom: 1px solid #2d2d2d

Channel Strip:
- Width: 80px (w-20)
- Background: #252525
- Border Right: 1px solid #2d2d2d
- Padding: 0.5rem (p-2)
- Layout: Vertical flex column with gaps

Master Channel:
- Width: 96px (w-24)
- Background: #2a2a2a
- Border Left: 2px solid #3b82f6/30 (blue accent)
```

---

## VISUAL EFFECTS & ANIMATIONS

### Hover Effects
```css
Track Header Hover: background changes to #2a2a2a
Plugin Slot Hover: border-[#444], bg-[#252525]
Track Region Hover: opacity increases from 80% to 100%
Scrollbar Thumb Hover: background changes from #333 to #444
```

### Transitions
```css
Standard Transition: transition-colors or transition-all
VU Meter: transition-all duration-75 (fast response)
Button Hover: Default transition (~200ms)
```

### Box Shadows & Glows
```css
Track Region:
- inset 0 1px 2px rgba(255, 255, 255, 0.1)
- inset 0 -1px 2px rgba(0, 0, 0, 0.2)

Playhead Glow:
- shadow-[0_0_8px_rgba(239,68,68,0.5)]

Status Indicators:
- Active: 0 0 8px rgba(34, 197, 94, 0.6) (green glow)
- Recording: 0 0 8px rgba(239, 68, 68, 0.6) (red glow, with pulse)

Knob Control:
- inset 0 2px 4px rgba(0, 0, 0, 0.5)
- 0 1px 2px rgba(255, 255, 255, 0.1)

Knob Indicator:
- 0 0 4px rgba(96, 165, 250, 0.8)
```

### Animations
```css
Pulse Animation (Recording):
- animate-pulse (built-in Tailwind)

Meter Pulse:
- @keyframes pulse-meter
- 0%, 100%: opacity 1
- 50%: opacity 0.7
- Duration: 1s ease-in-out infinite
```

### Gradients
```css
VU Meter Background:
- linear-gradient(to top, [color], [color with 88 alpha])

Fader Track (Multi-zone):
- Red: 0-5%
- Orange: 5-15%
- Green: 15-100%

Channel Strip:
- linear-gradient(to bottom, #252525, #1e1e1e)

Track Regions:
- bg-gradient-to-br with track color
- opacity-80, hover:opacity-100
```

### Custom Scrollbars
```css
::-webkit-scrollbar {
  width: 12px;
  height: 12px;
}

::-webkit-scrollbar-track {
  background: #1a1a1a;
}

::-webkit-scrollbar-thumb {
  background: #333;
  border-radius: 6px;
  border: 2px solid #1a1a1a;
}

::-webkit-scrollbar-thumb:hover {
  background: #444;
}
```

---

## INTERACTION PATTERNS

### Focus States
```css
Inputs & Buttons:
- outline: 2px solid rgba(59, 130, 246, 0.5)
- outline-offset: 2px
```

### Active/Toggle States
```
Record Armed:
- Button Background: bg-red-900/30
- Icon: fill-red-500 text-red-500

Mute:
- Button Background: bg-yellow-900/30
- Text Color: text-yellow-500

Solo:
- Button Background: bg-blue-900/30
- Text Color: text-blue-500

Playing:
- Button Background: bg-green-600/20
- Hover: bg-green-600/30
```

### Draggable Elements
```
- File/Instrument items in sidebar: draggable attribute
- Cursor: cursor-pointer
- Visual feedback on hover with border and background change
```

---

## LAYOUT STRUCTURE

### Main Application Layout
```
Vertical Flex Column (Full Height):
├── Toolbar (fixed height: 48px)
├── Transport Controls (fixed height: 56px)
└── Main Content (flex-1, flex row, overflow-hidden)
    ├── Left Sidebar (fixed width: 256px)
    ├── Center Tracks (flex-1, flex column)
    │   ├── Timeline (fixed height: 32px)
    │   └── Tracks Scroll Area (flex-1)
    └── Right Mixer (fixed width: 440px)
```

### Responsive Considerations
```
- Minimum width for tracks area: 800px (min-w-[800px])
- Horizontal scroll enabled for tracks
- Vertical scroll for track list
- Fixed panel widths maintain consistency
```

---

## ICONS (from lucide-react)

### Commonly Used Icons
```
- File operations: File, FolderOpen, Save
- Edit operations: Scissors, Copy, Clipboard, Undo, Redo
- Transport: Play, Pause, Square (stop), SkipBack, SkipForward, Circle (record)
- Audio: Volume2, AudioLines, Mic, Music, Sliders
- Track controls: Eye (visibility), Lock, Circle (record arm)
- UI: Settings, HelpCircle, Folder, Disc3
```

---

## DESIGN PRINCIPLES

### Professional Audio Software Aesthetics
1. **Dark Theme Dominance**: Reduces eye strain during long sessions
2. **Color-Coded Functionality**: Visual hierarchy through consistent color meanings
3. **Dense Information Display**: Maximize screen real estate efficiency
4. **Subtle Borders & Separators**: Clear organization without visual clutter
5. **Minimal Rounded Corners**: Professional, technical appearance
6. **Muted Color Palette**: Neutral grays with accent colors only for important states
7. **High Contrast Elements**: Critical controls (record, play) are immediately identifiable
8. **Compact Spacing**: Professional tools prioritize function density

### Interaction Design
1. **Immediate Visual Feedback**: Hover states, active states clearly indicated
2. **Color-Coded Status**: Red = record/danger, Yellow = mute/warning, Green = active/safe, Blue = selection/info
3. **Professional Typography**: Small, efficient text sizes (9-12px for most UI)
4. **Consistent Icon Sizing**: 12px and 16px as primary sizes
5. **Draggable Interface**: Files and instruments can be dragged onto tracks

---

## IMPLEMENTATION NOTES

### CSS Framework
- Built with **Tailwind CSS v4**
- Custom DAW-specific classes in separate stylesheet
- Uses CSS custom properties for theming
- Leverages `@layer components` for reusable DAW components

### Component Architecture
- Modular React components for each panel/section
- State management through props and local state
- Reusable UI primitives (Button, Slider, ScrollArea, Tabs)
- Canvas-based visualization for waveforms

### Font Stack
- System font stack (default)
- Monospace for time/numerical displays (font-mono)

### Performance Considerations
- Canvas for waveform rendering (better performance than SVG)
- Smooth transitions (75ms for meters, 200ms for UI)
- Overflow handling with custom scrollbars
- Fixed panel widths for layout stability

---

## USAGE EXAMPLE

To apply this style to another application:

1. **Set base background**: Use `#1a1a1a` for main app background
2. **Use color hierarchy**: `#1a1a1a` < `#1e1e1e` < `#252525` < `#2a2a2a` for depth
3. **Apply borders**: Consistent `#2d2d2d` for all separations
4. **Size panels**: Follow the specified heights and widths
5. **Typography**: Prefer small text (10-12px) for professional density
6. **Spacing**: Use tight gaps (1-3 = 4-12px) between elements
7. **Color accents**: Red for critical, yellow for warning, green for active, blue for info
8. **Hover effects**: Subtle background changes (`#2a2a2a`) and border color shifts
9. **Icons**: 12px or 16px from lucide-react, white or colored for states
10. **Interactions**: Clear active states with background tint and color change

---

## COLOR REFERENCE QUICK GUIDE

| Purpose | Color | Hex |
|---------|-------|-----|
| Darkest BG | bg-[#1a1a1a] | #1a1a1a |
| Dark BG | bg-[#1e1e1e] | #1e1e1e |
| Medium BG | bg-[#252525] | #252525 |
| Light BG | bg-[#2a2a2a] | #2a2a2a |
| Border | border-[#2d2d2d] | #2d2d2d |
| Input Border | border-[#333] | #333333 |
| Hover Border | border-[#444] | #444444 |
| Record/Danger | red-500/600 | #ef4444, #dc2626 |
| Warning/Mute | yellow-400/500 | #facc15, #eab308 |
| Active/Safe | green-500/600 | #22c55e, #16a34a |
| Info/Select | blue-400/500 | #60a5fa, #3b82f6 |

---

This style guide captures the complete professional DAW aesthetic with exact colors, spacing, typography, effects, and interaction patterns that can be directly applied to any application requiring this design language.
