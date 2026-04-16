# PatternFlow UI Redesign Spec (Figma Handoff)

**Audience**: a design-focused AI (or designer) producing a high-fidelity redesign in Figma, plus an engineering AI implementing it in JUCE/C++.

**Goal**: redesign the interface elements and interactions while preserving the product’s capabilities, constraints, and performance characteristics.

**Scope**: GUI layout, component inventory, states, interactions, and visual system. This spec intentionally includes implementation constraints so the redesign stays buildable in JUCE.

---

## 1) Product summary (what PatternFlow is)

PatternFlow is a **MIDI arrangement + comping tool** delivered as **VST3 + Standalone** (JUCE). It:
- Imports MIDI files via a built-in browser and drag/drop.
- Places MIDI regions on a **beat-based session timeline** of fixed length (4/8/16 bars).
- Builds a computed **Main** lane (combined output) from all lanes or from comp segments.
- Provides **Comp** workflow (take-comp segment selection across lanes).
- Provides **live Transpose-to-Scale** mapping (root + scale type + octave).
- Provides a **Loop** system with start/end knobs and auto-enable/disable rules.
- Outputs MIDI during host playback.

### Hard constraints (must be reflected in the redesign)
- This is a plugin UI: **cannot create DAW clips directly** (no Ableton session-slot API).
- “Export” to a DAW is only possible via **dragging a `.mid` file** out of the plugin (external file drag) or via DAW-native features (Record/Capture).
- UI runs in JUCE: avoid designs requiring complex web/layout engines.
- Must remain performant while host is playing: repaints happen frequently.

---

## 2) What it DOES / DOES NOT do

### Does
- **Session timeline**: fixed-length loopable canvas; length selected in bars (4/8/16).
- **Beat grid**: grid division dropdown controls:
  - visual grid line spacing (arrangement + ruler)
  - snap behavior for drags and edits
- **Lanes + regions**:
  - multiple lanes; each lane has regions referencing MIDI clips
  - regions have start/end beats, can be moved/resized
  - multi-select supported
  - lane-level mute/solo
  - region mute in context menus
- **Main lane**:
  - computed combined clip (merged notes) shown at top of arrangement
  - can be dragged out to the DAW as a `.mid` file (external drag)
- **Comp**:
  - comp segments select time ranges from specific lanes
  - Random generates segments; boundaries snap to current grid division
  - Swap cycles segments to next lane
  - Default clears comp segments and disables comping
- **Transpose**:
  - live mapping to selected root + scale type, optional octave shift
  - toggle on/off
- **Loop**:
  - start/end knobs always adjustable
  - loop markers only visible when loop enabled
  - loop auto-on when start/end differ from defaults; auto-off when returned to defaults
- **Tempo modifiers**:
  - Half/Double toggles multiply internal playhead speed (0.5x / 2x)
  - playhead BPM display reflects the multiplier
- **File browser**:
  - empty state until folder selected
  - persists last folder and restores it
  - horizontal scrollbar appears only when needed
- **Preview strip**:
  - mini piano roll preview of selected MIDI
  - preview mute/solo buttons
  - background tint indicates preview “playing” when unmuted

### Does not
- No direct “commit to Ableton clip slot” without DAW-side scripting.
- No audio synthesis; MIDI only.
- No deep DAW track/clip control beyond standard plugin I/O and file drag.

---

## 3) Layout architecture (Figma frame structure)

### Overall window
- **Minimum size**: 1120 × 600
- **Resizable**: yes (up to ~2400 × 1600)
- **Primary split**: left sidebar (File Browser) + main content (Arrangement + optional Piano Roll).

### Regions
1) **Menu Bar 1 (Top Header Row)**  
2) **Menu Bar 2 (Control Strip)**  
3) **Content Area**  
   - Left: File Browser panel (fixed/resizable width)  
   - Center: Arrangement view  
   - Bottom (optional): Piano Roll editor  

### Resizers
- Vertical resizer between File Browser and Arrangement.
- Horizontal resizer between Arrangement and Piano Roll (when Piano Roll is visible).

