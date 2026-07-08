// knob.jsx — rotary Knob + folding "Groove" KnobsPanel (Swing / Humanize / Length / Intensity).

function Knob({ label, value, min, max, def, unit = '%', onChange, size = 48 }) {
  const norm = Math.max(0, Math.min(1, (value - min) / (max - min)));
  const START = -135, END = 135;
  const angle = START + norm * (END - START);
  const r = size / 2 - 5;
  const cx = size / 2, cy = size / 2;
  const pt = (deg, rr = r) => { const a = (deg - 90) * Math.PI / 180; return [cx + rr * Math.cos(a), cy + rr * Math.sin(a)]; };
  const [sx, sy] = pt(START), [ex, ey] = pt(END);
  const [px, py] = pt(angle, r - 3.5);
  const zeroNorm = min < 0 ? (0 - min) / (max - min) : 0;     // bipolar knobs fill from center
  const zeroAngle = START + zeroNorm * (END - START);
  const loA = Math.min(zeroAngle, angle), hiA = Math.max(zeroAngle, angle);
  const [lx, ly] = pt(loA), [hx, hy] = pt(hiA);
  const arc = (x1, y1, x2, y2, large) => `M ${x1.toFixed(2)} ${y1.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;

  const begin = (e) => {
    e.preventDefault();
    const sY = e.clientY, sV = value, range = max - min;
    document.body.style.cursor = 'ns-resize';
    const move = (ev) => {
      const dy = sY - ev.clientY;
      let nv = sV + (dy / 170) * range;
      onChange(Math.round(Math.max(min, Math.min(max, nv))));
    };
    const up = () => { document.body.style.cursor = ''; window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', up); };
    window.addEventListener('pointermove', move); window.addEventListener('pointerup', up);
  };

  const active = value !== def;
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 7, userSelect: 'none', flex: '0 0 auto' }}>
      <div onPointerDown={begin} onDoubleClick={() => onChange(def)} title={`${label} — drag up/down, double-click to reset`}
        style={{ width: size, height: size, cursor: 'ns-resize', position: 'relative', touchAction: 'none' }}>
        <svg width={size} height={size} style={{ display: 'block', overflow: 'visible' }}>
          <path d={arc(sx, sy, ex, ey, 1)} fill="none" stroke="var(--mb-line-strong)" strokeWidth="3" strokeLinecap="round" />
          {Math.abs(angle - zeroAngle) > 0.6 && <path d={arc(lx, ly, hx, hy, (hiA - loA) > 180 ? 1 : 0)} fill="none" stroke="var(--mb-accent)" strokeWidth="3" strokeLinecap="round" />}
          <circle cx={cx} cy={cy} r={r - 3} fill="var(--mb-elev)" stroke="var(--mb-line)" strokeWidth="1" />
          <line x1={cx} y1={cy} x2={px} y2={py} stroke="var(--mb-accent-bright)" strokeWidth="2" strokeLinecap="round" />
          <circle cx={cx} cy={cy} r="1.6" fill="var(--mb-text3)" />
        </svg>
      </div>
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 1 }}>
        <span style={{ fontSize: 10, fontWeight: 600, letterSpacing: '0.05em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>{label}</span>
        <span style={{ fontFamily: 'var(--mb-mono)', fontSize: 12, fontWeight: 600, color: active ? 'var(--mb-accent)' : 'var(--mb-text2)', fontVariantNumeric: 'tabular-nums' }}>{Math.round(value)}{unit}</span>
      </div>
    </div>
  );
}

const KNOB_DEFS = [
  { key: 'swing', label: 'Swing', min: 0, max: 75, def: 0 },
  { key: 'pocket', label: 'Pocket', min: -100, max: 100, def: 0 },
  { key: 'humanize', label: 'Humanize', min: 0, max: 100, def: 0 },
  { key: 'dynamics', label: 'Dynamics', min: 0, max: 100, def: 0 },
  { key: 'length', label: 'Length', min: 25, max: 200, def: 100 },
  { key: 'intensity', label: 'Intensity', min: 0, max: 100, def: 80 },
];
const DEFAULT_KNOBS = { swing: 0, pocket: 0, humanize: 0, dynamics: 0, length: 100, intensity: 80 };

function KnobsPanel({ open, onToggle, knobs, onKnob, onResetAll }) {
  const activeCount = KNOB_DEFS.filter((d) => knobs[d.key] !== d.def).length;
  return (
    <div style={{ flex: '0 0 auto', borderTop: '1px solid var(--mb-line)', background: 'var(--mb-panel)' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, height: 32, padding: '0 8px 0 10px' }}>
        <button type="button" onClick={onToggle} title={open ? 'Fold groove panel' : 'Show groove panel'}
          style={{ display: 'flex', alignItems: 'center', gap: 8, flex: 1, minWidth: 0, height: '100%', border: 0, background: 'transparent', cursor: 'pointer', font: 'inherit', padding: 0 }}>
          <MBIcon name={open ? 'caret-down' : 'caret-up'} weight="bold" size={11} style={{ color: 'var(--mb-text3)', flex: '0 0 auto' }} />
          <MBIcon name="sliders" weight="fill" size={13} style={{ color: 'var(--mb-text2)', flex: '0 0 auto' }} />
          <span style={{ fontSize: 11.5, fontWeight: 700, letterSpacing: '0.05em', textTransform: 'uppercase', color: 'var(--mb-text2)', flex: '0 0 auto' }}>Groove</span>
          {activeCount > 0 && <Badge tone="edit">{activeCount}</Badge>}
          <div style={{ flex: 1 }} />
          {!open && <span style={{ fontSize: 11, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>Swing · Pocket · Humanize · Dynamics · Length · Intensity</span>}
        </button>
        {open && activeCount > 0 && <Btn icon="arrow-counter-clockwise" variant="ghost" onClick={onResetAll} title="Reset groove" />}
      </div>
      {open && (
        <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'flex-start', justifyContent: 'space-around', gap: 6, rowGap: 12, padding: '2px 14px 16px' }}>
          {KNOB_DEFS.map((d) => <Knob key={d.key} {...d} value={knobs[d.key]} onChange={(v) => onKnob(d.key, v)} />)}
        </div>
      )}
    </div>
  );
}

// Apply groove params to notes for rendering (non-destructive display transform).
function applyGroove(notes, k) {
  return notes.map((n) => {
    let start = n.start;
    if (k.swing) { const eighth = Math.floor(n.start / 2); if (eighth % 2 === 1) start += (k.swing / 100) * 1.0; }
    if (k.pocket) { start += (k.pocket / 100) * 1.6; }   // − pushes ahead (tight), + lays back (loose) of the grid
    if (k.humanize) { const r = ((Math.imul(n.id + 1, 2654435761) >>> 0) % 1000) / 1000 * 2 - 1; start += r * (k.humanize / 100) * 0.9; }
    const len = Math.max(0.5, n.len * (k.length / 100));
    return { ...n, start: Math.max(0, start), len };
  });
}

// deterministic base velocity 0.45–1.0 per note
function baseVel(id) { return 0.45 + ((Math.imul(id + 7, 2654435761) >>> 0) % 56) / 100; }
const STEPS_KB = (window.MB_DATA && window.MB_DATA.STEPS_PER_BAR) || 16;

// Effective velocity for a note. Intensity scales the whole performance; Dynamics widens
// the contrast by metric position (accent downbeats, pull back the in-between 16ths).
function noteVelocity(n, k) {
  let v = baseVel(n.id);
  if (k.dynamics) {
    const pos = ((Math.round(n.start) % STEPS_KB) + STEPS_KB) % STEPS_KB;
    let accent;
    if (pos === 0) accent = 1;            // bar downbeat
    else if (pos % 4 === 0) accent = 0.45; // beats 2 / 3 / 4
    else if (pos % 2 === 0) accent = -0.2; // 8th-note offbeats
    else accent = -0.6;                    // in-between 16ths
    v += accent * (k.dynamics / 100) * 0.55;
  }
  v *= (k.intensity / 100);
  return Math.max(0.04, Math.min(1, v));
}

Object.assign(window, { Knob, KnobsPanel, KNOB_DEFS, DEFAULT_KNOBS, applyGroove, baseVel, noteVelocity });
