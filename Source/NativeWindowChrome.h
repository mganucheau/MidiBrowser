#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace pflow {
namespace nativeChrome {

#if JUCE_MAC

/** FullSizeContentView + transparent titlebar so native traffic lights
    float over the app header. */
void applyCupertinoTitlebar(juce::Component& anyComponentInWindow);

/** [NSWindow performWindowDragWithEvent:] — header drag + double-click zoom. */
bool performWindowDrag(juce::Component& anyComponentInWindow);

void zoomWindow(juce::Component& anyComponentInWindow);
bool isWindowKey(juce::Component& anyComponentInWindow);
void preferNativeTitleBar(juce::Component& anyComponentInWindow);

#else

inline void applyCupertinoTitlebar(juce::Component&) {}
inline bool performWindowDrag(juce::Component&) { return false; }
inline void zoomWindow(juce::Component&) {}
inline bool isWindowKey(juce::Component&) { return true; }
inline void preferNativeTitleBar(juce::Component&) {}

#endif

} // namespace nativeChrome
} // namespace pflow