---

## 4) Design tokens (style system)

Use these as the **source of truth** for Figma variables/tokens.

### 4.1 Base scale
- **UI Scale**: 1.5× (baked into metrics in code)

### 4.2 Metrics (key sizes)
Derived from `Source/Theme.h` `metrics::...`:
- **Corner radius (default)**: 6px
- **Button height (baseline)**: ~48px (32 * uiScale)
- **Header row heights**:
  - Menu Bar 1 height = Menu Bar 2 height (same height)
- **Sidebar width defaults**:
  - default: 230px
  - min: 160px
  - max: 420px

### 4.3 Typography
Based on `fontFor(TextStyle::LabelLarge)` etc.:
- **Primary UI label font**: ~15px, SemiBold (buttons, dropdowns, lane titles)
- **Secondary label font**: 12–13px (subtle text)
- **Wordmark**: large (the current UI uses a large “Pattern Flow” wordmark; redesign can scale but must preserve hierarchy).

### 4.4 Colors (dark DAW palette)
Tokens come from `colours::...()` in `Theme.h`:
- **Background / surface**: `#1a1a1a`
- **Panel / elevated**: `#252525`
- **Panel border**: `outlineVariant` (~`#2d2d2d`)
- **Text primary**: white
- **Text secondary**: `#a0a0a0`
- **Accent**: blue `#3b82f6`
- **Accent dim**: deep blue `#1e3a8a` (used for active tint overlays)
- **Record**: red `#ef4444`

### 4.5 Component styling rules
Buttons (`ActionButton`):
- Base fill: `#252525`
- Border: `#333333`
- Hover: slightly brighter fill (~`#2a2a2a`) and border `#444`
- Active (toggle on): fill = accentDim with alpha ~0.30, border accent alpha ~0.5, text accentBright

Dropdowns (`ComboBox`):
- Same container style as buttons
- Chevron at right
- Text left padding = `comboTextPadding`

Ghost header buttons (legacy) should be avoided in redesign; use consistent ActionButton styling for all clickable controls.

---

## 5) Menu Bar 1 (Top Header Row) — component spec

### Purpose
Primary “transport + global editing” strip. Everything here is **session-global** and should remain compact, readable, and stable during resize.

### Layout (left → right)
**All items share a common height** (same as Menu Bar 2 controls), vertically centered in the bar.

1) **Wordmark / App Title**
   - Text: **“Pattern Flow”** (note the space)
   - Font: Display/Headline scale (visually 2× the old logo height); must not compress horizontally.
   - Alignment: left aligned within a fixed title region.
   - Interaction: none (not a button).

2) **Playhead BPM display box** (read-only display that looks like a button)
   - Content: integer BPM of *effective* playhead tempo.
     - \( \text{effectiveBPM} = \text{hostBPM} \times \text{tempoMultiplier} \)
   - Default: shows host BPM (e.g. 120).
   - Style: identical to a button container but **non-clickable**; cursor stays default.
   - Width: auto based on 3–4 digits, plus padding; minimum width to avoid jitter when BPM changes.
   - States:
     - Disabled host tempo: show “—” (or “0”) when host BPM unavailable.
     - While host playing: no additional animation (avoid distracting pulsing).

3) **Half-time button** (“Half”)
   - Type: toggle
   - Behavior:
     - If Off → On: set tempoMultiplier = 0.5, force Double off.
     - If On → Off: set tempoMultiplier = 1.0.
   - Visual:
     - On: active background tint + active text color.
     - Off: normal.

4) **Double-time button** (“Double”)
   - Type: toggle
   - Behavior:
     - If Off → On: set tempoMultiplier = 2.0, force Half off.
     - If On → Off: set tempoMultiplier = 1.0.
   - Visual same as Half.

5) **Section divider** (vertical hairline)
   - Height: ~60–70% of row height
   - Color: outlineVariant, 60–80% opacity
   - Purpose: separates tempo tools from record/grid tools.

