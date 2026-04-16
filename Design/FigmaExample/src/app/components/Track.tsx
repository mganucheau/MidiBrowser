import { useEffect, useRef, useState } from 'react';
import { Volume2, Lock, Eye, Circle } from 'lucide-react';
import { Button } from './ui/button';
import { Slider } from './ui/slider';

interface TrackProps {
  id: string;
  name: string;
  color: string;
  volume: number;
  pan: number;
  muted?: boolean;
  solo?: boolean;
  hasAudio?: boolean;
  onVolumeChange?: (value: number) => void;
  onPanChange?: (value: number) => void;
}

export function Track({
  id,
  name,
  color,
  volume,
  pan,
  muted = false,
  solo = false,
  hasAudio = false,
  onVolumeChange,
  onPanChange
}: TrackProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isMuted, setIsMuted] = useState(muted);
  const [isSolo, setIsSolo] = useState(solo);
  const [isRecordArmed, setIsRecordArmed] = useState(false);

  useEffect(() => {
    if (!hasAudio || !canvasRef.current) return;

    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    
    ctx.scale(dpr, dpr);

    // Draw waveform
    ctx.fillStyle = '#1e1e1e';
    ctx.fillRect(0, 0, rect.width, rect.height);

    const centerY = rect.height / 2;
    const samples = 1000;
    
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();

    for (let i = 0; i < samples; i++) {
      const x = (i / samples) * rect.width;
      const amplitude = Math.sin(i / 20) * Math.random() * 0.8;
      const y = centerY + amplitude * (rect.height / 2);
      
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    
    ctx.stroke();

    // Draw center line
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, centerY);
    ctx.lineTo(rect.width, centerY);
    ctx.stroke();

  }, [hasAudio, color]);

  return (
    <div className="h-24 border-b border-[#2d2d2d] flex">
      {/* Track Header */}
      <div className="w-56 bg-[#252525] border-r border-[#2d2d2d] p-2 flex flex-col gap-1.5">
        <div className="flex items-center gap-1.5">
          <Button
            variant="ghost"
            size="sm"
            className={`h-6 w-6 p-0 ${isRecordArmed ? 'bg-red-900/30' : ''}`}
            onClick={() => setIsRecordArmed(!isRecordArmed)}
          >
            <Circle className={`size-3 ${isRecordArmed ? 'fill-red-500 text-red-500' : ''}`} />
          </Button>
          <Button
            variant="ghost"
            size="sm"
            className={`h-6 px-2 text-xs ${isMuted ? 'bg-yellow-900/30 text-yellow-500' : ''}`}
            onClick={() => setIsMuted(!isMuted)}
          >
            M
          </Button>
          <Button
            variant="ghost"
            size="sm"
            className={`h-6 px-2 text-xs ${isSolo ? 'bg-blue-900/30 text-blue-500' : ''}`}
            onClick={() => setIsSolo(!isSolo)}
          >
            S
          </Button>
          <input
            type="text"
            defaultValue={name}
            className="flex-1 bg-[#1a1a1a] px-2 py-1 rounded text-xs border border-[#333] focus:outline-none focus:border-[#444]"
          />
        </div>

        <div className="flex items-center gap-2">
          <Volume2 className="size-3 text-gray-400" />
          <Slider
            value={[volume]}
            onValueChange={(v) => onVolumeChange?.(v[0])}
            max={100}
            step={1}
            className="flex-1"
          />
          <span className="text-[10px] text-gray-400 w-6 text-right">{volume}</span>
        </div>

        <div className="flex items-center gap-2">
          <span className="text-[10px] text-gray-400 w-3">Pan</span>
          <Slider
            value={[pan]}
            onValueChange={(v) => onPanChange?.(v[0])}
            min={-50}
            max={50}
            step={1}
            className="flex-1"
          />
          <span className="text-[10px] text-gray-400 w-6 text-right">{pan > 0 ? 'R' : pan < 0 ? 'L' : 'C'}</span>
        </div>
      </div>

      {/* Track Content */}
      <div className="flex-1 bg-[#1e1e1e] relative overflow-hidden">
        {hasAudio ? (
          <canvas ref={canvasRef} className="w-full h-full" />
        ) : (
          <div className="w-full h-full flex items-center justify-center text-xs text-gray-600">
            Empty track
          </div>
        )}
      </div>
    </div>
  );
}
