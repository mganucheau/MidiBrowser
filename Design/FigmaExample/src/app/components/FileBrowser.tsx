import { File, Folder, Music } from 'lucide-react';
import { ScrollArea } from './ui/scroll-area';

const midiFiles = [
  { id: '1', name: 'Bass Line.mid', folder: 'Bass' },
  { id: '2', name: 'Chord Progression.mid', folder: 'Chords' },
  { id: '3', name: 'Drum Pattern 1.mid', folder: 'Drums' },
  { id: '4', name: 'Drum Pattern 2.mid', folder: 'Drums' },
  { id: '5', name: 'Lead Melody.mid', folder: 'Melody' },
  { id: '6', name: 'Arp Pattern.mid', folder: 'Melody' },
  { id: '7', name: 'Pad Texture.mid', folder: 'Pads' },
  { id: '8', name: 'Vocal Chop.mid', folder: 'Vocals' },
];

export function FileBrowser() {
  const folders = Array.from(new Set(midiFiles.map((f) => f.folder)));

  return (
    <div className="w-64 bg-[#1e1e1e] border-r border-[#2d2d2d] flex flex-col">
      {/* Header */}
      <div className="h-10 bg-[#252525] border-b border-[#2d2d2d] px-3 flex items-center">
        <Music className="size-3 mr-2 text-[#999]" />
        <span className="text-xs font-medium">MIDI Clips</span>
      </div>

      {/* File List */}
      <ScrollArea className="flex-1">
        <div className="p-2 space-y-3">
          {folders.map((folder) => (
            <div key={folder} className="space-y-1">
              {/* Folder Header */}
              <div className="flex items-center gap-1.5 px-2 py-1 text-[#999]">
                <Folder className="size-3" />
                <span className="text-[10px] font-medium uppercase tracking-wide">{folder}</span>
              </div>

              {/* Files in Folder */}
              {midiFiles
                .filter((f) => f.folder === folder)
                .map((file) => (
                  <div
                    key={file.id}
                    draggable
                    className="px-2 py-1.5 rounded hover:bg-[#252525] hover:border hover:border-[#333] cursor-pointer transition-colors flex items-center gap-2 group"
                  >
                    <File className="size-3 text-[#999] group-hover:text-blue-400" />
                    <span className="text-[10px] text-white group-hover:text-blue-400">
                      {file.name}
                    </span>
                  </div>
                ))}
            </div>
          ))}
        </div>
      </ScrollArea>

      {/* Footer Info */}
      <div className="h-8 bg-[#252525] border-t border-[#2d2d2d] px-3 flex items-center justify-between text-[9px] text-[#999]">
        <span>{midiFiles.length} clips</span>
        <span>Drag to add</span>
      </div>
    </div>
  );
}