6) **Record button** (circular or rounded-square)
   - Must be the **same height** as dropdowns.
   - Placement: grouped with grid + session dropdowns (record first).
   - Icon: solid record dot or “REC”.
   - Color: red fill; hover slightly brighter; pressed slightly darker.
   - States:
     - Off: not recording
     - On: recording active (red), optionally a subtle inner glow; do not animate continuously if it risks repaint cost.
   - Interaction:
     - Click toggles recording on/off.
     - While recording, arrangement updates may refresh periodically.

7) **Grid division dropdown** (“Beats” / grid)
   - Content (examples): 1/4, 1/8, 1/16, triplets if supported.
   - **No icons** in the dropdown.
   - Width: wide enough to avoid truncation of any label.
   - Behavior:
     - Affects snap grid for edits.
     - Affects arrangement vertical grid line spacing (must match precisely).

8) **Session length dropdown** (“Bars” / session)
   - Content: 4 Bars, 8 Bars, 16 Bars (or as supported)
   - **No icons** in the dropdown.
   - Behavior:
     - Changes session end beat.
     - Loop End default must update to session end (see Loop rules).

9) **Section divider** (vertical hairline)

10) **Step button**
    - Type: momentary action (NOT toggle)
    - Visual: hover highlight only; after click it returns to normal.
    - Behavior: performs “step” operation on clips (exact algorithm is implementation-defined; preserve semantics).

11) **Extend button**
    - Type: momentary action
    - Behavior: extends clips (preserve semantics).

12) **Trim button**
    - Type: momentary action
    - Behavior: trims **all** clips (no selection required).

13) **Settings button** (gear icon)
    - Button frame should be tight to icon with consistent padding.
    - Gear icon size: **2× previous** (target ~40px).
    - Interaction: opens settings/preferences panel or popup (current UI uses a button; redesign can use popover or modal).

### Resize behavior
- Menu Bar 1 height is fixed (tokenized), does not scale with window size.
- As window narrows:
  - Title can shrink first (ellipsis allowed) **after** controls preserve minimum widths.
  - Dropdowns must not truncate their text; if needed, enforce minimum widths and allow title truncation.
  - Record + dropdown group should remain stable.

---

## 6) Menu Bar 2 (Control Strip) — component spec

### Purpose
This strip contains the “musical transforms” that affect output: **Comp**, **Transpose**, and **Loop**. It must communicate which transforms are active and expose controls without clutter.

### Structure
Menu Bar 2 is visually divided into **three columns/sections**:
1) **Comp**
2) **Transpose**
3) **Loop**

Each section:
- Has a section label (optional if the control labels are self-evident).
- Has controls with the same baseline button/dropdown height.
- Shows an **Active Section Background Tint** when its feature is enabled.

### Active section background tint (critical)
- When Comp is enabled, the Comp section background gets a subtle tint (accentDim @ ~0.18–0.24 alpha).
- When Transpose/Scale is enabled, Transpose section gets tint.
- When Loop is enabled, Loop section gets tint.
- Tint must not reduce legibility; it should read as “this module is currently engaged.”

### 6.1 Comp section

**Controls (left → right)**:
- **Comp Toggle** (button)
  - Type: toggle
  - On: Comping engaged (affects Main output)
  - Off: Comping disabled (Main is combined/standard)

- **Random** (button)
  - Type: momentary action
  - Generates comp segments across session, snapping to current grid division.
  - Disabled if there are insufficient lanes or no clips (implementation may disable).

- **Swap** (button, text-only)
  - Type: momentary action
  - No icon.
  - Swaps the active comp lane per segment (preserve current behavior).

- **Default** (button)
  - Type: momentary action
  - Clears all comps and disables comping (returns to neutral state).

**States**:
- When Comp Toggle is OFF:
  - Random/Swap/Default may be disabled (preferred) or enabled but no-op (avoid).
  - Section tint OFF.
- When Comp Toggle is ON:
  - Random/Swap/Default enabled.
  - Section tint ON.

### 6.2 Transpose section (Scale)

**Controls (left → right)**:
- **Transpose/Scale Toggle**
  - Type: toggle
  - Enables pitch mapping to selected scale.

