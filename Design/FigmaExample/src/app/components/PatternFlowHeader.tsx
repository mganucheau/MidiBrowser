import { Circle, Grid3x3, Clock, Scissors, Plus, Minus } from 'lucide-react';

interface PatternFlowHeaderProps {
  isRecording: boolean;
  onRecordToggle: () => void;
  gridSnap: string;
  onGridSnapChange: (value: string) => void;
  sessionLength: string;
  onSessionLengthChange: (value: string) => void;
}

export function PatternFlowHeader({
  isRecording,
  onRecordToggle,
  gridSnap,
  onGridSnapChange,
  sessionLength,
  onSessionLengthChange,
}: PatternFlowHeaderProps) {
  const gridOptions = ['1/4', '1/8', '1/16', '1/32', 'Off'];
  const sessionOptions = ['4 bars', '8 bars', '16 bars', '32 bars'];

  return (
    <div className="h-12 bg-[#1e1e1e] border-b border-[#2d2d2d] px-3 flex items-center justify-between">
      {/* Left: Title & Record */}
      <div className="flex items-center gap-3">
        <h1 className="text-sm font-medium">PatternFlow</h1>

        <button
          onClick={onRecordToggle}
          className={`h-6 w-6 rounded-full flex items-center justify-center transition-all ${
            isRecording
              ? 'bg-red-600 shadow-[0_0_8px_rgba(239,68,68,0.6)] animate-pulse'
              : 'bg-[#252525] border border-[#333] hover:border-[#444]'
          }`}
        >
          <Circle
            className={`size-3 ${isRecording ? 'fill-white text-white' : 'fill-red-500 text-red-500'}`}
          />
        </button>
      </div>

      {/* Right: Global Controls */}
      <div className="flex items-center gap-2">
        {/* Grid Snap */}
        <div className="flex items-center gap-1.5 bg-[#252525] border border-[#333] rounded px-2 h-8">
          <Grid3x3 className="size-3 text-[#999]" />
          <select
            value={gridSnap}
            onChange={(e) => onGridSnapChange(e.target.value)}
            className="bg-transparent text-xs outline-none border-none cursor-pointer text-white"
          >
            {gridOptions.map((opt) => (
              <option key={opt} value={opt} className="bg-[#252525]">
                {opt}
              </option>
            ))}
          </select>
        </div>

        {/* Session Length */}
        <div className="flex items-center gap-1.5 bg-[#252525] border border-[#333] rounded px-2 h-8">
          <Clock className="size-3 text-[#999]" />
          <select
            value={sessionLength}
            onChange={(e) => onSessionLengthChange(e.target.value)}
            className="bg-transparent text-xs outline-none border-none cursor-pointer text-white"
          >
            {sessionOptions.map((opt) => (
              <option key={opt} value={opt} className="bg-[#252525]">
                {opt}
              </option>
            ))}
          </select>
        </div>

        {/* Divider */}
        <div className="w-px h-6 bg-[#2d2d2d] mx-1" />

        {/* Step Button */}
        <button className="h-8 px-3 bg-transparent hover:bg-[#2a2a2a] rounded text-xs transition-colors flex items-center gap-1.5">
          <Plus className="size-3" />
          <span>Step</span>
        </button>

        {/* Extend Button */}
        <button className="h-8 px-3 bg-transparent hover:bg-[#2a2a2a] rounded text-xs transition-colors flex items-center gap-1.5">
          <Plus className="size-3" />
          <span>Extend</span>
        </button>

        {/* Trim Button */}
        <button className="h-8 px-3 bg-transparent hover:bg-[#2a2a2a] rounded text-xs transition-colors flex items-center gap-1.5">
          <Scissors className="size-3" />
          <span>Trim</span>
        </button>
      </div>
    </div>
  );
}
