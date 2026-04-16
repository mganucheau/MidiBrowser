import { useState } from 'react';
import { Volume2, Sliders } from 'lucide-react';
import { Slider } from './ui/slider';
import { Button } from './ui/button';

interface MixerChannelProps {
  name: string;
  color: string;
  volume: number;
  pan: number;
  onVolumeChange: (value: number) => void;
  onPanChange: (value: number) => void;
}

function MixerChannel({ name, color, volume, pan, onVolumeChange, onPanChange }: MixerChannelProps) {
  const [isMuted, setIsMuted] = useState(false);
  const [isSolo, setIsSolo] = useState(false);

  return (
    <div className="w-20 bg-[#252525] border-r border-[#2d2d2d] p-2 flex flex-col items-center gap-3">
      {/* VU Meter */}
      <div className="w-8 h-32 bg-[#1a1a1a] rounded border border-[#333] relative overflow-hidden">
        <div 
          className="absolute bottom-0 w-full transition-all duration-75"
          style={{
            height: `${volume}%`,
            background: `linear-gradient(to top, ${color}, ${color}88)`
          }}
        />
        {/* Scale markers */}
        <div className="absolute inset-0 flex flex-col justify-between py-1">
          {[0, -6, -12, -18, -24, -30].map((db) => (
            <div key={db} className="w-full h-px bg-[#333]" />
          ))}
        </div>
      </div>

      {/* Pan */}
      <div className="w-full">
        <div className="text-[9px] text-gray-400 text-center mb-1">PAN</div>
        <div className="h-6 flex items-center">
          <Slider
            value={[pan]}
            onValueChange={(v) => onPanChange(v[0])}
            min={-50}
            max={50}
            step={1}
            orientation="horizontal"
            className="w-full"
          />
        </div>
        <div className="text-[9px] text-gray-400 text-center mt-0.5">
          {pan > 0 ? `R${pan}` : pan < 0 ? `L${Math.abs(pan)}` : 'C'}
        </div>
      </div>

      {/* Fader */}
      <div className="flex-1 flex flex-col items-center gap-2 py-2">
        <Slider
          value={[volume]}
          onValueChange={(v) => onVolumeChange(v[0])}
          max={100}
          step={1}
          orientation="vertical"
          className="h-full"
        />
        <div className="text-[10px] text-gray-400">{volume}</div>
      </div>

      {/* Mute/Solo */}
      <div className="flex gap-1 w-full">
        <Button
          variant="ghost"
          size="sm"
          className={`h-6 flex-1 text-[10px] ${isMuted ? 'bg-yellow-900/30 text-yellow-500' : ''}`}
          onClick={() => setIsMuted(!isMuted)}
        >
          M
        </Button>
        <Button
          variant="ghost"
          size="sm"
          className={`h-6 flex-1 text-[10px] ${isSolo ? 'bg-blue-900/30 text-blue-500' : ''}`}
          onClick={() => setIsSolo(!isSolo)}
        >
          S
        </Button>
      </div>

      {/* Track Name */}
      <div className="text-[10px] text-center text-gray-400 truncate w-full" title={name}>
        {name}
      </div>
    </div>
  );
}

export function Mixer() {
  const [tracks] = useState([
    { id: '1', name: 'Vocals', color: '#ff6b6b', volume: 75, pan: 0 },
    { id: '2', name: 'Guitar', color: '#4ecdc4', volume: 65, pan: -20 },
    { id: '3', name: 'Bass', color: '#45b7d1', volume: 70, pan: 0 },
    { id: '4', name: 'Drums', color: '#f9ca24', volume: 80, pan: 0 },
    { id: '5', name: 'Keys', color: '#a29bfe', volume: 60, pan: 25 }
  ]);

  return (
    <div className="w-[440px] bg-[#1e1e1e] border-l border-[#2d2d2d] flex flex-col">
      {/* Mixer Header */}
      <div className="h-10 bg-[#252525] border-b border-[#2d2d2d] flex items-center px-3 gap-2">
        <Sliders className="size-4" />
        <span className="text-sm">Mixer</span>
      </div>

      {/* Channels */}
      <div className="flex-1 flex overflow-x-auto">
        {tracks.map((track) => (
          <MixerChannel
            key={track.id}
            name={track.name}
            color={track.color}
            volume={track.volume}
            pan={track.pan}
            onVolumeChange={(v) => console.log(`${track.name} volume:`, v)}
            onPanChange={(v) => console.log(`${track.name} pan:`, v)}
          />
        ))}
        
        {/* Master Channel */}
        <div className="w-24 bg-[#2a2a2a] border-l-2 border-blue-500/30 p-2 flex flex-col items-center gap-3">
          <div className="w-10 h-32 bg-[#1a1a1a] rounded border border-blue-500/30 relative overflow-hidden">
            <div 
              className="absolute bottom-0 w-full transition-all duration-75"
              style={{
                height: '75%',
                background: 'linear-gradient(to top, #3b82f6, #3b82f688)'
              }}
            />
          </div>
          
          <div className="flex-1 flex flex-col items-center gap-2 py-2">
            <Slider
              value={[75]}
              max={100}
              step={1}
              orientation="vertical"
              className="h-full"
            />
            <div className="text-[10px] text-gray-400">75</div>
          </div>

          <div className="text-xs text-center text-blue-400">
            MASTER
          </div>
        </div>
      </div>
    </div>
  );
}
