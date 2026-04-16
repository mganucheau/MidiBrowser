import { useState } from 'react';
import { PatternFlowHeader } from './components/PatternFlowHeader';
import { ControlStrip } from './components/ControlStrip';
import { FileBrowser } from './components/FileBrowser';
import { ArrangementView } from './components/ArrangementView';
import { PianoRoll } from './components/PianoRoll';

export default function App() {
  const [selectedLane, setSelectedLane] = useState<string | null>(null);
  const [selectedClip, setSelectedClip] = useState<string | null>(null);
  const [isRecording, setIsRecording] = useState(false);
  const [gridSnap, setGridSnap] = useState('1/16');
  const [sessionLength, setSessionLength] = useState('8 bars');

  // Control strip state
  const [compEnabled, setCompEnabled] = useState(false);
  const [compValue, setCompValue] = useState(3);
  const [transposeEnabled, setTransposeEnabled] = useState(false);
  const [key, setKey] = useState('C');
  const [scale, setScale] = useState('Major');
  const [octave, setOctave] = useState('4');
  const [loopStart, setLoopStart] = useState(0);
  const [loopSync, setLoopSync] = useState(true);
  const [loopEnd, setLoopEnd] = useState(8);

  return (
    <div className="size-full flex flex-col bg-[#1a1a1a] text-white overflow-hidden">
      {/* Row 1: Title Bar */}
      <PatternFlowHeader
        isRecording={isRecording}
        onRecordToggle={() => setIsRecording(!isRecording)}
        gridSnap={gridSnap}
        onGridSnapChange={setGridSnap}
        sessionLength={sessionLength}
        onSessionLengthChange={setSessionLength}
      />

      {/* Row 2: Control Strip */}
      <ControlStrip
        compEnabled={compEnabled}
        onCompEnabledChange={setCompEnabled}
        compValue={compValue}
        onCompValueChange={setCompValue}
        transposeEnabled={transposeEnabled}
        onTransposeEnabledChange={setTransposeEnabled}
        keyValue={key}
        onKeyChange={setKey}
        scaleValue={scale}
        onScaleChange={setScale}
        octaveValue={octave}
        onOctaveChange={setOctave}
        loopStart={loopStart}
        onLoopStartChange={setLoopStart}
        loopSync={loopSync}
        onLoopSyncChange={setLoopSync}
        loopEnd={loopEnd}
        onLoopEndChange={setLoopEnd}
      />

      {/* Main Content Area */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left: File Browser */}
        <FileBrowser />

        {/* Center/Right: Arrangement + Piano Roll */}
        <div className="flex-1 flex flex-col overflow-hidden">
          {/* Arrangement View */}
          <div className="flex-[2] overflow-hidden">
            <ArrangementView
              selectedLane={selectedLane}
              onLaneSelect={setSelectedLane}
              selectedClip={selectedClip}
              onClipSelect={setSelectedClip}
            />
          </div>

          {/* Piano Roll */}
          <div className="flex-[1] border-t border-[#2d2d2d] overflow-hidden">
            <PianoRoll selectedClip={selectedClip} />
          </div>
        </div>
      </div>
    </div>
  );
}
