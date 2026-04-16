import { Music2, Maximize2 } from 'lucide-react';
import { ScrollArea } from './ui/scroll-area';
import { useEffect, useRef } from 'react';

interface PianoRollProps {
  selectedClip: string | null;
}

const notes = ['C', 'B', 'A#', 'A', 'G#', 'G', 'F#', 'F', 'E', 'D#', 'D', 'C#'];
const octaves = [5, 4, 3, 2, 1];

export function PianoRoll({ selectedClip }: PianoRollProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  // Generate piano roll grid and example notes
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;

    // Clear canvas
    ctx.fillStyle = '#1a1a1a';
    ctx.fillRect(0, 0, width, height);

    const noteHeight = 12;
    const totalNotes = notes.length * octaves.length;
    const gridWidth = 32; // 32 divisions (8 bars × 4 beats)

    // Draw horizontal lines (notes)
    ctx.strokeStyle = '#2d2d2d';
    ctx.lineWidth = 1;
    for (let i = 0; i <= totalNotes; i++) {
      const y = i * noteHeight;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }

    // Draw vertical grid lines
    for (let i = 0; i <= gridWidth; i++) {
      const x = (i / gridWidth) * width;
      ctx.strokeStyle = i % 4 === 0 ? '#333' : '#252525';
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, height);
      ctx.stroke();
    }

    // Draw example notes if clip is selected
    if (selectedClip) {
      const exampleNotes = [
        { note: 24, start: 0, length: 2, velocity: 0.8 },
        { note: 28, start: 2, length: 2, velocity: 0.7 },
        { note: 31, start: 4, length: 2, velocity: 0.9 },
        { note: 28, start: 6, length: 1, velocity: 0.6 },
        { note: 24, start: 7, length: 1, velocity: 0.8 },
      ];

      exampleNotes.forEach((n) => {
        const x = (n.start / gridWidth) * width;
        const noteWidth = (n.length / gridWidth) * width;
        const y = n.note * noteHeight;

        // Note gradient
        const gradient = ctx.createLinearGradient(x, y, x + noteWidth, y + noteHeight);
        gradient.addColorStop(0, `rgba(96, 165, 250, ${n.velocity})`);
        gradient.addColorStop(1, `rgba(59, 130, 246, ${n.velocity * 0.7})`);

        ctx.fillStyle = gradient;
        ctx.fillRect(x + 1, y + 1, noteWidth - 2, noteHeight - 2);

        // Note border
        ctx.strokeStyle = 'rgba(96, 165, 250, 1)';
        ctx.lineWidth = 1;
        ctx.strokeRect(x + 1, y + 1, noteWidth - 2, noteHeight - 2);
      });
    }
  }, [selectedClip]);

  return (
    <div className="size-full flex flex-col bg-[#1a1a1a]">
      {/* Piano Roll Header */}
      <div className="h-10 bg-[#252525] border-b border-[#2d2d2d] px-3 flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Music2 className="size-3 text-[#999]" />
          <span className="text-xs font-medium">Piano Roll</span>
          {selectedClip && (
            <span className="text-[10px] text-[#999]">
              - Editing: {selectedClip}
            </span>
          )}
        </div>
        <button className="h-6 w-6 rounded hover:bg-[#2a2a2a] flex items-center justify-center transition-colors">
          <Maximize2 className="size-3" />
        </button>
      </div>

      {/* Piano Roll Content */}
      <div className="flex-1 flex overflow-hidden">
        {/* Piano Keys */}
        <div className="w-12 bg-[#252525] border-r border-[#2d2d2d] flex flex-col">
          {octaves.map((octave) =>
            notes.map((note, idx) => {
              const isBlackKey = note.includes('#');
              return (
                <div
                  key={`${note}${octave}`}
                  className={`h-3 border-b border-[#2d2d2d] flex items-center justify-end px-1 ${
                    isBlackKey ? 'bg-[#1a1a1a]' : 'bg-[#252525]'
                  }`}
                >
                  <span className="text-[9px] text-[#999]">
                    {note}
                    {idx === 0 ? octave : ''}
                  </span>
                </div>
              );
            })
          )}
        </div>

        {/* Piano Roll Grid */}
        <ScrollArea className="flex-1">
          <div className="relative">
            <canvas
              ref={canvasRef}
              width={1280}
              height={720}
              className="block"
              style={{ imageRendering: 'crisp-edges' }}
            />
            {!selectedClip && (
              <div className="absolute inset-0 flex items-center justify-center">
                <div className="text-center">
                  <Music2 className="size-8 text-[#333] mx-auto mb-2" />
                  <p className="text-xs text-[#999]">Select a clip to edit</p>
                </div>
              </div>
            )}
          </div>
        </ScrollArea>
      </div>
    </div>
  );
}
