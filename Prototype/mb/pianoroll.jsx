// pianoroll.jsx — mini preview + full editor with non-destructive note dragging,
// trim, and the instrument (octave / key / mode) strip.
const { STEPS_PER_BAR: STEPS, isBlackKey, pitchName, MODE_NAMES: MODES_LIST, ROOTS: ROOT_LIST } = window.MB_DATA;

// Compute the visible pitch window for a clip's resolved notes.
function pitchWindow(notes, pad = 2, min = 12) {
  if (!notes.length) return { top: 72, bottom: 48 };
  let lo = Infinity, hi = -Infinity;
  for (const n of notes) { lo = Math.min(lo, n.pitch); hi = Math.max(hi, n.pitch); }
  lo -= pad; hi += pad;
  while (hi - lo < min) { hi += 1; lo -= 1; }
  return { top: hi, bottom: lo };
}

function gridTokens(style) {
  // returns { bg, barLine, beatLine, rowLine, rowShade, note, noteMoved, noteEdge, playhead, kbBlack, kbWhite }
  if (style === 'blueprint') return {
    bg: 'var(--mb-roll-bp-bg)', barLine: 'var(--mb-roll-bp-bar)', beatLine: 'var(--mb-roll-bp-beat)',
    rowLine: 'var(--mb-roll-bp-row)', rowShade: 'transparent', note: 'var(--mb-roll-bp-note)', noteMoved: '#fff',
    noteEdge: 'var(--mb-roll-bp-edge)', mono: true,
  };
  if (style === 'minimal') return {
    bg: 'var(--mb-roll-bg)', barLine: 'var(--mb-line-strong)', beatLine: 'transparent',
    rowLine: 'transparent', rowShade: 'var(--mb-roll-shade)', note: 'var(--mb-accent)', noteMoved: 'var(--mb-accent-bright)',
    noteEdge: 'transparent', mono: false,
  };
  return { // lanes (default)
    bg: 'var(--mb-roll-bg)', barLine: 'var(--mb-line-strong)', beatLine: 'var(--mb-line)',
    rowLine: 'var(--mb-roll-rowline)', rowShade: 'var(--mb-roll-shade)', note: 'var(--mb-accent)', noteMoved: 'var(--mb-accent-bright)',
    noteEdge: 'var(--mb-roll-noteedge)', mono: false,
  };
}

// ── Mini preview (compact bottom strip — read-only) ───────────────────────────
function PianoRollMini({ clip, resolved, gridStyle, height = 132, playhead, playing }) {
  const notes = resolved.notes;
  const win = pitchWindow(notes, 1, 10);
  const rows = win.top - win.bottom + 1;
  const tk = gridTokens(gridStyle);
  const kbW = 34;
  const totalSteps = resolved.bars * STEPS;
  return (
    <div style={{ height, display: 'flex', background: tk.bg, position: 'relative', overflow: 'hidden' }}>
      {/* tiny keyboard */}
      <div style={{ width: kbW, flex: '0 0 auto', position: 'relative', borderRight: '1px solid var(--mb-line)' }}>
        {Array.from({ length: rows }).map((_, i) => {
          const pitch = win.top - i;
          return <div key={i} style={{ position: 'absolute', left: 0, right: 0, top: `${(i / rows) * 100}%`, height: `${(1 / rows) * 100}%`,
            background: isBlackKey(pitch) ? 'var(--mb-kb-black)' : 'var(--mb-kb-white)', borderBottom: '1px solid var(--mb-roll-bg)' }} />;
        })}
      </div>
      <div style={{ flex: 1, position: 'relative', minWidth: 0 }}>
        {/* bar lines */}
        {Array.from({ length: resolved.bars + 1 }).map((_, b) => (
          <div key={b} style={{ position: 'absolute', top: 0, bottom: 0, left: `${(b / resolved.bars) * 100}%`, width: 1, background: tk.barLine }} />
        ))}
        {/* notes */}
        {notes.map((n) => {
          const i = win.top - n.pitch;
          return <div key={n.id} style={{
            position: 'absolute', left: `${(n.start / totalSteps) * 100}%`, width: `${(n.len / totalSteps) * 100}%`,
            top: `${(i / rows) * 100}%`, height: `calc(${(1 / rows) * 100}% - 1px)`, marginTop: 0.5,
            background: n.moved ? tk.noteMoved : tk.note, borderRadius: 2, minWidth: 2,
            boxShadow: tk.noteEdge !== 'transparent' ? `inset 0 0 0 1px ${tk.noteEdge}` : 'none',
          }} />;
        })}
        {/* playhead */}
        {playing && <div style={{ position: 'absolute', top: 0, bottom: 0, left: `${(playhead / totalSteps) * 100}%`, width: 1.5, background: 'var(--mb-playhead)' }} />}
      </div>
    </div>
  );
}

