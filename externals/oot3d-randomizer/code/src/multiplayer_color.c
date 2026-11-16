#include "multiplayer_color.h"
#include "settings.h"

u32 rMultiplayerTunicColor = 0;

u32 MultiplayerTunic_ResolveColor(u32 color, u8 tunic) {
    if ((color >> 24) == 0xA5) return color;
    // The multiplayer model uses the recolorable material even when the
    // override is off. Use the upstream vanilla palette, not asset placeholders.
    if (gSettingsContext.mp_Enabled && !gSettingsContext.customTunicColors) {
        static const u32 vanilla[] = {0x168A0E, 0x96000E, 0x002A8E};
        return 0xA5000000 | vanilla[tunic < 3 ? tunic : 0];
    }
    return 0;
}

static u32 ColorOffset(u8 child, u32 channel, u32 key) {
    return 0x70 + channel * (child ? 0x18 : 0x28) + key * 8;
}

void MultiplayerTunic_Begin(MultiplayerTunicScope* scope, MaterialAnimation* animation, u32 color, u8 child) {
    scope->chunk = NULL;
    if ((color >> 24) != 0xA5 || !animation || !animation->cmabManager) return;
    char* chunk = *(char**)animation->cmabManager;
    if (!chunk) return;
    scope->chunk = chunk;
    scope->child = child;
    for (u32 channel = 0; channel < 3; ++channel) {
        const f32 value = ((color >> (16 - 8 * channel)) & 255) / 255.0f;
        for (u32 key = 0; key < (child ? 1u : 3u); ++key) {
            f32* at = (f32*)(chunk + ColorOffset(child, channel, key));
            scope->saved[channel * 3 + key] = *at;
            *at = value;
        }
    }
}

void MultiplayerTunic_End(MultiplayerTunicScope* scope) {
    if (!scope->chunk) return;
    for (u32 channel = 0; channel < 3; ++channel)
        for (u32 key = 0; key < (scope->child ? 1u : 3u); ++key)
            *(f32*)(scope->chunk + ColorOffset(scope->child, channel, key)) = scope->saved[channel * 3 + key];
    scope->chunk = NULL;
}
