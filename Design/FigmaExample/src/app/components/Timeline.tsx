import { useEffect, useRef } from 'react';

interface TimelineProps {
  zoom?: number;
}

export function Timeline({ zoom = 1 }: TimelineProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    
    ctx.scale(dpr, dpr);

    // Clear
    ctx.fillStyle = '#1a1a1a';
    ctx.fillRect(0, 0, rect.width, rect.height);

    // Draw ruler
    const pixelsPerSecond = 50 * zoom;
    const totalSeconds = Math.ceil(rect.width / pixelsPerSecond);

    ctx.strokeStyle = '#444';
    ctx.fillStyle = '#999';
    ctx.font = '10px monospace';

    for (let i = 0; i <= totalSeconds; i++) {
      const x = i * pixelsPerSecond;
      
      // Major tick
      ctx.beginPath();
      ctx.moveTo(x, rect.height - 10);
      ctx.lineTo(x, rect.height);
      ctx.stroke();

      // Label
      const minutes = Math.floor(i / 60);
      const seconds = i % 60;
      const label = `${minutes}:${seconds.toString().padStart(2, '0')}`;
      ctx.fillText(label, x + 4, 12);

      // Minor ticks (every 0.25 seconds)
      for (let j = 1; j < 4; j++) {
        const minorX = x + (j * pixelsPerSecond / 4);
        ctx.beginPath();
        ctx.moveTo(minorX, rect.height - 5);
        ctx.lineTo(minorX, rect.height);
        ctx.stroke();
      }
    }

    // Playhead
    ctx.strokeStyle = '#ff4444';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(0, rect.height);
    ctx.stroke();
    
  }, [zoom]);

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-8 bg-[#1a1a1a] border-b border-[#2d2d2d]"
    />
  );
}