- **Root dropdown**
  - Values: C, C#, D, … B

- **Scale dropdown**
  - Values: Major, Minor, Dorian, Phrygian, etc. (match implementation list)
  - Must be the “gold standard” sizing: other dropdowns must match its text size and height.

- **Octave control** (if present)
  - Could be a dropdown or ± buttons; preserve functionality if present in current app.

**States**:
- Toggle OFF:
  - Controls may remain adjustable but should visually indicate inactive mapping (dim text) OR be disabled. (Either is acceptable; prefer disabled to reduce confusion.)
  - Section tint OFF.
- Toggle ON:
  - Controls enabled.
  - Section tint ON.

### 6.3 Loop section

**Controls (left → right)**:
- **Loop Toggle** (button)
  - Type: toggle
  - Turns looping behavior on/off.
  - Does not prevent editing loop points (knobs always functional), but it controls:
    - visibility of loop markers
    - whether loop sync button is enabled

- **∞ Sync** button
  - Type: toggle or action (current UX is “sync”)
  - **Gated**: if Loop Toggle is OFF, Sync must be disabled (cannot change loop state).

- **Loop Start knob**
  - Type: continuous knob (rotary)
  - Range: 0 → session end
  - Default: 0
  - Always adjustable.

- **Loop End knob**
  - Type: continuous knob (rotary)
  - Range: 0 → session end
  - Default: session end (depends on bars dropdown)
  - Always adjustable.

**Auto-enable/auto-disable rules (must be exactly modeled)**
- Let:
  - \( S = \) loopStartBeat
  - \( E = \) loopEndBeat
  - \( D_S = 0 \)
  - \( D_E = \) sessionEndBeat
- Default state on load:
  - \( S = D_S \)
  - \( E = D_E \)
  - Loop Toggle = OFF
  - Loop markers = hidden
- If user changes **either knob** such that \( S \neq D_S \) OR \( E \neq D_E \):
  - Loop Toggle automatically turns ON
  - Loop markers become visible
- If user returns both knobs to defaults \( S = D_S \) AND \( E = D_E \):
  - Loop Toggle automatically turns OFF
  - Loop markers hidden
- User can always manually toggle Loop ON/OFF:
  - If toggled OFF, markers hidden even if \(S/E\) are non-default (matches current rule: markers only when enabled).
  - However, moving a knob again must be allowed to re-enable.

**Loop visual states**
- Loop OFF:
  - Markers hidden in arrangement.
  - Loop knobs and related UI adopt **grayscale** styling (text + indicators gray).
  - Sync is disabled.
- Loop ON:
  - Markers visible.
  - Knobs use accent styling.
  - Sync enabled.

---

## 7) Left Sidebar: File Browser + Preview — component spec

### Purpose
Browse MIDI files, select for preview, drag files in, and see a preview mini-roll + mute/solo.

### 7.1 Folder selection header
- Button label: **“Select a folder”**
- Shape: **no rounded corners** (radius = 0)
- Width: full width of sidebar column
- Height: must match Menu Bar 2 button height
- Position: pinned to top of sidebar; does not overlap content

### 7.2 Empty state (default)
- When no user folder selected:
  - file list is hidden
  - show centered hint text (e.g., “Choose a folder to browse MIDI files”)
  - keep UI calm; no empty list chrome

### 7.3 Persisted folder behavior
- After folder is selected:
  - save folder path
  - on next load, auto-open that folder and show its file tree

### 7.4 File tree list
- Layout: directly below folder button; fills remaining height
- Text behavior:
  - **Do not condense** long filenames (no squeezing/scaling to fit)
  - When filename exceeds viewport, content width expands and:
    - horizontal scrollbar appears **only when needed**
    - otherwise remains hidden
- Row height: compact but readable; align to overall typography scale.
- Selection:
  - selected row uses subtle selection background; avoid high-contrast neon.

### 7.5 Preview strip (inside the sidebar)
The preview area is a fixed-height section (typically bottom or top depending on existing layout; keep predictable).

