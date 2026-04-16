import { Plus } from 'lucide-react';
import { ScrollArea } from './ui/scroll-area';
import { useState } from 'react';

interface Clip {
  id: string;
  name: string;
  start: number;
  length: number;
  color: string;
}

interface Lane {
  id: string;
  name: string;
  clips: Clip[];
}

interface ArrangementViewProps {
  selectedLane: string | null;
  onLaneSelect: (id: string) => void;
  selectedClip: string | null;
  onClipSelect: (id: string) => void;
}

export function ArrangementView({
  selectedLane,
  onLaneSelect,
  selectedClip,
  onClipSelect,
}: ArrangementViewProps) {
  const [lanes] = useState<Lane[]>([
    {
      id: '1',
      name: 'Lane 1',
      clips: [
        { id: 'c1', name: 'Bass Line', start: 0, length: 4, color: '#45b7d1' },
        { id: 'c2', name: 'Bass Fill', start: 7, length: 1, color: '#45b7d1' },
      ],
    },
    {
      id: '2',
      name: 'Lane 2',
      clips: [{ id: 'c3', name: 'Chord Prog', start: 0, length: 8, color: '#a29bfe' }],
    },
    {
      id: '3',
      name: 'Lane 3',
      clips: [
        { id: 'c4', name: 'Drum 1', start: 0, length: 4, color: '#f9ca24' },
        { id: 'c5', name: 'Drum 2', start: 4, length: 4, color: '#f9ca24' },
      ],
    },
    { id: '4', name: 'Lane 4', clips: [] },
  ]);

  // Render grid lines (8 bars with 4 beats each = 32 divisions)
  const gridLines = Array.from({ length: 33 }, (_, i) => i);

  return (
    <div className="size-full flex flex-col bg-[#1a1a1a]">
      {/* Timeline Ruler */}
      <div className="h-8 bg-[#1a1a1a] border-b border-[#2d2d2d] flex">
        {/* Lane header space */}
        <div className="w-48 border-r border-[#2d2d2d] bg-[#252525]" />

        {/* Timeline markers */}
        <div className="flex-1 relative">
          {Array.from({ length: 9 }, (_, i) => (
            <div
              key={i}
              className="absolute top-0 h-full flex flex-col justify-center border-l border-[#252525]"
              style={{ left: `${(i / 8) * 100}%` }}
            >
              <span className="text-[10px] text-white ml-1">{i}</span>
            </div>
          ))}
        </div>
      </div>

      {/* Lanes Container */}
      <ScrollArea className="flex-1">
        <div className="flex flex-col">
          {lanes.map((lane) => (
            <div
              key={lane.id}
              className={`h-20 flex border-b border-[#2d2d2d] transition-colors ${
                selectedLane === lane.id ? 'bg-[#1e1e1e]' : 'bg-[#1a1a1a]'
              }`}
              onClick={() => onLaneSelect(lane.id)}
            >
              {/* Lane Header */}
              <div className="w-48 bg-[#252525] border-r border-[#2d2d2d] p-2 flex flex-col justify-between">
                <div className="flex items-center justify-between">
                  <span className="text-xs font-medium">{lane.name}</span>
                  <button className="h-5 w-5 rounded hover:bg-[#2a2a2a] flex items-center justify-center">
                    <Plus className="size-3" />
                  </button>
                </div>
                <div className="text-[9px] text-[#999]">{lane.clips.length} clips</div>
              </div>

              {/* Lane Content with Grid */}
              <div className="flex-1 relative">
                {/* Grid lines */}
                {gridLines.map((i) => (
                  <div
                    key={i}
                    className="absolute top-0 h-full w-px bg-[#252525]"
                    style={{ left: `${(i / 32) * 100}%` }}
                  />
                ))}

                {/* Clips */}
                {lane.clips.map((clip) => (
                  <div
                    key={clip.id}
                    onClick={(e) => {
                      e.stopPropagation();
                      onClipSelect(clip.id);
                    }}
                    className={`absolute top-2 bottom-2 rounded overflow-hidden cursor-pointer transition-all ${
                      selectedClip === clip.id ? 'ring-2 ring-blue-400' : ''
                    }`}
                    style={{
                      left: `${(clip.start / 32) * 100}%`,
                      width: `${(clip.length / 32) * 100}%`,
                      background: `linear-gradient(to bottom right, ${clip.color}, ${clip.color}88)`,
                      boxShadow:
                        'inset 0 1px 2px rgba(255, 255, 255, 0.1), inset 0 -1px 2px rgba(0, 0, 0, 0.2)',
                    }}
                  >
                    <div className="px-2 py-1">
                      <div className="text-[10px] font-medium text-white">{clip.name}</div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          ))}

          {/* Add Lane Button */}
          <div className="h-12 flex items-center justify-center border-b border-[#2d2d2d] hover:bg-[#1e1e1e] cursor-pointer transition-colors">
            <Plus className="size-4 text-[#999]" />
            <span className="text-xs text-[#999] ml-2">Add Lane</span>
          </div>
        </div>
      </ScrollArea>
    </div>
  );
}
