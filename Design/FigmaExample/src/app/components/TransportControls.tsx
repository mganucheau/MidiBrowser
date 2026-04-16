import { useState } from 'react';
import { 
  SkipBack, 
  Play, 
  Pause, 
  Square, 
  Circle, 
  SkipForward,
  Repeat,
  Volume2
} from 'lucide-react';
import { Button } from './ui/button';
import { Slider } from './ui/slider';

export function TransportControls() {
  const [isPlaying, setIsPlaying] = useState(false);
  const [isRecording, setIsRecording] = useState(false);
  const [bpm, setBpm] = useState(120);
  const [volume, setVolume] = useState([75]);

  return (
    <div className="h-16 bg-[#252525] border-b border-[#2d2d2d] flex items-center px-4 gap-6">
      {/* Transport Buttons */}
      <div className="flex items-center gap-2">
        <Button 
          variant="ghost" 
          size="sm" 
          className="h-9 w-9 p-0 hover:bg-[#333]"
        >
          <SkipBack className="size-5" />
        </Button>
        
        <Button 
          variant="ghost" 
          size="sm" 
          className={`h-11 w-11 p-0 ${isPlaying ? 'bg-[#333]' : 'hover:bg-[#333]'}`}
          onClick={() => setIsPlaying(!isPlaying)}
        >
          {isPlaying ? <Pause className="size-6" /> : <Play className="size-6 ml-0.5" />}
        </Button>
        
        <Button 
          variant="ghost" 
          size="sm" 
          className="h-9 w-9 p-0 hover:bg-[#333]"
          onClick={() => setIsPlaying(false)}
        >
          <Square className="size-5" />
        </Button>

        <Button 
          variant="ghost" 
          size="sm" 
          className={`h-9 w-9 p-0 ${isRecording ? 'bg-red-900/30 hover:bg-red-900/40' : 'hover:bg-[#333]'}`}
          onClick={() => setIsRecording(!isRecording)}
        >
          <Circle className={`size-5 ${isRecording ? 'fill-red-500 text-red-500' : ''}`} />
        </Button>
        
        <Button 
          variant="ghost" 
          size="sm" 
          className="h-9 w-9 p-0 hover:bg-[#333]"
        >
          <SkipForward className="size-5" />
        </Button>

        <Button 
          variant="ghost" 
          size="sm" 
          className="h-9 w-9 p-0 hover:bg-[#333]"
        >
          <Repeat className="size-5" />
        </Button>
      </div>

      {/* Timecode */}
      <div className="flex items-center gap-4">
        <div className="bg-[#1a1a1a] px-4 py-2 rounded border border-[#333] font-mono text-sm">
          00:00:00.000
        </div>
        <div className="text-xs text-gray-400">
          Bar 1 | Beat 1 | Tick 0
        </div>
      </div>

      {/* BPM */}
      <div className="flex items-center gap-2">
        <label className="text-xs text-gray-400">BPM</label>
        <input
          type="number"
          value={bpm}
          onChange={(e) => setBpm(Number(e.target.value))}
          className="bg-[#1a1a1a] px-3 py-1.5 rounded border border-[#333] w-16 text-sm text-center"
        />
      </div>

      <div className="flex-1" />

      {/* Master Volume */}
      <div className="flex items-center gap-3 min-w-[200px]">
        <Volume2 className="size-4 text-gray-400" />
        <Slider 
          value={volume}
          onValueChange={setVolume}
          max={100}
          step={1}
          className="flex-1"
        />
        <span className="text-xs text-gray-400 w-8 text-right">{volume[0]}</span>
      </div>
    </div>
  );
}