**Preview background state**
- If preview is unmuted (playing), preview background gets a subtle active tint.
- If preview is muted, background returns to normal panel color.

**Mute/Solo buttons**
- Shape: rectangular buttons with same corner radius as menu bar buttons
- Padding:
  - top padding above buttons must equal bottom padding below buttons
  - buttons should not feel “stuck” to the container bottom
- Colors:
  - Preview Mute “inactive” / muted indication should be **dark gray**, not yellow.

**Hit targets**
- Must remain easy to click (minimum ~22px height; consistent with lane header buttons).

---

## 8) Main Content: Arrangement View — component spec

### Purpose
Primary canvas for MIDI clip arrangement, lane management, comp editing, loop markers, and playhead.

### 8.1 Coordinate system
- Horizontal axis: beats (0 → session end beat)
- Vertical axis: lanes (Main lane at top + N lanes below)

### 8.2 Grid & ruler
- Ruler at top of arrangement shows:
  - bar numbers
  - beat subdivisions (optional)
- Vertical grid lines:
  - spacing must **exactly match** the grid division dropdown (1/4 means a line every quarter note, etc.)
  - major grid lines at bar boundaries (stronger)
  - minor grid lines at subdivision boundaries (weaker)

### 8.3 Playhead
- Shows current beat position.
- Must reflect Half/Double tempo modifiers (effective playhead BPM).
- Visual:
  - thin bright line with subtle glow; avoid thick neon.

### 8.4 Loop markers
- Only visible when Loop is ON.
- Markers at Start and End:
  - vertical lines with handles (optional) and translucent region shading between them.
- When Loop OFF:
  - markers not drawn at all (not merely dimmed).

### 8.5 Lanes

**Lane header**
- Contains:
  - Lane title text
  - Mute button
  - Solo button
- Lane title style:
  - must match the typography and spacing of Menu Bar 2 button/dropdown text
  - same font size (~15px) and similar alpha dimming for inactive lanes

**Lane Mute/Solo buttons**
- Must match preview window M/S:
  - same size
  - same font style
  - same corner radius

**Main lane**
- Label: “MAIN” (not “COMBINED”)
- Represents final output.
- Must support drag-out (external drag) of the main MIDI as a `.mid` file.
  - Provide a clear affordance (e.g. drag handle icon or “Drag out” hint that only appears on hover).

### 8.6 Regions (clips)
- Appear as rounded rectangles with:
  - name label
  - optional mini note preview pattern inside
- States:
  - default
  - hovered (outline)
  - selected (stronger outline or fill)
  - muted (dimmed)
  - recording (if applicable, show subtle indicator)
- Interactions:
  - click selects
  - shift/cmd modifies selection
  - drag moves clip; snap to grid
  - drag edges resizes; snap to grid
  - drag across empty area performs marquee selection

### 8.7 Extra bottom space (important)
- The arrangement must show:
  - at least one additional “empty lane” partially visible (top half)
  - purpose: enable drag-selection without needing to resize the window

---

## 9) Piano Roll Editor (optional / when visible)

### Purpose
Detailed note editing for selected region(s).

### Layout
- Appears below arrangement in a split view.
- Has:
  - keyboard (vertical)
  - note grid aligned to same session grid division
  - note blocks with velocity indication (optional)

### Interactions
- Click to add note (if supported).
- Drag to move notes (snap to grid).
- Resize note length (snap).
- Selection & multi-select.

### Performance
- Avoid heavy gradients and per-note effects that require expensive repaints.

---

## 10) Settings (gear)

### Presentation
- Popover or modal sheet anchored to the gear icon.
- Must not steal focus in a way that breaks host keyboard shortcuts unexpectedly.

### Likely contents (design should allocate space)
- Theme / appearance (if supported)
- Default paths (file browser)
- Recording preferences
- MIDI routing info / help

---

## 11) Interaction principles (global)

### 11.1 Momentary vs toggle (strict)
- Step / Extend / Trim are **momentary**:
  - hover highlight only
  - no persistent gray “stuck” state

