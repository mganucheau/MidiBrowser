// editor-d.jsx — focused piano-roll editor.
// • Instrument strip is always on (no toggle).
// • Roll sizes to its content (no stretched blank space) — the window shrinks to fit.
// • Foldable Velocity lane below the roll: vertical bars per note, scaled by Intensity.
// • Foldable Groove knob panel below that.
const { STEPS_PER_BAR: STEPS_D, isBlackKey: isBlackKeyD, pitchName: pitchNameD, ROOTS: ROOT_LIST_D, MODE_NAMES: MODES_LIST_D } = window.MB_DATA;

// Local instrument strip with a "Drag Me" affordance (drags the clip into the DAW in the real app).
function InstrumentStripD({ edit, onEdit, clip }) {
  return (
    <div style={{ display: 'flex', alignItems: 'flex-end', gap: 12, rowGap: 9, padding: '9px 12px', borderBottom: '1px solid var(--mb-line)', background: 'var(--mb-panel)', flexWrap: 'wrap' }}>
      <Stepper label="Octave" value={edit.octave} min={-3} max={3} display={`${edit.octave > 0 ? '+' : ''}${edit.octave}`} onChange={(v) => onEdit({ octave: v })} />
      <Picker label="Key" w={60} value={edit.root || 'C'} options={ROOT_LIST_D} onChange={(v) => onEdit({ root: v, fitScale: true })} />
      <Picker label="Mode" w={112} value={edit.mode} options={MODES_LIST_D} onChange={(v) => onEdit({ mode: v, fitScale: true })} />
      <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
        <span style={{ fontSize: 10.5, fontWeight: 600, letterSpacing: '0.04em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>Fit to scale</span>
        <div style={{ display: 'flex', alignItems: 'center', height: 28, gap: 8 }}>
          <Switch value={edit.fitScale} onChange={(v) => onEdit({ fitScale: v, root: edit.root || 'C' })} />
          <span style={{ fontSize: 11.5, color: 'var(--mb-text3)', whiteSpace: 'nowrap' }}>{edit.fitScale ? 'snapping' : 'off'}</span>
        </div>
      </div>
      <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
        <span style={{ fontSize: 10.5, fontWeight: 600, letterSpacing: '0.04em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>Map to root</span>
        <div style={{ display: 'flex', alignItems: 'center', height: 28, gap: 8 }}>
          <Switch value={edit.mapToRoot} onChange={(v) => onEdit({ mapToRoot: v, root: edit.root || 'C' })} />
          <span style={{ fontSize: 11.5, color: 'var(--mb-text3)', whiteSpace: 'nowrap', fontFamily: 'var(--mb-mono)' }}>{edit.mapToRoot ? `${clip.root} → ${edit.root || 'C'}` : 'off'}</span>
        </div>
      </div>

      <div style={{ flex: 1, minWidth: 12 }} />

      <button type="button" draggable="true" title={`Drag “${clip.name}” into your DAW`}
        onDragStart={(e) => { try { e.dataTransfer.setData('text/plain', clip.name); e.dataTransfer.effectAllowed = 'copy'; } catch (_) {} }}
        className="mb-btn"
        style={{ display: 'flex', alignItems: 'center', gap: 8, height: 34, padding: '0 14px', borderRadius: 9,
          border: '1px dashed var(--mb-accent-line)', background: 'var(--mb-accent-soft)', color: 'var(--mb-accent)',
          font: 'inherit', fontSize: 12.5, fontWeight: 600, letterSpacing: '-0.01em', cursor: 'grab', whiteSpace: 'nowrap', userSelect: 'none' }}>
        <MBIcon name="dots-six-vertical" weight="bold" size={15} />
        Drag Me
      </button>
    </div>
  );
}

function pitchWindowD(notes, pad = 2, min = 12) {
  if (!notes.length) return { top: 72, bottom: 48 };
  let lo = Infinity, hi = -Infinity;
  for (const n of notes) { lo = Math.min(lo, n.pitch); hi = Math.max(hi, n.pitch); }
  lo -= pad; hi += pad;
  while (hi - lo < min) { hi += 1; lo -= 1; }
  return { top: hi, bottom: lo };
}

function gridTokensD(style) {
  if (style === 'blueprint') return {
    bg: 'var(--mb-roll-bp-bg)', barLine: 'var(--mb-roll-bp-bar)', beatLine: 'var(--mb-roll-bp-beat)',
    rowLine: 'var(--mb-roll-bp-row)', rowShade: 'transparent', note: 'var(--mb-roll-bp-note)', noteMoved: '#fff',
    noteEdge: 'var(--mb-roll-bp-edge)',
  };
  if (style === 'minimal') return {
    bg: 'var(--mb-roll-bg)', barLine: 'var(--mb-line-strong)', beatLine: 'transparent',
    rowLine: 'transparent', rowShade: 'var(--mb-roll-shade)', note: 'var(--mb-accent)', noteMoved: 'var(--mb-accent-bright)',
    noteEdge: 'transparent',
  };
  return {
    bg: 'var(--mb-roll-bg)', barLine: 'var(--mb-line-strong)', beatLine: 'var(--mb-line)',
    rowLine: 'var(--mb-roll-rowline)', rowShade: 'var(--mb-roll-shade)', note: 'var(--mb-accent)', noteMoved: 'var(--mb-accent-bright)',
    noteEdge: 'var(--mb-roll-noteedge)',
  };
}

// deterministic base velocity now lives in knob.jsx (baseVel / noteVelocity)

const ROLL_MAX = 348; // cap roll viewport; taller clips scroll internally

function PianoRollEditorD({
  clip, edit, resolved, gridStyle, density, playhead, playing,
  onEdit, onMoveNote, onTrim, onRevert, badges, canTrim, isTrimmed,
  knobs, onKnob, knobsOpen, onToggleKnobs, onResetKnobs, velOpen, onToggleVel,
}) {
  const [snap, setSnap] = React.useState(2);
  const [zoom, setZoom] = React.useState(1); // 1 = fit to view; >1 zooms in (horizontal)
  const [drag, setDrag] = React.useState(null);
  const [selectedNotes, setSelectedNotes] = React.useState(() => new Set());
  const [marquee, setMarquee] = React.useState(null);
  const velScrollRef = React.useRef(null);
  const gridScrollRef = React.useRef(null);
  const gridInnerRef = React.useRef(null);
  React.useEffect(() => { setSelectedNotes(new Set()); }, [clip.id]);

  // Trackpad / DAW-style navigation over the roll: pinch (ctrl/⌘ + wheel) zooms toward the
  // cursor; a plain two-finger horizontal swipe pans natively via the overflow-x container.
  React.useEffect(() => {
    const sc = gridScrollRef.current; if (!sc) return;
    const onWheel = (e) => {
      if (!(e.ctrlKey || e.metaKey)) return; // leave normal scroll/pan alone
      e.preventDefault();
      const rect = sc.getBoundingClientRect();
      const frac = sc.scrollWidth ? (e.clientX - rect.left + sc.scrollLeft) / sc.scrollWidth : 0;
      setZoom((z) => {
        const nz = Math.max(1, Math.min(3, +(z * (1 - e.deltaY * 0.0026)).toFixed(3)));
        requestAnimationFrame(() => { sc.scrollLeft = Math.max(0, sc.scrollWidth * frac - (e.clientX - rect.left)); });
        return nz;
      });
    };
    sc.addEventListener('wheel', onWheel, { passive: false });
    return () => sc.removeEventListener('wheel', onWheel);
  }, []);
  const tk = gridTokensD(gridStyle);
  const notes = resolved.notes;
  const win = pitchWindowD(notes, 2, 12);
  const rows = win.top - win.bottom + 1;
  const totalSteps = resolved.bars * STEPS_D;
  const kbW = 48;
  const rowPct = 100 / rows;            // each pitch row, % of height — always fills, no blank space
  const stepPct = 100 / totalSteps;     // each 1/16 step, % of (zoomed) width
  const innerW = (zoom * 100) + '%';    // fills width at zoom 1; overflows + scrolls when zoomed in

  const gNotes = React.useMemo(() => applyGroove(notes, knobs), [notes, knobs]);

  const origById = React.useMemo(() => Object.fromEntries(clip.notes.map((n) => [n.id, n])), [clip]);

  const onRollScroll = (e) => {
    if (velScrollRef.current) velScrollRef.current.scrollLeft = e.currentTarget.scrollLeft;
  };

  const beginDrag = (e, note) => {
    e.preventDefault(); e.stopPropagation();
    const sc = gridScrollRef.current;
    const stepW = (sc ? sc.scrollWidth : 600) / totalSteps;
    const rowH = (sc ? sc.clientHeight : 240) / rows;
    const sx = e.clientX, sy = e.clientY;
    const additive = e.shiftKey;
    // dragging a note that's part of a multi-selection moves the whole selection
    const movingIds = (selectedNotes.has(note.id) && selectedNotes.size > 1) ? [...selectedNotes] : [note.id];
    let moved = false;
    const state = { ids: movingIds, dPitch: 0, dStep: 0 };
    setDrag(state);
    const move = (ev) => {
      const dStep = Math.round(((ev.clientX - sx) / stepW) / snap) * snap;
      const dPitch = -Math.round((ev.clientY - sy) / rowH);
      if (dStep !== state.dStep || dPitch !== state.dPitch) {
        if (dStep || dPitch) moved = true;
        state.dStep = dStep; state.dPitch = dPitch;
        setDrag({ ids: movingIds, dPitch, dStep });
      }
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      if (moved && (state.dStep || state.dPitch)) {
        movingIds.forEach((id) => onMoveNote(id, state.dPitch, state.dStep));
      } else {
        // a click (no drag) selects — shift toggles within the current selection
        setSelectedNotes((prev) => {
          if (additive) { const s = new Set(prev); s.has(note.id) ? s.delete(note.id) : s.add(note.id); return s; }
          return new Set([note.id]);
        });
      }
      setDrag(null);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  };

  // rubber-band selection over the grid background
  const beginMarquee = (e) => {
    if (e.button !== 0) return;
    const inner = gridInnerRef.current; if (!inner) return;
    e.preventDefault();
    const rect = inner.getBoundingClientRect();
    const x0 = e.clientX - rect.left, y0 = e.clientY - rect.top;
    const additive = e.shiftKey;
    const base = additive ? new Set(selectedNotes) : new Set();
    setSelectedNotes(base);
    setMarquee({ x0, y0, x1: x0, y1: y0 });
    const move = (ev) => {
      const x1 = ev.clientX - rect.left, y1 = ev.clientY - rect.top;
      setMarquee({ x0, y0, x1, y1 });
      const left = Math.min(x0, x1), right = Math.max(x0, x1), top = Math.min(y0, y1), bot = Math.max(y0, y1);
      const w = rect.width, h = rect.height;
      const sel = new Set(base);
      for (const n of gNotes) {
        const nx = (n.start / totalSteps) * w, nw = Math.max(3, (n.len / totalSteps) * w);
        const i = win.top - n.pitch; const ny = (i / rows) * h, nh = (1 / rows) * h;
        if (nx < right && nx + nw > left && ny < bot && ny + nh > top) sel.add(n.id);
      }
      setSelectedNotes(sel);
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      setMarquee(null);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  };

  // clicking a piano key selects every note on that pitch
  const selectPitch = (pitch, additive) => {
    const ids = gNotes.filter((n) => n.pitch === pitch).map((n) => n.id);
    setSelectedNotes((prev) => {
      const baseSet = additive ? new Set(prev) : new Set();
      ids.forEach((id) => baseSet.add(id));
      return baseSet;
    });
  };

  const snapOpts = [{ value: 16, label: '1 bar' }, { value: 8, label: '1/2' }, { value: 4, label: '1/4' }, { value: 2, label: '1/8' }, { value: 1, label: '1/16' }];
  const velInner = 60;

  return (
    <div style={{ flex: 1, minWidth: 0, display: 'flex', flexDirection: 'column', background: 'var(--mb-panel2)', borderLeft: '1px solid var(--mb-line)' }}>
      {/* toolbar */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '0 8px 0 12px', height: 44, borderBottom: '1px solid var(--mb-line)', flex: '0 0 auto', overflow: 'hidden' }}>
        <MBIcon name="piano-keys" weight="fill" size={15} style={{ color: 'var(--mb-text2)', flex: '0 0 auto' }} />
        <div style={{ minWidth: 0, flex: '0 1 auto', display: 'flex', flexDirection: 'column', lineHeight: 1.15 }}>
          <span style={{ fontSize: 12.5, fontWeight: 600, color: 'var(--mb-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{clip.name}</span>
          <span style={{ fontSize: 10.5, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', whiteSpace: 'nowrap' }}>{resolved.bars} bars · {notes.length} notes</span>
        </div>
        <div style={{ flex: 1, minWidth: 8 }} />
        <Btn icon="scissors" variant={isTrimmed ? 'accent' : 'soft'} active={isTrimmed} onClick={onTrim} disabled={!canTrim && !isTrimmed} title={isTrimmed ? 'Restore trimmed bars' : (canTrim ? 'Trim empty leading/trailing bars' : 'No empty bars to trim')} />
        <div style={{ width: 1, height: 22, background: 'var(--mb-line)', flex: '0 0 auto' }} />
        <Picker w={72} value={snap} options={snapOpts} onChange={(v) => setSnap(Number(v))} />
        <div style={{ display: 'flex', gap: 2, flex: '0 0 auto' }}>
          <Btn icon="magnifying-glass-minus" variant="ghost" onClick={() => setZoom((z) => Math.max(1, +(z - 0.25).toFixed(2)))} disabled={zoom <= 1} title="Zoom out" />
          <Btn icon="magnifying-glass-plus" variant="ghost" onClick={() => setZoom((z) => Math.min(2.75, +(z + 0.25).toFixed(2)))} title="Zoom in" />
        </div>
        {selectedNotes.size > 0 && <Badge tone="edit" onClear={() => setSelectedNotes(new Set())} title="Clear selection">{selectedNotes.size} sel</Badge>}
        <div style={{ width: 1, height: 22, background: 'var(--mb-line)', flex: '0 0 auto' }} />
        <Btn icon="arrow-counter-clockwise" variant="ghost" onClick={() => onRevert('all')} disabled={!badges.length} title="Revert all edits" />
      </div>

      {/* instrument strip — always on, with Drag Me */}
      <InstrumentStripD edit={edit} onEdit={onEdit} clip={clip} />

      {/* roll — fixed share of the window: clips never change height, only zoom rescales the notes */}
      <div style={{ flex: '1 1 0', minHeight: 0, display: 'flex', overflow: 'hidden' }}>
        {/* keyboard — fills full height, percentage rows (no vertical scroll) */}
        <div style={{ width: kbW, flex: '0 0 auto', overflow: 'hidden', borderRight: '1px solid var(--mb-line)', background: 'var(--mb-panel)', position: 'relative' }}>
          {Array.from({ length: rows }).map((_, i) => {
            const pitch = win.top - i; const black = isBlackKeyD(pitch); const isC = pitch % 12 === 0;
            const keySel = gNotes.some((n) => n.pitch === pitch && selectedNotes.has(n.id));
            return (
              <div key={i} onClick={(e) => selectPitch(pitch, e.shiftKey)} title={`${pitchNameD(pitch)} — select all notes on this key`}
                style={{ position: 'absolute', top: `${i * rowPct}%`, left: 0, right: 0, height: `${rowPct}%`,
                background: keySel ? 'var(--mb-accent-soft)' : (black ? 'var(--mb-kb-black)' : 'var(--mb-kb-white)'),
                boxShadow: keySel ? 'inset -2.5px 0 0 var(--mb-accent)' : 'none',
                borderBottom: '1px solid var(--mb-roll-bg)', cursor: 'pointer',
                display: 'flex', alignItems: 'center', justifyContent: 'flex-end', paddingRight: 5, overflow: 'hidden' }}>
                {isC && <span style={{ fontSize: 8.5, fontFamily: 'var(--mb-mono)', color: keySel ? 'var(--mb-accent)' : 'var(--mb-text3)', fontWeight: 600 }}>{pitchNameD(pitch)}</span>}
              </div>
            );
          })}
        </div>
        {/* grid — fills height; zoom widens content + scrolls horizontally */}
        <div ref={gridScrollRef} onScroll={onRollScroll} style={{ flex: 1, overflowX: 'auto', overflowY: 'hidden', position: 'relative' }} className="mb-rollscroll">
          <div ref={gridInnerRef} onPointerDown={beginMarquee} style={{ width: innerW, height: '100%', position: 'relative', background: tk.bg, cursor: marquee ? 'crosshair' : 'default' }}>
            {Array.from({ length: rows }).map((_, i) => {
              const pitch = win.top - i; const black = isBlackKeyD(pitch);
              return <div key={i} style={{ position: 'absolute', top: `${i * rowPct}%`, left: 0, width: '100%', height: `${rowPct}%`,
                background: black ? tk.rowShade : 'transparent', borderBottom: `1px solid ${tk.rowLine}` }} />;
            })}
            {Array.from({ length: Math.floor(totalSteps / snap) + 1 }).map((_, k) => {
              const step = k * snap;
              const isBar = step % STEPS_D === 0;
              return <div key={k} style={{ position: 'absolute', top: 0, bottom: 0, left: `${(step / totalSteps) * 100}%`, width: isBar ? 1.5 : 1,
                background: isBar ? tk.barLine : tk.beatLine }} />;
            })}
            {Array.from({ length: resolved.bars }).map((_, b) => (
              <span key={b} style={{ position: 'absolute', top: 2, left: `calc(${(b / resolved.bars) * 100}% + 4px)`, fontSize: 9, fontFamily: 'var(--mb-mono)', color: 'var(--mb-text3)', fontWeight: 600, pointerEvents: 'none' }}>{b + 1}</span>
            ))}
            {gNotes.filter((n) => n.moved).map((n) => {
              const o = origById[n.id]; if (!o) return null;
              const i = win.top - o.pitch;
              if (i < 0 || i >= rows) return null;
              return <div key={'g' + n.id} style={{ position: 'absolute', left: `${o.start * stepPct}%`, top: `calc(${i * rowPct}% + 1px)`, width: `calc(${o.len * stepPct}% - 1px)`, height: `calc(${rowPct}% - 2px)`,
                borderRadius: 3, border: '1px dashed var(--mb-roll-ghost)', boxSizing: 'border-box', pointerEvents: 'none' }} />;
            })}
            {gNotes.map((n) => {
              const isDragging = drag && drag.ids && drag.ids.includes(n.id);
              const dP = isDragging ? drag.dPitch : 0;
              const dS = isDragging ? drag.dStep : 0;
              const i = win.top - (n.pitch + dP);
              const isSel = selectedNotes.has(n.id);
              const op = isDragging ? 1 : (0.4 + 0.55 * noteVelocity(n, knobs));
              return (
                <div key={n.id} onPointerDown={(e) => beginDrag(e, n)} title={`${pitchNameD(n.pitch + dP)} · click to select · drag to move`}
                  style={{ position: 'absolute', left: `${(n.start + dS) * stepPct}%`, top: `calc(${i * rowPct}% + 1px)`, width: `calc(${n.len * stepPct}% - 1px)`, minWidth: 3, height: `calc(${rowPct}% - 2px)`,
                    background: (n.moved || isDragging) ? tk.noteMoved : tk.note, opacity: isSel ? 1 : op, borderRadius: 3, cursor: 'grab', boxSizing: 'border-box',
                    boxShadow: isSel ? '0 0 0 1.5px var(--mb-accent-bright), 0 0 0 3px rgba(0,0,0,.5)' : (isDragging ? '0 3px 10px rgba(0,0,0,.4)' : (tk.noteEdge !== 'transparent' ? `inset 0 0 0 1px ${tk.noteEdge}` : 'none')),
                    zIndex: isSel ? 4 : (isDragging ? 5 : 1), transition: isDragging ? 'none' : 'background .1s, opacity .1s' }} />
              );
            })}
            {marquee && <div style={{ position: 'absolute', left: Math.min(marquee.x0, marquee.x1), top: Math.min(marquee.y0, marquee.y1),
              width: Math.abs(marquee.x1 - marquee.x0), height: Math.abs(marquee.y1 - marquee.y0),
              background: 'var(--mb-accent-soft)', border: '1px solid var(--mb-accent-line)', borderRadius: 2, pointerEvents: 'none', zIndex: 7 }} />}
            {playing && <div style={{ position: 'absolute', top: 0, bottom: 0, left: `${playhead * stepPct}%`, width: 1.5, background: 'var(--mb-playhead)', zIndex: 6, pointerEvents: 'none' }}>
              <div style={{ position: 'absolute', top: 0, left: -3.5, width: 8, height: 8, background: 'var(--mb-playhead)', transform: 'rotate(45deg)' }} />
            </div>}
          </div>
        </div>
      </div>

      {/* velocity lane (foldable) */}
      <div style={{ flex: '0 0 auto', borderTop: '1px solid var(--mb-line)', background: 'var(--mb-panel)' }}>
        <button type="button" onClick={onToggleVel} title={velOpen ? 'Fold velocity lane' : 'Show velocity lane'}
          style={{ display: 'flex', alignItems: 'center', gap: 8, width: '100%', height: 30, padding: '0 12px', border: 0, background: 'transparent', cursor: 'pointer', font: 'inherit' }}>
          <MBIcon name={velOpen ? 'caret-down' : 'caret-up'} weight="bold" size={11} style={{ color: 'var(--mb-text3)', flex: '0 0 auto' }} />
          <MBIcon name="chart-bar" weight="fill" size={13} style={{ color: 'var(--mb-text2)', flex: '0 0 auto' }} />
          <span style={{ fontSize: 11.5, fontWeight: 700, letterSpacing: '0.05em', textTransform: 'uppercase', color: 'var(--mb-text2)' }}>Velocity</span>
          <div style={{ flex: 1 }} />
          <span style={{ fontSize: 11, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', whiteSpace: 'nowrap' }}>Intensity {Math.round(knobs.intensity)}% · Dyn {Math.round(knobs.dynamics)}%</span>
        </button>
        {velOpen && (
          <div style={{ display: 'flex', height: velInner + 14, borderTop: '1px solid var(--mb-line)' }}>
            <div style={{ width: kbW, flex: '0 0 auto', borderRight: '1px solid var(--mb-line)', background: 'var(--mb-panel)', display: 'flex', alignItems: 'flex-end', justifyContent: 'flex-end', padding: '0 6px 6px 0' }}>
              <span style={{ fontSize: 8.5, fontFamily: 'var(--mb-mono)', fontWeight: 600, color: 'var(--mb-text3)' }}>VEL</span>
            </div>
            <div ref={velScrollRef} style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
              <div style={{ width: innerW, height: '100%', position: 'relative', background: 'var(--mb-roll-bg)' }}>
                {Array.from({ length: resolved.bars + 1 }).map((_, b) => (
                  <div key={b} style={{ position: 'absolute', top: 0, bottom: 0, left: `${(b / resolved.bars) * 100}%`, width: 1, background: 'var(--mb-line-strong)' }} />
                ))}
                {gNotes.map((n) => {
                  const v = noteVelocity(n, knobs);
                  const h = Math.max(2, v * velInner);
                  return (
                    <div key={'v' + n.id} title={`vel ${Math.round(v * 127)}`} style={{ position: 'absolute', left: `${n.start * stepPct}%`, bottom: 7, width: `calc(${n.len * stepPct}% - 1px)`, minWidth: 3,
                      height: h, background: n.moved ? 'var(--mb-accent-bright)' : 'var(--mb-accent)', opacity: 0.92, borderRadius: '2px 2px 0 0', transition: 'height .12s' }}>
                      <div style={{ position: 'absolute', top: -3, left: 0, right: 0, height: 3, borderRadius: 2, background: 'var(--mb-accent-bright)' }} />
                    </div>
                  );
                })}
              </div>
            </div>
          </div>
        )}
      </div>

      {/* groove knob panel (foldable) */}
      <KnobsPanel open={knobsOpen} onToggle={onToggleKnobs} knobs={knobs} onKnob={onKnob} onResetAll={onResetKnobs} />
    </div>
  );
}

Object.assign(window, { PianoRollEditorD });
