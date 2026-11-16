// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <QWindow>
#import <QuartzCore/CADisplayLink.h>
#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/interpolation_timing.h"
// MoltenVK owns drawable acquisition and pool reconfiguration. Do not attach
// CAMetalDisplayLink to this layer: it conflicts with swapchain recreation.
@interface OOTMetalLayer : CAMetalLayer
@end
@implementation OOTMetalLayer
- (id<CAMetalDrawable>)nextDrawable {
    id<CAMetalDrawable> drawable = [super nextDrawable];
    [drawable addPresentedHandler:^(id<MTLDrawable> presented) {
        OOT3D::Interpolation::Timing::Displayed(presented.presentedTime);
    }];
    return drawable;
}
@end

API_AVAILABLE(macos(14.0))
@interface OOTDisplayLinkTarget : NSObject {
    CFTimeInterval started;
    unsigned callbacks;
    int requested;
}
- (void)tick:(CADisplayLink*)link;
@end

@implementation OOTDisplayLinkTarget
- (void)tick:(CADisplayLink*)link {
    using namespace OOT3D::Interpolation::Timing;
    const int desired = display_request_fps.load();
    if (desired != requested) {
        requested = desired;
        // Ask ProMotion for our target; the OS still chooses the actual rate.
        link.preferredFrameRateRange = desired > 0
            ? CAFrameRateRangeMake(desired, desired, desired) : CAFrameRateRangeDefault;
    }
    const auto now = CACurrentMediaTime();
    if (!started) started = now;
    ++callbacks;
    if (now - started >= 1.0) {
        display_callback_fps = callbacks / (now - started);
        started = now;
        callbacks = 0;
    }
}
@end

#endif

namespace MetalUtil {

void* CreateMetalLayer(QWindow* window) {
    // Qt's MetalSurface QWindow on macOS does not always back the NSView
    // with a CAMetalLayer immediately; MoltenVK 1.3+ aborts in
    // MVKSurface::getNaturalExtent() if the layer is not CAMetalLayer.
    // Force-install a fresh CAMetalLayer on the view.
#ifdef ENABLE_OOT3D_PRESENTATION
    CAMetalLayer* metalLayer = [OOTMetalLayer layer];
#else
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
#endif
    NSView* view = (__bridge NSView*)window->winId();
    view.wantsLayer = YES;
    view.layer = metalLayer;
#ifdef ENABLE_OOT3D_PRESENTATION
    if (@available(macOS 14.0, *)) {
        auto* target = [[OOTDisplayLinkTarget alloc] init];
        CADisplayLink* link = [view displayLinkWithTarget:target selector:@selector(tick:)];
        [link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
        // The run loop retains the link, which retains its target. Tie invalidation
        // to Qt's render window so neither survives a closed/recreated surface.
        QObject::connect(window, &QObject::destroyed, [link] { [link invalidate]; });
#if !__has_feature(objc_arc)
        [target release];
#endif
    }
#endif
    return (__bridge void*)metalLayer;
}

} // namespace MetalUtil