### 11.2 Disabled gating (strict)
- Loop Sync disabled when Loop OFF.
- Loop markers hidden when Loop OFF.
- Any controls that could accidentally change state should be gated or clearly indicated.

### 11.3 Feedback
- Use subtle toasts for actions like “Default comps applied”, “Folder loaded”, “Drag MIDI to DAW”.
- Avoid spam during playback (rate-limit any notifications).

---

## 12) Performance + implementation constraints (design must respect)

### Repaint budget
- The arrangement view repaints frequently (playhead, recording).
- Avoid:
  - animated gradients
  - blur shadows that change every frame
  - large transparent overlays that force full-surface redraws

### Hit targets and text
- Minimum interactive height: ~22px for tiny buttons (M/S), ~48px for primary row buttons.
- Text must not be scaled down to fit (no condensation). Prefer:
  - allow overflow + scroll (file list)
  - enforce minimum widths (dropdowns)

### Layout system
- Implementation uses manual `resized()` layout. Keep:
  - clear rectangles
  - consistent paddings
  - avoid complex constraint-based geometry.

---

## 13) Accessibility
- Contrast: ensure readable text on dark backgrounds (AA-ish).
- States: active vs inactive must be discernible beyond color alone (outline or icon).
- Keyboard:
  - tab focus order should follow visual order for menu bars.
  - escape closes popovers.

---

## 14) Figma deliverables checklist
- Component library:
  - Buttons (default/hover/pressed/toggled/disabled)
  - Dropdowns
  - Dividers
  - Knobs (loop start/end)
  - Mute/Solo mini buttons
- Frames:
  - Default idle state (no folder selected)
  - Folder selected + file list
  - Loop OFF vs Loop ON
  - Comp ON tint
  - Transpose ON tint
  - Recording ON
  - Narrow window (min width) behavior
  - Wide window behavior
- Prototype interactions:
  - Loop auto-on/off logic
  - Half/Double toggles updating BPM display
  - File name overflow showing scrollbar

---

## 15) Full component inventory (for Figma components)

This is the canonical list of UI elements that must exist in the redesign, with required behaviors.

### 15.1 Buttons (standard)
- **ActionButton / Primary**: used for Record, toggles, and primary actions.
- **ActionButton / Secondary**: used for non-critical actions (e.g. Swap/Default).
- **IconButton**: used for Settings (gear), optionally for drag handles.
- **Segmented toggle** (optional): may replace Half/Double with a 3-state control (Normal/Half/Double) **only if it preserves “press again to return to Normal”**.

### 15.2 Dropdowns
- **Grid dropdown** (no icon): values must all fit, no truncation.
- **Session dropdown** (no icon): values must all fit, no truncation.
- **Root dropdown**.
- **Scale dropdown** (reference sizing standard).

### 15.3 Knobs
- **RotaryKnob / LoopStart**
  - Value text formatting: show beats or bars:beats if available; otherwise numeric.
  - Drag: vertical drag changes value; shift-drag fine control.
  - Double-click: resets to default (LoopStart → 0).
- **RotaryKnob / LoopEnd**
  - Double-click resets to session end.

### 15.4 Mini buttons (Mute/Solo)
- **MiniButton / Mute** and **MiniButton / Solo**
  - Same geometry in lane headers and preview.
  - Text labels: “M” and “S” OR “Mute”/“Solo” depending on available space.
  - Tooltip on hover is recommended (low repaint cost).

### 15.5 Dividers
- **HeaderDivider**: 1px vertical line with padding.
- **SectionBackground**: tint overlay block for active sections.

---

## 16) Precise spacing & alignment rules (no guesswork)

### 16.1 Global paddings
- **Outer window padding**: 8px around top bars and content.
- **Control spacing within bars**:
  - Gap between controls: 6–8px
  - Padding inside buttons: 10–12px horizontal, 8–10px vertical
  - IconButton padding: 8–10px around icon

### 16.2 Dropdown internal layout
- Left padding to text: 12px
- Right padding to chevron: 10px
- Chevron width: 12–14px

