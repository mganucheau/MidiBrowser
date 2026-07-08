# PatternFlow Design System (DAW / FigmaExample)

This document captures the **canonical UI design system** used by the prototype in `Design/FigmaExample/`.
It is the source of truth for **colors, typography, spacing, and interaction states**.

## Foundations

### Color palette (dark)

- **Background (app)**: `#1a1a1a`
- **Background (panel)**: `#1e1e1e`
- **Background (header / elevated)**: `#252525`
- **Background (hover / higher elevation)**: `#2a2a2a`

- **Border (primary separators)**: `#2d2d2d`
- **Border (inputs/buttons)**: `#333333`
- **Border (hover)**: `#444444`

- **Text (primary)**: `#ffffff`
- **Text (secondary)**: `#a0a0a0`
- **Text (muted)**: `#717182`

### Accent + status colors

- **Accent / selection (blue)**: `#3b82f6`
- **Accent bright (blue)**: `#60a5fa`
- **Record / critical (red)**: `#ef4444` (alt: `#dc2626`)
- **Active / safe (green)**: `#22c55e` (alt: `#16a34a`)
- **Warning / mute (yellow)**: `#facc15` (alt: `#eab308`)

### Typography

- **Header title**: 14px, medium
- **Most controls (buttons/selects/labels)**: 12px
- **Dense labels**: 10–11px

## Header layout (matches `Design/FigmaExample/src/app/components/PatternFlowHeader.tsx`)

### Row 1 (Toolbar)

- **Height**: 48px
- **Padding**: 12px left/right (`px-3`)
- **Background**: `#1e1e1e`
- **Bottom border**: 1px `#2d2d2d`
- **Left cluster**: Title + Record
  - Title: “PatternFlow”, 14px, medium
  - Record: 24×24, circle
    - Off: bg `#252525`, border `#333`, hover border `#444`
    - On: bg red + glow, pulse animation
- **Right cluster**: Global controls + divider + action buttons
  - Select chips: height 32px, bg `#252525`, border `#333`, hover border `#444`
  - Divider: 1px × 24px, color `#2d2d2d`
  - Action buttons (ghost): height 32px, transparent bg, hover bg `#2a2a2a`, 12px text

### Row 2 (Control strip)

- **Height**: 64px
- **Background**: `#1e1e1e`
- **Bottom border**: 1px `#2d2d2d`
- **3 columns**: equal width, with 1px dividers `#2d2d2d`
- **Each column**: centered content, consistent gaps (8–12px)

Column definitions:

1. **Comping**: `Comp` toggle + Comp knob + `Random` + `Swap`
2. **Pitch/Scale**: `Transpose` toggle + `Key` + `Scale` + `Octave` + `C0`
3. **Loop**: Loop Start knob + `∞` sync toggle + Loop End knob

### Button styles (applies app-wide)

- **Ghost (toolbar)**:
  - Default: transparent
  - Hover: bg `#2a2a2a`
  - No border (or very subtle)

- **Filled (controls / strip / general)**:
  - Default: bg `#252525`, border `#333`, hover bg `#2a2a2a`, hover border `#444`
  - Active/toggled: bg blue-900 @ ~30% alpha, text blue-400, border blue-500 @ ~50% alpha

## JUCE mapping notes

In the JUCE implementation, these tokens map to:

- `Source/Theme.h` / `pflow::colours::*` for palette roles.
- `Source/Theme.cpp` / `PatternFlowLookAndFeel` for drawing buttons, combo boxes, scrollbars.
- `Source/PluginEditor.cpp` for **Row 1** layout and sizing.
- `Source/ControlPanel.cpp` for **Row 2** (3-column control strip).

