import { Folder, Music, AudioLines, Mic, Disc3 } from 'lucide-react';
import { ScrollArea } from './ui/scroll-area';
import { Tabs, TabsContent, TabsList, TabsTrigger } from './ui/tabs';

export function Sidebar() {
  const audioFiles = [
    { name: 'Kick_01.wav', duration: '0:02', size: '256 KB' },
    { name: 'Snare_Tight.wav', duration: '0:01', size: '128 KB' },
    { name: 'HiHat_Closed.wav', duration: '0:01', size: '64 KB' },
    { name: 'Bass_Loop.wav', duration: '0:08', size: '1.2 MB' },
    { name: 'Guitar_Riff.wav', duration: '0:16', size: '2.4 MB' },
    { name: 'Vocal_Sample.wav', duration: '0:04', size: '512 KB' },
  ];

  const instruments = [
    { name: 'Piano', type: 'Synth' },
    { name: 'Bass Synth', type: 'Synth' },
    { name: 'Drum Machine', type: 'Sampler' },
    { name: 'Electric Guitar', type: 'Amp Sim' },
    { name: 'Strings', type: 'Orchestral' },
  ];

  return (
    <div className="w-64 bg-[#1e1e1e] border-r border-[#2d2d2d] flex flex-col">
      <Tabs defaultValue="files" className="flex-1 flex flex-col">
        <div className="h-10 bg-[#252525] border-b border-[#2d2d2d] px-2">
          <TabsList className="w-full h-full grid grid-cols-2 bg-transparent">
            <TabsTrigger value="files" className="text-xs data-[state=active]:bg-[#333]">
              <Folder className="size-3 mr-1.5" />
              Files
            </TabsTrigger>
            <TabsTrigger value="instruments" className="text-xs data-[state=active]:bg-[#333]">
              <Music className="size-3 mr-1.5" />
              Instruments
            </TabsTrigger>
          </TabsList>
        </div>

        <TabsContent value="files" className="flex-1 m-0">
          <ScrollArea className="h-full">
            <div className="p-3 space-y-1">
              {audioFiles.map((file, index) => (
                <div
                  key={index}
                  className="p-2 rounded hover:bg-[#252525] cursor-pointer border border-transparent hover:border-[#333] transition-colors"
                  draggable
                >
                  <div className="flex items-center gap-2 mb-1">
                    <AudioLines className="size-3 text-blue-400" />
                    <span className="text-xs flex-1 truncate">{file.name}</span>
                  </div>
                  <div className="flex items-center justify-between text-[10px] text-gray-500 ml-5">
                    <span>{file.duration}</span>
                    <span>{file.size}</span>
                  </div>
                </div>
              ))}
            </div>
          </ScrollArea>
        </TabsContent>

        <TabsContent value="instruments" className="flex-1 m-0">
          <ScrollArea className="h-full">
            <div className="p-3 space-y-1">
              {instruments.map((instrument, index) => (
                <div
                  key={index}
                  className="p-2 rounded hover:bg-[#252525] cursor-pointer border border-transparent hover:border-[#333] transition-colors"
                  draggable
                >
                  <div className="flex items-center gap-2 mb-1">
                    <Mic className="size-3 text-purple-400" />
                    <span className="text-xs flex-1 truncate">{instrument.name}</span>
                  </div>
                  <div className="text-[10px] text-gray-500 ml-5">
                    {instrument.type}
                  </div>
                </div>
              ))}
            </div>
          </ScrollArea>
        </TabsContent>
      </Tabs>
    </div>
  );
}