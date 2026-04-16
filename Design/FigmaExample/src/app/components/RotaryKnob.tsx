import { useRef, useState, useEffect } from 'react';

interface RotaryKnobProps {
  value: number;
  min: number;
  max: number;
  onChange: (value: number) => void;
  label?: string;
  size?: number;
}

export function RotaryKnob({ value, min, max, onChange, label, size = 40 }: RotaryKnobProps) {
  const knobRef = useRef<HTMLDivElement>(null);
  const [isDragging, setIsDragging] = useState(false);
  const startY = useRef(0);
  const startValue = useRef(0);

  // Calculate rotation angle (-135deg to 135deg range)
  const normalized = (value - min) / (max - min);
  const rotation = -135 + normalized * 270;

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      if (!isDragging) return;

      const delta = startY.current - e.clientY;
      const sensitivity = 0.5;
      const newValue = Math.max(min, Math.min(max, startValue.current + delta * sensitivity));
      onChange(Math.round(newValue));
    };

    const handleMouseUp = () => {
      setIsDragging(false);
    };

    if (isDragging) {
      document.addEventListener('mousemove', handleMouseMove);
      document.addEventListener('mouseup', handleMouseUp);
    }

    return () => {
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isDragging, min, max, onChange]);

  const handleMouseDown = (e: React.MouseEvent) => {
    setIsDragging(true);
    startY.current = e.clientY;
    startValue.current = value;
  };

  return (
    <div className="flex flex-col items-center gap-1">
      <div
        ref={knobRef}
        onMouseDown={handleMouseDown}
        className="relative rounded-full bg-[#1a1a1a] border border-[#333] cursor-pointer select-none"
        style={{
          width: size,
          height: size,
          boxShadow: 'inset 0 2px 4px rgba(0, 0, 0, 0.5), 0 1px 2px rgba(255, 255, 255, 0.1)',
        }}
      >
        {/* Knob Indicator */}
        <div
          className="absolute inset-0 flex items-start justify-center pt-1 transition-transform"
          style={{ transform: `rotate(${rotation}deg)` }}
        >
          <div
            className="w-0.5 h-3 bg-blue-400 rounded-full"
            style={{ boxShadow: '0 0 4px rgba(96, 165, 250, 0.8)' }}
          />
        </div>

        {/* Center dot */}
        <div className="absolute inset-0 flex items-center justify-center">
          <div className="w-1.5 h-1.5 bg-[#252525] rounded-full" />
        </div>
      </div>

      {label && <span className="text-[9px] text-[#999]">{label}</span>}
      <span className="text-[10px] text-white font-medium">{value}</span>
    </div>
  );
}
