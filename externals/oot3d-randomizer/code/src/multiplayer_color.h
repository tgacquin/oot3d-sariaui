#pragma once
#include "z3D/z3D.h"

extern u32 rMultiplayerTunicColor;
u32 Multiplayer_GetTunicColor(void);
u32 MultiplayerTunic_ResolveColor(u32 color, u8 tunic);
// CMAB resources are shared. Override only around one actor's draw and restore
// every key afterwards, so local/remote/rainbow palettes cannot leak to peers.
typedef struct {
    char* chunk;
    f32 saved[9];
    u8 child;
} MultiplayerTunicScope;
void MultiplayerTunic_Begin(MultiplayerTunicScope* scope, MaterialAnimation* animation, u32 color, u8 child);
void MultiplayerTunic_End(MultiplayerTunicScope* scope);
