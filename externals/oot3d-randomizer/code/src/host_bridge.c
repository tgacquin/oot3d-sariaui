// Host bridge for per-save activation and restoration of native static tables.
__attribute__((naked)) void Host_InitBegin(void) { __asm__("svc 0xfff401\n bx lr"); }
__attribute__((naked)) void Host_InitEnd(void) { __asm__("svc 0xfff402\n bx lr"); }

__attribute__((naked)) int Host_PurchasePrice(int price) { __asm__("svc 0xfff405\n bx lr"); }

__attribute__((naked)) int Host_MultiplayerStatus(int stage, int players, int error) { __asm__("svc 0xfff40b\n bx lr"); }
