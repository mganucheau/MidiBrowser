import { Infinity, RotateCcw } from 'lucide-react';
import { RotaryKnob } from './RotaryKnob';

interface ControlStripProps {
  compEnabled: boolean;
  onCompEnabledChange: (value: boolean) => void;
  compValue: number;
  onCompValueChange: (value: number) => void;
  transposeEnabled: boolean;
  onTransposeEnabledChange: (value: boolean) => void;
  keyValue: string;
  onKeyChange: (value: string) => void;
  scaleValue: string;
  onScaleChange: (value: string) => void;
  octaveValue: string;
  onOctaveChange: (value: string) => void;
  loopStart: number;
  onLoopStartChange: (value: number) => void;
  loopSync: boolean;
  onLoopSyncChange: (value: boolean) => void;
  loopEnd: number;
  onLoopEndChange: (value: number) => void;
}

export function ControlStrip({
  compEnabled,
  onCompEnabledChange,
  compValue,
  onCompValueChange,
  transposeEnabled,
  onTransposeEnabledChange,
  keyValue,
  onKeyChange,
  scaleValue,
  onScaleChange,
  octaveValue,
  onOctaveChange,
  loopStart,
  onLoopStartChange,
  loopSync,
  onLoopSyncChange,
  loopEnd,
  onLoopEndChange,
}: ControlStripProps) {
  const keys = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const scales = ['Major', 'Minor', 'Dorian', 'Phrygian', 'Lydian', 'Mixolydian', 'Locrian'];
  const octaves = ['0', '1', '2', '3', '4', '5', '6', '7', '8'];

  return (
    <div className="h-16 bg-[#1e1e1e] border-b border-[#2d2d2d] flex">
      {/* Column 1: Comping */}
      <div className="flex-1 flex items-center justify-center gap-3 px-4 border-r border-[#2d2d2d]">
        <button
          onClick={() => onCompEnabledChange(!compEnabled)}
          className={`h-7 px-3 rounded text-xs transition-all ${
            compEnabled
              ? 'bg-blue-900/30 text-blue-400 border border-blue-500/50'
              : 'bg-[#252525] border border-[#333] hover:border-[#444]'
          }`}
        >
          Comp
        </button>

        <RotaryKnob
          value={compValue}
          min={1}
          max={8}
          onChange={onCompValueChange}
          label="Comp"
          size={32}
        />

        <button className="h-7 px-3 bg-[#252525] hover:bg-[#2a2a2a] border border-[#333] hover:border-[#444] rounded text-xs transition-colors">
          Random
        </button>

        <button className="h-7 px-3 bg-[#252525] hover:bg-[#2a2a2a] border border-[#333] hover:border-[#444] rounded text-xs transition-colors flex items-center gap-1.5">
          <RotateCcw className="size-3" />
          <span>Swap</span>
        </button>
      </div>

      {/* Column 2: Pitch/Scale */}
      <div className="flex-1 flex items-center justify-center gap-2 px-4 border-r border-[#2d2d2d]">
        <button
          onClick={() => onTransposeEnabledChange(!transposeEnabled)}
          className={`h-7 px-3 rounded text-xs transition-all ${
            transposeEnabled
              ? 'bg-blue-900/30 text-blue-400 border border-blue-500/50'
              : 'bg-[#252525] border border-[#333] hover:border-[#444]'
          }`}
        >
          Transpose
        </button>

        <select
          value={keyValue}
          onChange={(e) => onKeyChange(e.target.value)}
          className="h-7 px-2 bg-[#252525] border border-[#333] hover:border-[#444] rounded text-xs outline-none cursor-pointer"
        >
          {keys.map((k) => (
            <option key={k} value={k} className="bg-[#252525]">
              {k}
            </option>
          ))}
        </select>

        <select
          value={scaleValue}
          onChange={(e) => onScaleChange(e.target.value)}
          className="h-7 px-2 bg-[#252525] border border-[#333] hover:border-[#444] rounded text-xs outline-none cursor-pointer"
        >
          {scales.map((s) => (
            <option key={s} value={s} className="bg-[#252525]">
              {s}
            </option>
          ))}
        </select>

        <select
          value={octaveValue}
          onChange={(e) => onOctaveChange(e.target.value)}
          className="h-7 px-2 bg-[#252525] border border-[#333] hover:border-[#444] rounded text-xs outline-none cursor-pointer"
        >
          {octaves.map((o) => (
            <option key={o} value={o} className="bg-[#252525]">
              Oct {o}
            </option>
          ))}
        </select>

        <button className="h-7 w-10 bg-[#252525] hover:bg-[#2a2a2a] border border-[#333] hover:border-[#444] rounded text-xs transition-colors">
          C0
        </button>
      </div>

      {/* Column 3: Loop Controls */}
      <div className="flex-1 flex items-center justify-center gap-3 px-4">
        <RotaryKnob
          value={loopStart}
          min={0}
          max={32}
          onChange={onLoopStartChange}
          label="Loop Start"
          size={32}
        />

        <button
          onClick={() => onLoopSyncChange(!loopSync)}
          className={`h-8 w-8 rounded flex items-center justify-center transition-all ${
            loopSync
              ? 'bg-blue-900/30 text-blue-400 border border-blue-500/50'
              : 'bg-[#252525] border border-[#333] hover:border-[#444]'
          }`}
        >
          <Infinity className="size-4" />
        </button>

        <RotaryKnob
          value={loopEnd}
          min={0}
          max={32}
          onChange={onLoopEndChange}
          label="Loop End"
          size={32}
        />
      </div>
    </div>
  );
}
