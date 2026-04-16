import { 
  File, 
  FolderOpen, 
  Save, 
  Scissors, 
  Copy, 
  Clipboard, 
  Undo, 
  Redo,
  Settings,
  HelpCircle
} from 'lucide-react';
import { Button } from './ui/button';

export function Toolbar() {
  return (
    <div className="h-12 bg-[#1e1e1e] border-b border-[#2d2d2d] flex items-center px-3 gap-1">
      {/* File Menu */}
      <div className="flex items-center gap-1 px-2 border-r border-[#2d2d2d]">
        <Button variant="ghost" size="sm" className="h-8 px-2">
          <File className="size-4" />
          <span className="ml-1.5 text-xs">File</span>
        </Button>
        <Button variant="ghost" size="sm" className="h-8 px-2">
          <span className="text-xs">Edit</span>
        </Button>
        <Button variant="ghost" size="sm" className="h-8 px-2">
          <span className="text-xs">View</span>
        </Button>
        <Button variant="ghost" size="sm" className="h-8 px-2">
          <span className="text-xs">Track</span>
        </Button>
        <Button variant="ghost" size="sm" className="h-8 px-2">
          <span className="text-xs">Plugins</span>
        </Button>
      </div>

      {/* Quick Actions */}
      <div className="flex items-center gap-0.5 px-2">
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <FolderOpen className="size-4" />
        </Button>
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Save className="size-4" />
        </Button>
      </div>

      <div className="w-px h-6 bg-[#2d2d2d]" />

      {/* Edit Tools */}
      <div className="flex items-center gap-0.5 px-2">
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Undo className="size-4" />
        </Button>
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Redo className="size-4" />
        </Button>
      </div>

      <div className="w-px h-6 bg-[#2d2d2d]" />

      <div className="flex items-center gap-0.5 px-2">
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Scissors className="size-4" />
        </Button>
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Copy className="size-4" />
        </Button>
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Clipboard className="size-4" />
        </Button>
      </div>

      <div className="flex-1" />

      {/* Right Side */}
      <div className="flex items-center gap-0.5">
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <Settings className="size-4" />
        </Button>
        <Button variant="ghost" size="sm" className="h-8 w-8 p-0">
          <HelpCircle className="size-4" />
        </Button>
      </div>
    </div>
  );
}