// ── Instrument strip ──────────────────────────────────────────────────────────
function InstrumentStrip({ edit, onEdit }) {
  return (
    <div style={{ display: 'flex', alignItems: 'flex-end', gap: 12, rowGap: 9, padding: '9px 12px', borderBottom: '1px solid var(--mb-line)', background: 'var(--mb-panel)', flexWrap: 'wrap' }}>
      <Stepper label="Octave" value={edit.octave} min={-3} max={3} display={`${edit.octave > 0 ? '+' : ''}${edit.octave}`} onChange={(v) => onEdit({ octave: v })} />
      <Picker label="Key" w={60} value={edit.root || 'C'} options={ROOT_LIST} onChange={(v) => onEdit({ root: v, fitScale: true })} />
      <Picker label="Mode" w={112} value={edit.mode} options={MODES_LIST} onChange={(v) => onEdit({ mode: v, fitScale: true })} />
      <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
        <span style={{ fontSize: 10.5, fontWeight: 600, letterSpacing: '0.04em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>Fit to scale</span>
        <div style={{ display: 'flex', alignItems: 'center', height: 28, gap: 8 }}>
          <Switch value={edit.fitScale} onChange={(v) => onEdit({ fitScale: v, root: edit.root || 'C' })} />
          <span style={{ fontSize: 11.5, color: 'var(--mb-text3)', whiteSpace: 'nowrap' }}>{edit.fitScale ? 'snapping' : 'off'}</span>
        </div>
      </div>
    </div>
  );
}

// ── Full editor ────────────────────────────────────────────────────────────────
function PianoRollEditor({
  clip, edit, resolved, gridStyle, density, playhead, playing,
  instrumentOpen, onToggleInstrument, onEdit, onMoveNote, onTrim, onRevert, onCollapse, badges, canTrim,
}) {
  const [snap, setSnap] = React.useState(2); // steps
  const [zoom, setZoom] = React.useState(1);
  const [drag, setDrag] = React.useState(null);
  const tk = gridTokens(gridStyle);
  const notes = resolved.notes;
  const win = pitchWindow(notes, 2, 12);
  const rows = win.top - win.bottom + 1;
  const rowH = Math.round((density === 'comfortable' ? 13 : 11) * zoom);
  const stepW = (density === 'comfortable' ? 12 : 10) * zoom;
  const kbW = 48;
  const gridW = resolved.bars * STEPS * stepW;
  const gridH = rows * rowH;

  // original positions for ghosting moved notes
  const origById = React.useMemo(() => Object.fromEntries(clip.notes.map((n) => [n.id, n])), [clip]);

  const beginDrag = (e, note) => {
    e.preventDefault(); e.stopPropagation();
    const sx = e.clientX, sy = e.clientY;
    const state = { id: note.id, dPitch: 0, dStep: 0 };
    setDrag(state);
    const move = (ev) => {
      const dStepRaw = (ev.clientX - sx) / stepW;
      const dStep = Math.round(dStepRaw / snap) * snap;
      const dPitch = -Math.round((ev.clientY - sy) / rowH);
      if (dStep !== state.dStep || dPitch !== state.dPitch) {
        state.dStep = dStep; state.dPitch = dPitch;
        setDrag({ id: note.id, dPitch, dStep });
      }
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      if (state.dStep || state.dPitch) onMoveNote(note.id, state.dPitch, state.dStep);
      setDrag(null);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  };

  const totalSteps = resolved.bars * STEPS;
  const snapOpts = [{ value: 16, label: '1 bar' }, { value: 8, label: '1/2' }, { value: 4, label: '1/4' }, { value: 2, label: '1/8' }, { value: 1, label: '1/16' }];

  return (
    <div style={{ flex: 1, minWidth: 0, minHeight: 0, display: 'flex', flexDirection: 'column', background: 'var(--mb-panel2)', borderLeft: '1px solid var(--mb-line)' }}>
      {/* toolbar */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '0 8px 0 12px', height: 44, borderBottom: '1px solid var(--mb-line)', flex: '0 0 auto', overflow: 'hidden' }}>
        <MBIcon name="piano-keys" weight="fill" size={15} style={{ color: 'var(--mb-text2)', flex: '0 0 auto' }} />
        <div style={{ minWidth: 0, flex: '0 1 auto', display: 'flex', flexDirection: 'column', lineHeight: 1.15 }}>
          <span style={{ fontSize: 12.5, fontWeight: 600, color: 'var(--mb-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{clip.name}</span>
          <span style={{ fontSize: 10.5, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', whiteSpace: 'nowrap' }}>{resolved.bars} bars · {notes.length} notes</span>
        </div>
        <div style={{ flex: 1, minWidth: 8 }} />
        <Btn icon="sliders-horizontal" iconWeight={instrumentOpen ? 'fill' : 'regular'} variant="accent" active={instrumentOpen} onClick={onToggleInstrument} title="Instrument panel">Instrument</Btn>
        <Btn icon="scissors" variant="soft" onClick={onTrim} disabled={!canTrim} title={canTrim ? 'Remove empty leading/trailing bars' : 'No empty bars to trim'} />
        <div style={{ width: 1, height: 22, background: 'var(--mb-line)', flex: '0 0 auto' }} />
        <Picker w={72} value={snap} options={snapOpts} onChange={(v) => setSnap(Number(v))} />
        <div style={{ display: 'flex', gap: 2, flex: '0 0 auto' }}>
          <Btn icon="magnifying-glass-minus" variant="ghost" onClick={() => setZoom((z) => Math.max(0.6, +(z - 0.2).toFixed(2)))} title="Zoom out" />
          <Btn icon="magnifying-glass-plus" variant="ghost" onClick={() => setZoom((z) => Math.min(2, +(z + 0.2).toFixed(2)))} title="Zoom in" />
        </div>
        <div style={{ width: 1, height: 22, background: 'var(--mb-line)', flex: '0 0 auto' }} />
        <Btn icon="arrow-counter-clockwise" variant="ghost" onClick={() => onRevert('all')} disabled={!badges.length} title="Revert all edits" />
        <Btn icon="arrows-in-line-horizontal" variant="ghost" onClick={onCollapse} title="Collapse editor" />
      </div>

      {instrumentOpen && <InstrumentStrip edit={edit} onEdit={onEdit} />}

      {/* grid */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
        {/* keyboard */}
        <div style={{ width: kbW, flex: '0 0 auto', overflow: 'hidden', borderRight: '1px solid var(--mb-line)', background: 'var(--mb-panel)', position: 'relative' }}>
          <div style={{ height: gridH, position: 'relative' }}>
            {Array.from({ length: rows }).map((_, i) => {
              const pitch = win.top - i; const black = isBlackKey(pitch); const isC = pitch % 12 === 0;
              return (
                <div key={i} style={{ position: 'absolute', top: i * rowH, left: 0, right: 0, height: rowH,
                  background: black ? 'var(--mb-kb-black)' : 'var(--mb-kb-white)', borderBottom: '1px solid var(--mb-roll-bg)',
                  display: 'flex', alignItems: 'center', justifyContent: 'flex-end', paddingRight: 5 }}>
                  {isC && rowH >= 10 && <span style={{ fontSize: 8.5, fontFamily: 'var(--mb-mono)', color: 'var(--mb-text3)', fontWeight: 600 }}>{pitchName(pitch)}</span>}
                </div>
              );
            })}
          </div>
        </div>
        {/* scrollable grid */}
        <div style={{ flex: 1, overflow: 'auto', position: 'relative' }} className="mb-rollscroll">
          <div style={{ width: gridW, height: gridH, position: 'relative', background: tk.bg }}>
            {/* row shading + lines */}
            {Array.from({ length: rows }).map((_, i) => {
              const pitch = win.top - i; const black = isBlackKey(pitch);
              return <div key={i} style={{ position: 'absolute', top: i * rowH, left: 0, width: gridW, height: rowH,
                background: black ? tk.rowShade : 'transparent', borderBottom: `1px solid ${tk.rowLine}` }} />;
            })}
            {/* beat + bar lines */}
            {Array.from({ length: resolved.bars * 4 + 1 }).map((_, q) => {
              const isBar = q % 4 === 0;
              return <div key={q} style={{ position: 'absolute', top: 0, bottom: 0, left: q * 4 * stepW, width: isBar ? 1.5 : 1,
                background: isBar ? tk.barLine : tk.beatLine }} />;
            })}
            {/* bar numbers */}
            {Array.from({ length: resolved.bars }).map((_, b) => (
              <span key={b} style={{ position: 'absolute', top: 2, left: b * STEPS * stepW + 4, fontSize: 9, fontFamily: 'var(--mb-mono)', color: 'var(--mb-text3)', fontWeight: 600, pointerEvents: 'none' }}>{b + 1}</span>
            ))}
            {/* ghosts for moved notes (original position) */}
            {notes.filter((n) => n.moved).map((n) => {
              const o = origById[n.id]; if (!o) return null;
              const i = win.top - o.pitch;
              if (i < 0 || i >= rows) return null;
              return <div key={'g' + n.id} style={{ position: 'absolute', left: o.start * stepW, top: i * rowH + 1, width: o.len * stepW - 1, height: rowH - 2,
                borderRadius: 3, border: '1px dashed var(--mb-roll-ghost)', boxSizing: 'border-box', pointerEvents: 'none' }} />;
            })}
            {/* notes */}
            {notes.map((n) => {
              const isDragging = drag && drag.id === n.id;
              const dP = isDragging ? drag.dPitch : 0;
              const dS = isDragging ? drag.dStep : 0;
              const i = win.top - (n.pitch + dP);
              return (
                <div key={n.id} onPointerDown={(e) => beginDrag(e, n)} title={`${pitchName(n.pitch + dP)} · drag to move`}
                  style={{ position: 'absolute', left: (n.start + dS) * stepW, top: i * rowH + 1, width: Math.max(3, n.len * stepW - 1), height: rowH - 2,
                    background: (n.moved || isDragging) ? tk.noteMoved : tk.note, borderRadius: 3, cursor: 'grab', boxSizing: 'border-box',
                    boxShadow: isDragging ? '0 3px 10px rgba(0,0,0,.4)' : (tk.noteEdge !== 'transparent' ? `inset 0 0 0 1px ${tk.noteEdge}` : 'none'),
                    zIndex: isDragging ? 5 : 1, transition: isDragging ? 'none' : 'background .1s' }} />
              );
            })}
            {/* playhead */}
            {playing && <div style={{ position: 'absolute', top: 0, bottom: 0, left: playhead * stepW, width: 1.5, background: 'var(--mb-playhead)', zIndex: 6, pointerEvents: 'none' }}>
              <div style={{ position: 'absolute', top: 0, left: -3.5, width: 8, height: 8, background: 'var(--mb-playhead)', transform: 'rotate(45deg)' }} />
            </div>}
          </div>
        </div>
      </div>

      {/* footer hint */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '0 12px', height: 28, borderTop: '1px solid var(--mb-line)', flex: '0 0 auto', background: 'var(--mb-panel)', overflow: 'hidden' }}>
        <MBIcon name="hand-grabbing" size={12} style={{ color: 'var(--mb-text3)', flex: '0 0 auto' }} />
        <span style={{ fontSize: 11, color: 'var(--mb-text3)', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', minWidth: 0 }}>Drag notes to move them — source file stays intact</span>
        <div style={{ flex: 1, minWidth: 6 }} />
        <span style={{ fontSize: 10.5, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', whiteSpace: 'nowrap', flex: '0 0 auto' }}>Snap {snapOpts.find((o) => o.value === snap).label}</span>
      </div>
    </div>
  );
}

Object.assign(window, { PianoRollMini, PianoRollEditor, InstrumentStrip });