### 16.3 Lane header
- Title left inset: 10–12px
- Gap title → buttons: 10px
- M/S button gap: 6px
- Buttons aligned to lane header vertical center.

### 16.4 Sidebar
- Folder button flush edge-to-edge.
- File list begins exactly at folder button bottom edge (no overlap).
- Horizontal scrollbar:
  - thickness: 8–10px
  - only appears when content width > viewport width.

---

## 17) Interaction flows (step-by-step)

### 17.1 Typical workflow: load and arrange
1) User clicks **Select a folder** → chooses a directory.
2) File list appears; user selects a MIDI file.
3) Preview updates immediately.
4) User drags file from browser into arrangement lane → creates region snapped to grid.
5) User uses Step/Extend/Trim to shape regions.
6) User enables Comp and generates segments (Random), then tweaks with Swap/Default.
7) User enables Transpose and selects scale.
8) User adjusts loop start/end; loop auto-enables; markers appear.
9) User drags MAIN lane out to DAW to create a MIDI clip file drop.

### 17.2 Loop edge cases
- **Session length changed while loop knobs are default**:
  - LoopEnd default updates to new session end.
  - Loop remains OFF.
- **Session length changed while loop is ON**:
  - Clamp \(S/E\) into new session range.
  - If after clamping \(S=0\) and \(E=sessionEnd\), auto-turn loop OFF.

### 17.3 Half/Double edge cases
- Host BPM unavailable:
  - Half/Double still toggle multiplier but BPM display shows “—”.
- Switching Half → Double:
  - Happens instantly; never allow both active.

---

## 18) Visual state tables (must be implemented)

### 18.1 Button states
- **Default**: surface fill, outline border, text primary.
- **Hover**: +6–10% brightness on fill; outline slightly stronger.
- **Pressed**: -6–10% brightness on fill.
- **Toggled On**: accentDim tint overlay; text in accentBright (or white).
- **Disabled**: reduce alpha to 40–55%, remove hover/press feedback.

### 18.2 Module active tint states
- **Comp enabled**: Comp section background tinted.
- **Scale enabled**: Transpose section tinted.
- **Loop enabled**: Loop section tinted.

### 18.3 Loop colors
- Loop OFF: loop-related elements grayscale.
- Loop ON: loop-related elements use accent.

---

## 19) Drag/drop behaviors (critical)

### 19.1 External drag (MAIN lane → DAW)
- Dragging from MAIN produces a `.mid` file promise or temp file drag.
- Cursor: show “copy” badge.
- On drag start: generate MIDI snapshot of current MAIN content.
- On drag end: DAW receives file; plugin may delete temp file later.
- UX affordance:
  - show a small drag handle or hover hint in MAIN lane only.

### 19.2 Internal drag (regions)
- Drag region body: moves start time; snap to grid.
- Drag region edges: resizes; snap to grid.
- While dragging, show:
  - ghost outline
  - numeric tooltip of start/end in beats or bars:beats.

---

## 20) Context menus (recommended)

### 20.1 Lane header context menu
- Rename lane (if supported)
- Duplicate lane (if supported)
- Clear lane
- Mute/Solo (quick toggles)

### 20.2 Region context menu
- Mute region
- Duplicate region
- Delete region
- Quantize to grid (if supported)
- Set as comp source (if supported)

---

## 21) QA-oriented acceptance criteria (design-aligned)

The redesign is acceptable if:
- Menu Bar 1 and 2 are the same height and controls align perfectly.
- Grid + Session dropdowns match Scale dropdown typography and height.
- No dropdown label truncation at minimum window size (title can ellipsize instead).
- File list never condenses text; horizontal scrollbar appears only when needed.
- “Select a folder” is full width, square corners, fixed at top, never overlaps list.
- Lane titles match Menu Bar 2 typography.
- Lane M/S buttons match preview M/S buttons.
- Loop markers never visible when loop is OFF.
- Sync cannot change loop state when loop is OFF.
- Moving either loop knob from default turns loop ON; returning both to default turns it OFF.
- Step/Extend/Trim behave as momentary actions (no persistent toggle highlight).


