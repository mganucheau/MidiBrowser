#include "NativeWindowChrome.h"

#if JUCE_MAC
 #import <AppKit/AppKit.h>
#endif

namespace pflow {
namespace nativeChrome {

#if JUCE_MAC

namespace {

NSWindow* windowForComponent(juce::Component& c)
{
    if (auto* peer = c.getPeer())
        if (auto* view = (NSView*) peer->getNativeHandle())
            return [view window];
    return nil;
}

} // namespace

void preferNativeTitleBar(juce::Component& anyComponentInWindow)
{
    for (auto* c = &anyComponentInWindow; c != nullptr; c = c->getParentComponent())
    {
        if (auto* tl = dynamic_cast<juce::TopLevelWindow*>(c))
        {
            tl->setUsingNativeTitleBar(true);
            if (auto* dw = dynamic_cast<juce::DocumentWindow*>(tl))
            {
                // Keep real traffic lights (close / miniaturize / zoom) on the left.
                dw->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, true);
            }
            break;
        }
    }
}

void applyCupertinoTitlebar(juce::Component& anyComponentInWindow)
{
    preferNativeTitleBar(anyComponentInWindow);

    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window == nil)
        return;

    const NSWindowStyleMask keep = NSWindowStyleMaskTitled
                                 | NSWindowStyleMaskClosable
                                 | NSWindowStyleMaskMiniaturizable
                                 | NSWindowStyleMaskResizable;
    NSWindowStyleMask mask = [window styleMask];
    mask |= keep;
    mask |= NSWindowStyleMaskFullSizeContentView;
    [window setStyleMask:mask];
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleHidden;
    window.movableByWindowBackground = NO;

    [[window standardWindowButton:NSWindowCloseButton] setHidden:NO];
    [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:NO];
    [[window standardWindowButton:NSWindowZoomButton] setHidden:NO];
}

bool performWindowDrag(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window == nil)
        return false;
    NSEvent* event = [NSApp currentEvent];
    if (event == nil)
        return false;
    [window performWindowDragWithEvent:event];
    return true;
}

void zoomWindow(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window != nil)
        [window zoom:nil];
}

bool isWindowKey(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window != nil)
        return [window isKeyWindow];
    return true;
}

#endif // JUCE_MAC

} // namespace nativeChrome
} // namespace pflow
