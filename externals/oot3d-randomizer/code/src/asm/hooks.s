.arm
.text
.syntax unified

.macro HOOK name
    .global hook_\name
hook_\name:
.endm

.section .loader
HOOK into_loader
    push {r0-r12, lr}
    bl loader_main
    pop {r0-r12, lr}
    b nninitRegion

.section .text.randomizer_hooks

HOOK before_GlobalContext_Update
    push {r0-r12, lr}
    bl hookabi_void_before_GlobalContext_Update
    // The native update prologue saved r4-r8/lr before this hook. During
    // startup, keep network polling alive but do not advance hidden gameplay.
    ldr r0, .Lmultiplayer_hold
    ldr r0, [r0]
    cmp r0, #0
    pop {r0-r12, lr}
    beq .Lmultiplayer_update
    // Preserve the native per-frame render context registration (452394..3a4).
    ldr r1, .Lmultiplayer_render_context
    add r2, r0, #0x14
    str r2, [r1]
    pop {r4-r8, lr}
    b 0x2E25F0 // Keep the render manager/UI alive while simulation is held.
.Lmultiplayer_update:
    cpy r7,r0
    bx lr
.Lmultiplayer_hold:
    .word rMultiplayerHold
.Lmultiplayer_render_context:
    .word 0x539F98

HOOK after_GlobalContext_Update
    push {r0-r12, lr}
    bl hookabi_void_after_GlobalContext_Update
    pop {r0-r12, lr}
    b 0x2E25F0

HOOK Gfx_Update
    push {r0-r12, lr}
    bl hookabi_void_Gfx_Update
    pop {r0-r12, lr}
    pop {r4-r8, pc}

HOOK Draw_PreSwapBuffers
    push {r0-r12, lr}
    bl hookabi_void_Draw_PreSwapBuffers
    pop {r0-r12, lr}
    bx lr

HOOK Gfx_SleepQueryCallback
    push {r0-r12, lr}
    bl hookabi_void_Gfx_SleepQueryCallback
    pop {r0-r12, lr}
    add r0,r0,#0x9C
    b 0x3FD6C8

HOOK Gfx_AwakeCallback
    push {r0-r12, lr}
    bl hookabi_void_Gfx_AwakeCallback
    pop {r0-r12, lr}
    add r0,r0,#0x9C
    b 0x3FD440

HOOK ActorGetItemOffer
    push {r0-r12, lr}
    cpy r0,r5
    cpy r1,r6
    cpy r2,r7
    bl hookabi_void_ItemOverride_OfferGetItem
    pop {r0-r12, lr}
    bx lr

HOOK SaveFile_Init
    push {r0-r12, lr}
    mov r0, r5
    bl hookabi_void_SaveFile_Init
    pop {r0-r12, lr}
    strb r1,[r0,#0x35]
    bx lr

HOOK OverrideItemAndTextID
    cpy r0,r7 @ globalCtx
    push {r0-r12, lr}
    cpy r1,r4 @ player
    bl hookabi_value_ItemOverride_GetOverheadItem
    cmp r0,#0x0
    pop {r0-r12, lr}
    bxlt lr @ resume vanilla code
    beq 0x2BC1DC @ skip text and item
    b 0x2BC354 @ skip text, item and fanfare

HOOK OverrideDrawItemOne
    push {r1-r12, lr}
    bl hookabi_value_ItemOverride_GetDrawItem
    pop {r1-r12, lr}
    ldrh r0,[r0]
    bx lr

HOOK OverrideDrawItemTwo
    add r0,r1,r0,lsl #0x1
    push {r1-r12, lr}
    bl hookabi_value_ItemOverride_GetDrawItem
    pop {r1-r12, lr}
    bx lr

HOOK OverrideDrawItemThree
    add r5,r0,r1,lsl #0x1
    push {r0-r4,r6-r12, lr}
    cpy r0,r5
    bl hookabi_value_ItemOverride_GetDrawItem
    cpy r5,r0
    pop {r0-r4,r6-r12, lr}
    bx lr

HOOK OverrideGiDrawIdPlusOne
    push {r1-r12, lr}
    bl hookabi_value_ItemOverride_OverrideGiDrawIdPlusOne
    pop {r1-r12, lr}
    strh r0,[r6,#0x4e]
    bx lr

HOOK EditDrawGetItemBeforeModelSpawn
    push {r0-r12, lr}
    bl hookabi_void_ItemOverride_EditDrawGetItemBeforeModelSpawn
    pop {r0-r12, lr}
    mov r7,#0x0
    bx lr

HOOK EditDrawDetItemAfterModelSpawn
    push {r0-r12, lr}
    bl hookabi_void_ItemOverride_EditDrawGetItemAfterModelSpawn
    pop {r0-r12, lr}
    str r0,[r6,#0x78]
    bx lr

HOOK DrawGetItem_ShouldRotate
    push {r0-r12, lr}
    bl hookabi_value_ItemOverride_DrawGetItem_ShouldSkipRotation
    cmp r0,#0x0
    pop {r0-r12, lr}
    cmpeq r4,#0x0
    bx lr

HOOK EditDrawGetItemAfterMatrixUpdate
    push {r0-r12, lr}
    cpy r0,r1 @ SkeletonAnimationModel
    bl hookabi_void_ItemOverride_EditDrawGetItemAfterMatrixUpdate
    pop {r0-r12, lr}
    b 0x330B98

# TODO: Text ID in game gets messed up,
# Gives the "What's that?" text instead of
# the text about moving the statue
HOOK AdultRollingGoronTwo
    mov r0,#0x3000
    add r0,r0,#0x38
    b 0x1954B0

HOOK DaruniaStrengthCheck
    push {r0-r12, lr}
    bl hookabi_value_EnDu_CheckRewardFlag
    cmp r0,#0x1
    pop {r0-r12, lr}
    b 0x1E48A0

HOOK GetToken
    push {r0-r12, lr}
    cpy r0,r4 @ token actor
    bl hookabi_value_ItemOverride_GetSkulltulaToken
    cmp r0,#0x0
    pop {r0-r12, lr}
    addne lr,lr,#0x20 @ skip playing fanfare
    bx lr

HOOK ItemEtceteraModelDraw
    push {r0-r12, lr}
    bl hookabi_value_Model_DrawByActor
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x1170B4
    cpy r4,r0
    bx lr

HOOK KokiriCheckOpenForest
    push {r0-r12, lr}
    bl hookabi_value_EnKo_CheckOpenForest
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK EnExItemModelDraw
    push {r0-r12, lr}
    mov r0,r4
    bl hookabi_value_Model_DrawByActor
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x249BC8
    add r1,r4,#0x148
    bx lr

HOOK DemoEffectMedallionDraw
    push {r0-r12, lr}
    mov r0,r4
    bl hookabi_value_Model_DrawByActor
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x372148
    ldr r0,[r4,#0x2A8]
    b 0x372150

HOOK DemoEffectStoneDraw
    push {r0-r12, lr}
    mov r0,r4
    bl hookabi_value_Model_DrawByActor
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x1D20C0
    ldr r0,[r4,#0x2A8]
    b 0x1D20A0

HOOK EnKoInitCheckForest
    push {r1-r12, lr}
    bl hookabi_value_EnKo_CheckForestTempleBeat
    tst r0,#0x1
    pop {r1-r12, lr}
    bx lr

HOOK FireArrowCheckChestFlag
    push {r0-r12, lr}
    bl hookabi_value_ShotSun_CheckChestFlag
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK SerenadeCheckChestFlag
    push {r0-r12, lr}
    bl hookabi_value_Cutscene_SerenadeCheckChestFlag
    cmp r0,#0x0
    pop {r0-r12, lr}
    cpy r0,r5
    bx lr

HOOK ScarecrowCheckToBeActivated
    push {r0-r12, lr}
    bl hookabi_value_Scarecrow_CheckToBeActivated
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK EponasSongCheckFlag
    push {r0-r12, lr}
    bl hookabi_value_Cutscene_CheckEponasSongFlag
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK SariasSongCheckFlag
    push {r0-r12, lr}
    bl hookabi_value_Cutscene_CheckSariasSongFlag
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK ApplyDamageMultiplier
    push {r0-r3, r5-r12, lr}
    bl hookabi_value_Settings_ApplyDamageMultiplier
    cpy r4,r0
    pop {r0-r3, r5-r12, lr}
    bx lr

HOOK ActorUpdate
    push {r0-r12, lr}
    bl hookabi_void_Actor_rUpdate
    pop {r0-r12, lr}
    bx lr

HOOK SceneInitAfterCopyScenes
    push {r0-r12, lr}
    bl 0x371738
    bl hookabi_void_Scene_Init
    pop {r0-r12, lr}
    bx lr

HOOK StoreChildBButtonEquip
    push {r0-r12, lr}
    bl hookabi_void_SaveFile_SaveChildBButton
    pop {r0-r12, lr}
    b ret_StoreChildBButtonEquip

HOOK LullabyCheckFlag
    push {r0-r12, lr}
    bl hookabi_value_Cutscene_CheckLullabyFlag
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK FishingIgnoreTempBOne
    bne 0x2C3A14
    push {r0-r12, lr}
    bl hookabi_value_isFishing
    cmp r0,#0x1
    pop {r0-r12, lr}
    bne 0x2C3998
    moveq r0,#89
    b 0x2C3A14

HOOK FishingIgnoreTempBTwo
    blt 0x34CFD0
    push {r0-r12, lr}
    bl hookabi_value_isFishing
    cmp r0,#0x1
    pop {r0-r12, lr}
    bne 0x34CFF0
    ldrb r1,[r4,#0x80]
    b 0x34CFD0

HOOK ConvertBombDropOne
    push {r1-r12, lr}
    bl hookabi_value_Item_ConvertBombDrop
    pop {r1-r12, lr}
    cpy r12,r0
    b 0x35E474

HOOK ConvertBombDropTwo
    push {r1-r12, lr}
    cpy r0,r6
    bl hookabi_value_Item_ConvertBombDrop
    pop {r1-r12, lr}
    cpy r6,r0
    b 0x3747B0

HOOK BeanDaddyModifyBeansBought
    push {r1-r12, lr}
    bl hookabi_value_EnMs_ModifyBeansBought
    pop {r1-r12, lr}
    cmp r0,#0xa
    bx lr

HOOK BeanDaddyModifyPrice
    push {r0,r2-r12, lr}
    cpy r0,r1
    bl hookabi_value_EnMs_ModifyPrice
    cpy r1,r0
    pop {r0,r2-r12, lr}
    cmp r0,r1
    bx lr

HOOK CheckGerudoToken
    push {r1-r12, lr}
    bl hookabi_value_SaveFile_CheckGerudoToken
    pop {r1-r12, lr}
    bx lr

HOOK CowBottleCheck
    push {r1-r12, lr}
    cpy r0,r4
    bl hookabi_value_EnCow_BottleCheck
    pop {r1-r12, lr}
    bx lr

HOOK CowItemOverride
    push {r0-r1, r3-r12, lr}
    bl hookabi_value_EnCow_ItemOverride
    cpy r2,r0
    pop {r0-r1, r3-r12, lr}
    b 0x3EE37C

HOOK AnjuCheckCuccoAmount
    push {r1-r12, lr}
    bl hookabi_value_EnNiwLady_CheckCuccoAmount
    pop {r1-r12, lr}
    cmp r0,#0x7
    cpylt r8,r0
    b 0x179424

HOOK KingZoraCheckMovedFlag
    push {r1-r12, lr}
    bl hookabi_value_EnKz_CheckMovedFlag
    pop {r1-r12, lr}
    cmp r0,#0x0
    bx lr

HOOK EnFrSetupFrogSongTimerMultiplier
    push {r0,r1,r3-r12,lr}
    mov r0,r2
    bl hookabi_value_FrogSongTimerMultiplier
    mov r2,r0
    pop {r0,r1,r3-r12,lr}
    strh r2,[r1,#0x42]
    bx lr

HOOK FrogReward
    push {r0-r12, lr}
    bl hookabi_void_EnFr_rSetReward
    pop {r0-r12, lr}
    b 0x389930

HOOK CanPlayBombchuBowling
    push {r0-r12, lr}
    bl hookabi_value_EnBomBowlMan_CheckExplosives
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK SlidingDoorDestroyCustomModels
    push {r0-r12, lr}
    cpy r0,r4
    bl hookabi_void_Door_CheckToDeleteCustomModels
    pop {r0-r12, lr}
    mov r0,#0x0
    bx lr

HOOK HandleDoorDestroyCustomModels
    push {r0-r12, lr}
    cpy r0,r4
    bl hookabi_void_Door_CheckToDeleteCustomModels
    pop {r0-r12, lr}
    str r0,[r4,#0x3E4]
    bx lr

HOOK TalonGetCastleTextbox
    push {r0, r2-r12, lr}
    bl hookabi_value_EnTa_GetCastleTextbox
    mov r1, r0
    pop {r0, r2-r12, lr}
    bx lr

HOOK MidoCheckSpawn
    push {r0-r12, lr}
    bl hookabi_value_EnMd_ShouldSpawn
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK MidoForestDialog
    push {r1-r12, lr}
    bl hookabi_value_EnMd_GetTextKokiriForest
    pop {r1-r12, lr}
    bx lr

HOOK CheckDekuTreeClear
    push {r0-r12, lr}
    bl hookabi_value_Dungeon_CheckDekuTreeClear
    tst r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK CheckCurrentDungeonMode
    push {r0-r12, lr}
    bl hookabi_value_Dungeon_GetCurrentDungeonMode
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK DungeonCheckJabuMQBox
    push {r0-r12, lr}
    bl hookabi_value_Dungeon_GetCurrentDungeonMode
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK JabuSwitchRutoCheck
    cmp r0,#0xA1
    bxeq lr
    cmp r0,#0x110
    bx lr

HOOK JabuBoxCheckRuto
    tst r0,#0x80
    push {r0-r12, lr}
    bleq hookabi_value_ObjKibako_CheckRuto
    cmpeq r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK CanReadHints
    push {r0-r12, lr}
    bl hookabi_value_Hints_CanReadHints
    cmp r0,#0x1
    pop {r0-r12, lr}
    # Cannot read hints, show this text
    # "What do you suppose this stone is?"
    movne r0,#0x100
    addne r0,r0,#0xB1
    bxne lr
    push {r0-r12, lr}
    bl hookabi_value_Hints_GetHintsSetting
    cmp r0,#0x0
    pop {r0-r12, lr}
    # Vanilla hints, can read them (wearing mask)
    # "Responding to your mask..."
    moveq r0,#0x2000
    addeq r0,r0,#0x54
    bxeq lr
    # Randomizer hints enabled, can read them
    # Hint message, skipping other text
    ldrh r0,[r4,#0x1C]
    and r0,r0,#0xFF
    add r0,r0,#0x400
    bx lr

HOOK GossipStoneAddSariaHint
    ldrh r0,[r5,#0x16]
    push {r0-r12, lr}
    add r0,r0,#0x600
    bl hookabi_void_Hints_AddSariasSongHint
    pop {r0-r12, lr}
    bx lr

HOOK FastChests
    push {r0-r12, lr}
    bl hookabi_value_Chest_OverrideAnimation
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK FastOwlCutscenes
    push {r0-r12, lr}
    bl hookabi_void_EnOwl_FastCutscene
    pop {r0-r12, lr}
    mov r1,#0xa
    bx lr

HOOK SetChildCustomTunic
    push {r0-r12, lr}
    bl hookabi_void_Player_SetChildCustomTunicCMAB
    pop {r0-r12, lr}
    b 0x191E60

HOOK ShortenRainbowBridgeCS
    push {r0-r12, lr}
    bl hookabi_void_ShortenRainbowBridgeCS
    pop {r0-r12, lr}
    cpy r4,r0
    bx lr

HOOK Chest_OverrideIceSmoke
    push {r0-r12, lr}
    mov r0,r4
    bl hookabi_value_Chest_OverrideIceSmoke
    cmp r0,#0x0
    pop {r0-r12, lr}
    cpy r0,r6
    bne 0x1D5E64
    b 0x1D5E60

HOOK EnableFW
    push {r0-r12, lr}
    bl hookabi_void_EnableFW
    pop {r0-r12, lr}
    add sp,sp,#0x14
    bx lr

HOOK FWUnset
    push {r0-r12, lr}
    bl hookabi_void_MagicWind_Unset
    mov r0,#-0x1
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK FWGetSet
    push {r0-r12, lr}
    bl hookabi_value_MagicWind_CheckSet
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x351A64
    b 0x3519D0

HOOK SetSavewarpEntrance
    push {r0-r12, lr}
    bl hookabi_void_Entrance_SetSavewarpEntrance
    pop {r0-r12, lr}
    bx lr

HOOK SetGameOverEntrance
    push {r0-r12, lr}
    bl hookabi_void_Entrance_SetGameOverEntrance
    pop {r0-r12, lr}
    bx lr

HOOK SetGameOverRespawnFlag
    push {r0-r12, lr}
    bl hookabi_void_Grotto_ForceGrottoReturn
    pop {r0-r12, lr}
    cmp r8,#0x3
    bx lr

HOOK SetSunsSongRespawnFlag
    push {r0-r12, lr}
    bl hookabi_void_Grotto_ForceGrottoReturn
    pop {r0-r12, lr}
    cpy r0,r6
    bx lr

HOOK SunsSongEndCloseTextbox
    push {r0-r12, lr}
    bl hookabi_void_Settings_SunsSongEndCloseTextbox
    pop {r0-r12, lr}
    b ret_SunsSongEndCloseTextbox

HOOK SetSpecialVoidOutRespawnFlag
    push {r0-r12, lr}
    bl hookabi_void_Grotto_ForceRegularVoidOut
    pop {r0-r12, lr}
    mov r1,#0x104
    bx lr

HOOK NoHealFromHealthUpgrades
    push {r1-r12, lr}
    bl hookabi_value_NoHealFromHealthUpgrades
    pop {r1-r12, lr}
    bx lr

HOOK NoHealFromBombchuBowlingPrize
    push {r1-r12, lr}
    bl hookabi_value_NoHealFromBombchuBowlingPrize
    pop {r1-r12, lr}
    bx lr

HOOK FairyPickupHealAmount
    push {r0-r12, lr}
    bl hookabi_void_FairyPickupHealAmount
    pop {r0-r12, lr}
    bx lr

HOOK FairyReviveHealAmount
    push {r1-r12, lr}
    bl hookabi_value_FairyReviveHealAmount
    pop {r1-r12, lr}
    bx lr

HOOK FairyUseHealAmount
    push {r1-r12, lr}
    bl hookabi_value_FairyUseHealAmount
    pop {r1-r12, lr}
    bx lr

HOOK MedigoronCheckFlagOne
    push {r0-r12, lr}
    bl hookabi_value_EnGm_CheckRewardFlag
    cmp r0,#1
    pop {r0-r12, lr}
    blt 0x130254
    beq 0x13026C
    tst r12,r3
    b 0x130250

HOOK MedigoronCheckFlagTwo
    bne childLink
    push {r0-r12, lr}
    bl hookabi_value_EnGm_CheckRewardFlag
    cmp r0,#1
    pop {r0-r12, lr}
    blt 0x1302F8
    bne 0x1302C8
childLink:
    b 0x1302F0

HOOK MedigoronSetRewardFlag
    mvn r0,#0xc7
    push {r0-r12, lr}
    bl hookabi_void_EnGm_SetRewardFlag
    pop {r0-r12, lr}
    b 0x16C91C

HOOK MedigoronItemOverrideOne
    push {r0-r1, r3-r12, lr}
    bl hookabi_value_EnGm_ItemOverride
    cpy r2,r0
    pop {r0-r1, r3-r12, lr}
    b 0x14D960

HOOK MedigoronItemOverrideTwo
    push {r0-r1, r3-r12, lr}
    bl hookabi_value_EnGm_ItemOverride
    cpy r2,r0
    pop {r0-r1, r3-r12, lr}
    b 0x16C9C0

HOOK MedigoronGetCustomText
    push {r0-r12, lr}
    bl hookabi_value_EnGm_UseCustomText
    cmp r0,#1
    pop {r0-r12, lr}
    moveq r2,#0x9100
    addeq r2,r2,#0x20
    movne r2,#0x3000
    addne r2,r2,#0x4F
    b 0x130260

HOOK CarpetSalesmanCheckFlagOne
    push {r0-r12, lr}
    bl hookabi_value_EnJs_CheckRewardFlag
    cmp r0,#1
    pop {r0-r12, lr}
    cmpne r0,#0
    bx lr

HOOK CarpetSalesmanCheckFlagTwo
    push {r0-r12, lr}
    bl hookabi_value_EnJs_CheckRewardFlag
    cmp r0,#1
    pop {r0-r12, lr}
    addeq r1,r1,#1
    strh r1,[r0,r4]
    bx lr

HOOK CarpetSalesmanSetFlag
    push {r0-r12, lr}
    bl hookabi_void_EnJs_SetRewardFlag
    pop {r0-r12, lr}
    mvn r0,#0xC7
    bx lr

HOOK KakarikoGateCheck
    push {r0-r12, lr}
    bl hookabi_value_KakGate_CheckToFixBug
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK DoorOfTimeCheck
    cmp r0,#0x4
    bne 0x274B70
    push {r0-r12, lr}
    bl hookabi_value_DoorOfTime_RequirementCheck
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK SongOfTimeJingle
    mov r1,#0x0
    push {r0-r12, lr}
    bl hookabi_value_DoorOfTime_RequirementCheck
    cmp r0,#0x1
    pop {r0-r12, lr}
    addne r0,r0,#0x4
    bx lr

HOOK GKSetDurability
    push {r0-r12, lr}
    bl hookabi_value_GK_SetDurability
    strh r0,[r8,#0x4a]
    pop {r0-r12, lr}
    b 0x376BE0

HOOK TurboTextAdvance
    push {r0-r12, lr}
    bl hookabi_value_Settings_IsTurboText
    cmp r0,#0x0
    pop {r0-r12, lr}
    cmpeq r0,#0x0
    bx lr

HOOK ItemsMenuDraw
    push {r0-r12, lr}
    bl hookabi_void_ItemsMenu_Draw
    pop {r0-r12, lr}
    b 0x2F8160

HOOK GearMenuEmptySlot
    push {r0,r2-r12, lr}
    bl   hookabi_value_GearMenu_GetRewardHint
    cpy  r1,r0
    pop  {r0,r2-r12, lr}
    cmp  r1,#0x0
    beq  0x2E9A1C @ clear tex box
    mov  r2,#0x1
    b    0x2E9A3C @ print reward hint

HOOK Audio_PlayFanfare
    push {r1-r12, lr}
    bl hookabi_value_Bgm_OverrideFanfare
    pop {r1-r12, lr}
    push {r3-r7, lr}
    b 0x35C52C

HOOK SetBGMEntrance
    push {r1-r12, lr}
    bl hookabi_value_SetBGM
    pop {r1-r12, lr}
    push {r4-r6, lr}
    b 0x33104C

HOOK SetBGMDayNight
    push {r1-r12, lr}
    bl hookabi_value_SetBGM
    pop {r1-r12, lr}
    push {r4-r6, lr}
    b ret_SetBGMDayNight

HOOK Audio_PlaySequence
    push {r0, r2-r12, lr}
    cpy r0,r1
    bl hookabi_value_SetBGM
    cpy r1,r0
    pop {r0, r2-r12, lr}
    push {r4-r11, lr}
    b 0x36EC44

HOOK Audio_PlaySfxGeneral
    push {r1-r12, lr}
    bl hookabi_value_SetSFX
    pop {r1-r12, lr}
    push {r0-r11, lr}
    b 0x375480

HOOK TurboTextClose
    push {r0-r12, lr}
    bl hookabi_value_Settings_IsTurboText
    cmp r0,#0x0
    pop {r0-r12, lr}
    cmpeq r0,#0x0
    bx lr

HOOK TurboTextSignalNPC
    movne r4,#0x1
    push {r0-r12, lr}
    bl hookabi_value_Settings_IsTurboText
    cmp r0,#0x0
    @ If about to warp to credits, signal NPC so that
    @ the collection flag is set before the game is saved
    bleq hookabi_value_Triforce_IsWaitingForText
    cmp r0,#0x0
    pop {r0-r12, lr}
    movne r4,#0x1
    bx lr

HOOK SkipSongReplays_TimeBlocksFix
    bne 0x208008
    push  {r0-r12, lr}
    bl hookabi_value_Settings_GetSongReplaysOption
    cmp r0,#0x0
    pop {r0-r12, lr}
    moveq r1,#0x6E
    movne r1,#0x10
    cmp r0,r0
    b 0x208008

HOOK SkipSongReplays_WarpBlocksFix
    bne 0x20806C
    push  {r0-r12, lr}
    bl hookabi_value_Settings_GetSongReplaysOption
    cmp r0,#0x0
    pop {r0-r12, lr}
    moveq r1,#0x6E
    movne r1,#0x10
    cmp r0,r0
    b 0x20806C

HOOK CarpenterBossSetTradedSawFlag
    push {r0-r12, lr}
    bl hookabi_void_EnToryo_SetTradedSawFlag
    pop {r0-r12, lr}
    str r0,[r4,#0xB10]
    bx lr

HOOK KingZoraSetTradedPrescriptionFlag
    push {r0-r12, lr}
    bl hookabi_void_EnKz_SetTradedPrescriptionFlag
    pop {r0-r12, lr}
    mov r2,#0x24
    b 0x1C52A4

HOOK SwapFaroresWind
    @ The next native instruction reads eventChkInf through r0 (save + 0xE00).
    @ A direct C call clobbers it and replays the first Master Sword cutscene.
    push {r0-r12, lr}
    bl hookabi_void_SaveFile_SwapFaroresWind
    pop {r0-r12, lr}
    bx lr

HOOK SkipTimeTravelCutsceneOne
    push {r0-r12, lr}
    bl hookabi_void_TimeTravelAdvanceCutsceneTimer
    pop {r0-r12, lr}
    ldmia sp!,{r4,r5,r6,pc}

HOOK SkipTimeTravelCutsceneTwo
    push {r0-r12, lr}
    bl hookabi_void_SetTimeTraveled
    pop {r0-r12, lr}
    mov r1,#0x324
    bx lr

HOOK SkipMasterSwordFanfare
    push {r0-r12, lr}
    bl hookabi_value_ShouldSkipMasterSwordCutscene
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x3E5F7C
    mov r1,#0x0
    bx lr

HOOK EnteredLocation
    cpy r4,r0
    push {r0-r12, lr}
    bl hookabi_void_Entrance_EnteredLocation
    pop {r0-r12, lr}
    bx lr

HOOK LostWoodsBridgeMusic
    push {r0-r12, lr}
    bl hookabi_value_Entrance_IsLostWoodsBridge
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK BeforeLoadGame
    add r0, r4, r5
    push {r0-r12, lr}
    bl hookabi_void_SaveFile_BeforeLoadGame
    pop {r0-r12, lr}
    b ret_BeforeLoadGame

HOOK AfterLoadGame
    push {r0-r12, lr}
    bl hookabi_void_SaveFile_AfterLoadGame
    pop {r0-r12, lr}
    pop {r4-r6, pc}

HOOK FileSelect_CopyFile
    push {r0-r12, lr}
    bl hookabi_void_SaveFile_BeforeCopy
    pop {r0-r12, lr}
    sub sp,sp,#0x240
    bx lr

HOOK SaveGame
    cmp r5, #0
    beq .notSaving
    push {r0-r12, lr}
    ldrb r0, [r5, #42]
    adds r0, #-0x30
    bl hookabi_void_SaveFile_SaveExtSaveData
    pop {r0-r12, lr}
.notSaving:
    push {r4-r9, lr}
    b 0x2fbfac

HOOK SaveMenuIgnoreOpen
    push {r0-r12, lr}
    bl hookabi_value_SaveMenu_IgnoreOpen
    cmp r0,#0x1
    pop {r0-r12, lr}
    addeq lr,lr,0xC
    bx lr

HOOK GameOverStart
    push {r0-r12, lr}
    bl hookabi_void_SaveFile_OnGameOver
    pop {r0-r12, lr}
    bx lr

HOOK GameOverMenuSkip
    push {r1-r12, lr}
    bl hookabi_value_GameOverMenu_ShouldSkip
    cmp r0,#0x0
    pop {r1-r12, lr}
    movne r0,#0x10
    moveq r0,#0x2
    bx lr

HOOK GameOverActionSelection
    bxne lr
    push {r0-r7, r9-r12, lr}
    cpy r0,r8
    bl hookabi_value_GameOverMenu_OverrideAction
    cpy r8,r0
    pop {r0-r7, r9-r12, lr}
    cmp r8,#0x1
    bx lr

HOOK OverrideFogDuringGameplayInit
    push {r0-r12, lr}
    bl hookabi_void_Fog_OverrideState
    pop {r0-r12, lr}
    bx lr

HOOK SkipTwinrovaQuarrelCutscene
    mov r0,#0x500
    add r0,r0,#0x9
    bx lr

HOOK FixItemsMenuSlotDuplication
    mov r4,#0xFF
    mov lr,#0xFF
    add r10,r10,#0x1
    b ret_FixItemsMenuSlotDuplication

HOOK PlayEntranceCutscene
    push {r0-r12, lr}
    @ r0=flag
    bl hookabi_value_EntranceCutscene_ShouldPlay
    eor r0,r0,#0x1 @ flip condition
    cmp r0,#0x0
    pop {r0-r12, lr}
    addne lr,lr,#0xC @ skip cutscene
    bxne lr
    b EventCheck @ original instruction

HOOK SkipJabuOpeningCutscene
    ldrh r0,[r0,#0x0]
    push {r0-r12, lr}
    bl hookabi_void_Jabu_SkipOpeningCutscene
    pop {r0-r12, lr}
    bx lr

HOOK MultiplyPlayerSpeed
    vldr.32 s0,[r6,#0x21C]
    push {r0-r12, lr}
    bl hookabi_value_Player_GetSpeedMultiplier
    vmov s1,r0
    pop {r0-r12, lr}
    vmul.f32 s0,s1
    bx lr

HOOK RunAnimationSpeed
    vldr.32 s17,[r5,#0x21C]
    push {r0-r12, lr}
    bl hookabi_value_Player_GetSpeedMultiplier
    vmov s1,r0
    pop {r0-r12, lr}
    vmul.f32 s17,s1
    bx lr

HOOK NaviNotifications
    push {r0-r12, lr}
    bl hookabi_value_Navi_GetNotificationOption
    cmp r0,#0x1
    pop {r0-r12, lr}
    # Silenced
    blt 0x267E3C
    # Constant
    bgt 0x267E14
    # Normal
    ldr r0,[r0,#0x4C]
    bx lr

HOOK ChestMinigame_KeyChestVisibility
    push {r0-r12, lr}
    bl hookabi_value_Settings_GetChestMinigameOption
    cmp r0,#0x0
    pop {r0-r12, lr}
    orrne r10,r7,#0x0
    orreq r10,r7,#0x4000
    bx lr

HOOK ChestMinigame_DontOpenChestsOnInit
    cmp r0,#0x0
    bxeq lr
    push {r0-r12, lr}
    bl hookabi_value_Settings_GetChestMinigameOption
    cmp r0,#0x1
    cmpgt r0,r0
    pop {r0-r12, lr}
    bx lr

HOOK GameplayDestroy
    cpy r4,r0
    push {r0-r12, lr}
    bl hookabi_void_Entrance_CheckEpona
    pop {r0-r12, lr}
    bx lr

HOOK SceneExitOverride
    ldrsh r9, [r1,r0]
    push {r0-r8, r10-r12, lr}
    cpy r0, r9
    bl hookabi_value_Entrance_OverrideNextIndex
    cpy r9, r0
    pop {r0-r8, r10-r12, lr}
    bx lr

HOOK SceneExitDynamicOverride
    push {r0-r12, lr}
    bl hookabi_void_Entrance_OverrideDynamicExit
    pop {r0-r12, lr}
    bx lr

HOOK OverrideGrottoActorEntrance
    push {r0-r12, lr}
    cpy r0, r4
    bl hookabi_void_Grotto_OverrideActorEntrance
    pop {r0-r12, lr}
    b 0x3F22C4

HOOK ReturnFW
    push {r0-r12, lr}
    bl hookabi_void_Grotto_SetupReturnInfoOnFWReturn
    pop {r0-r12, lr}
    add sp,sp,#0x8
    bx lr

HOOK WarpSongEntranceOverride
    push {r0-r1, r3-r12, lr}
    cpy r0, r2
    bl hookabi_value_Entrance_OverrideNextIndex
    cpy r2, r0
    pop {r0-r1, r3-r12, lr}
    strh r2,[r1,#0x1c]
    bx lr

HOOK OwlEntranceOverride
    push {r0, r2-r12, lr}
    cpy r0, r1
    bl hookabi_value_Entrance_OverrideNextIndex
    cpy r1, r0
    pop {r0, r2-r12, lr}
    b SetNextEntrance

HOOK SavewarpSetRespawnFlag
    push {r0-r12, lr}
    bl hookabi_void_Grotto_ForceGrottoReturnOnSpecialEntrance
    pop {r0-r12, lr}
    mov r0,#0xFF
    bx lr

HOOK AdultItemsCMABsAsChild
    push {r0-r12, lr}
    bl hookabi_value_Player_ShouldApplyAdultItemsCMABs
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK Model_EnableMeshGroupByIndex
    push {r0,r2-r12,lr}
    bl hookabi_value_Model_OverrideMesh
    cpy r1,r0
    pop {r0,r2-r12,lr}
    b 0x4C8B8C

HOOK ArrowsOrSeeds
    push {r0-r12, lr}
    bl hookabi_value_Player_ShouldUseSlingshot
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK HookshotDrawChain
    push {r0-r12, lr}
    bl hookabi_value_Player_IsAdult
    cmp r0,#0x0
    pop {r0-r12, lr}
    beq 0x2202BC
    ldr r0,[r4,#0x290]
    b 0x2202A4

HOOK HookshotRotation
    push {r0-r12, lr}
    bl hookabi_value_Hookshot_GetZRotation
    vmov.f32 s0,r0
    pop {r0-r12, lr}
    bx lr

HOOK LinkReflection
    push {r0-r12, lr}
    bl hookabi_value_Player_IsAdult
    cmp r0,#0x1
    pop {r0-r12, lr}
    streq r1,[r0,#0x714]
    bx lr

HOOK ChildCanOpenBowSubMenu
    push {r0-r12, lr}
    bl hookabi_value_Settings_BowAsChild
    cmp r0,#0x1
    pop {r0-r12, lr}
    beq 0x2EB2DC
    cmp r12,#0x0
    b 0x2EB2DC

HOOK RedBoulderExplode
    ldrb r0,[r5,#0x1B5]
    push {r0-r12, lr}
    cpy r0,r5
    bl hookabi_value_ObjHamishi_HitCount
    cmp r0,#0x2
    pop {r0-r12, lr}
    bge 0x26FE9C
    b 0x26FE80

HOOK Multiplayer_UpdatePrevActorFlags
    str r0,[r5,#0x1b8]
    push {r0-r12, lr}
    bl hookabi_void_Multiplayer_Sync_UpdatePrevActorFlags
    pop {r0-r12, pc}

HOOK Multiplayer_OnLoadFile
    strh r6,[r0,#0x4C]
    push {r0-r12, lr}
    bl hookabi_void_Multiplayer_OnFileLoad
    pop {r0-r12, lr}
    bx lr

HOOK SendDroppedBottleContents
    add r0,r0,#0x8C
    push {r0-r12, lr}
    cpy r0,r2
    vmov r1,s0
    vmov r2,s1
    vmov r3,s2
    bl hookabi_void_SendDroppedBottleContents
    pop {r0-r12, lr}
    bx lr

HOOK IgnoreMaskReaction
    ldrh r0,[r0,#0x0]
    push {r0-r12, lr}
    cpy r0,r4
    bl hookabi_value_SaveFile_GetIgnoreMaskReactionOption
    cmp r0,#0x1
    pop {r0-r12, lr}
    moveq r0,#0x0
    b 0x36BBC8

HOOK MasterQuestGoldSkulltulaCheck
    push {r0-r5,r7-r12, lr}
    bl hookabi_value_Settings_IsMasterQuestDungeon
    cpy r6,r0
    pop {r0-r5,r7-r12, lr}
    b 0x3415CC

HOOK WaterSpoutMasterQuestCheck
    push {r1-r12, lr}
    bl hookabi_value_Settings_IsMasterQuestDungeon
    pop {r1-r12, lr}
    bx lr

HOOK PierreSoftlockFixTwo
    cpy r6,r1
    push {r0-r12, lr}
    mov r2,#0x1
    mov r1,#0x0
    cpy r0,r6
    bl 0x36E980
    pop {r0-r12, lr}
    bx lr

HOOK StoreTargetActorType
    push {r0-r12, lr}
    cpy r0,r6
    bl hookabi_void_Fairy_StoreTargetActorType
    pop {r0-r12, lr}
    cmp r5,#0x0
    bx lr

HOOK ForceTrailEffectUpdate
    push {r1-r12, lr}
    cpy r0,r4
    bl hookabi_value_forceTrailEffectUpdate
    pop {r1-r12, lr}
    cmp r0,#0x10
    bx lr

HOOK RainbowSwordTrail
    push {r0-r12, lr}
    bl hookabi_void_updateSwordTrailColors
    pop {r0-r12, lr}
    add r8,r7,#0x100
    bx lr

HOOK BoomerangTrailEffect
    push {r0-r12, lr}
    bl hookabi_value_updateBoomerangTrailEffect
    cmp r0,#0x1
    pop {r0-r12, lr}
    bne 0x1F4228
    strb r4,[r0,#0x282]
    bx lr

HOOK RainbowChuTrail
    push {r0-r12, lr}
    bl hookabi_value_updateChuTrailColors
    cmp r0,#0x1
    pop {r0-r12, lr}
    addne pc,lr,#0x4
    strb r7,[r0,#0x282]
    bx lr

HOOK TimerExpiration
    mov r0,#0x5
    push {r0-r12,lr}
    bl hookabi_value_IceTrap_IsCurseActive
    cmp r0,#0x1
    pop {r0-r12,lr}
    bxne lr
    add lr,lr,#0x30
    mov r0,#0x0
    strh r0,[r4,#0x62]
    bx lr

HOOK FWandWarpSongTimerDepletion
    moveq r1,#0x1
    movne r1,#0xEF
    push {r0-r12,lr}
    bl hookabi_value_IceTrap_IsCurseActive
    cmp r0,#0x1
    pop {r0-r12,lr}
    bxeq lr
    strh r1,[r0,#0x64]
    bx lr

HOOK Timer2TickSound
    push {r0-r12,lr}
    bl hookabi_value_IceTrap_IsCurseActive
    cmp r0,#0x1
    pop {r0-r12,lr}
    addeq lr,lr,#0x4
    cmp r0,#0x3C
    bx lr

HOOK CurseTrapDizzyStick
    push {r0-r12,lr}
    bl hookabi_void_IceTrap_ReverseStick
    pop {r0-r12,lr}
    b 0x2FF258

HOOK CurseTrapDizzyButtons
    add r4,r5,#0x4
    push {r0-r12,lr}
    @ [R4+0] and [R4+4] contain button status fields
    @ Apply the curse effect to both
    add r0,r4,#0x0
    bl hookabi_void_IceTrap_RandomizeButtons
    add r0,r4,#0x4
    bl hookabi_void_IceTrap_RandomizeButtons
    pop {r0-r12,lr}
    bx lr

HOOK CrouchStabHitbox
    push {r0-r12,lr}
    bl hookabi_value_IceTrap_IsSlashHitboxDisabled
    cmp r0,#0x0
    pop {r0-r12,lr}
    movne r10,#0xFF
    strb r10,[r6,#0x227]
    bx lr

HOOK BossChallenge_Enter
    push {r0-r12,lr}
    bl hookabi_void_BossChallenge_Enter
    pop {r0-r12,lr}
    cpy r4,r0
    bx lr

HOOK BossChallenge_ExitMenu
    push {r0-r12,lr}
    cpy r0,r8
    bl hookabi_void_BossChallenge_ExitMenu
    pop {r0-r12,lr}
    cmp r8,#0x0
    bx lr

HOOK RestoreISG
    @ Keep the original native call aligned after restoring its arguments.
    push {r4,lr}
    push {r0-r12}
    bl hookabi_value_Settings_IsIsgEnabled
    cmp r0,#0x0
    pop {r0-r12}
    bleq 0x34BBFC @Function that cancels ISG
    pop {r4,lr}
    bx lr

HOOK GrannyTextID
    push {r1-r12,lr}
    bl hookabi_value_EnDs_GetTextID
    pop {r1-r12,lr}
    bx lr

HOOK GrannyBottleCheck
    push {r0-r12,lr}
    bl hookabi_value_EnDs_ShouldIgnoreBottle
    cmp r0,#0x0
    pop {r0-r12,lr}
    beq 0x377A04 @Check for bottle
    bx lr

HOOK GrannyItemOverride
    push {r0,r1,r3-r12,lr}
    bl hookabi_value_EnDs_ItemOverride
    cpy r2,r0
    pop {r0,r1,r3-r12,lr}
    b Actor_OfferGetItem

HOOK GrannySetRewardFlag
    push {r0-r12,lr}
    bl hookabi_void_EnDs_SetRewardFlag
    pop {r0-r12,lr}
    mov r0,#0x0
    bx lr

HOOK BecomeAdult
    strh r3,[r2,#0x5e]
    push {r0-r12,lr}
    bl hookabi_value_SaveFile_BecomeAdult
    cmp r0,#0x0
    pop {r0-r12,lr}
    ldrb r3,[r2,#0x60]
    bx lr

HOOK SwordlessPatchCheck
    cmp r1,#0x0
    bxne lr
    push {r0-r12,lr}
    bl hookabi_value_SaveFile_SwordlessPatchesEnabled
    cmp r0,#0x0
    pop {r0-r12,lr}
    bx lr

HOOK HandleBButton
    cmp r0,#0x3B
    cmpne r0,#0x3C
    cmpne r0,#0x3D
    cmpne r0,#0xFE
    bx lr

HOOK LoadFileSwordless
    push {lr}
    push {r0-r12}
    bl hookabi_void_SaveFile_LoadFileSwordless
    pop {r0-r12}
    bl hook_SwordlessPatchCheck
    pop {lr}
    bx lr

HOOK GanonRestoreMSOnDeath
    tst r0,r1
    bxne lr
    push {r0-r12,lr}
    bl hookabi_value_SaveFile_SwordlessPatchesEnabled
    cmp r0,#0x0
    pop {r0-r12,lr}
    bx lr

HOOK CriticalHealthCheck
    cmp r0,#0x10
    movle r0,#0x00
    bxle lr
    cmp r0,#0x50
    movle r0,#0x10
    bxle lr
    cmp r0,#0xA0
    movle r0,#0x18
    bx lr

HOOK InitSceneMirrorWorld
    push {r0-r12,lr}
    bl hookabi_void_Entrance_UpdateMQFlag
    pop {r0-r12,lr}
    cpy r4,r0
    bx lr

HOOK InitSceneEntranceOverride
    push {r0-r12,lr}
    bl hookabi_void_Entrance_OverrideSpawnScene
    pop {r0-r12,lr}
    mov r0, #0x14
    bx lr

HOOK CollisionATvsAC
    ldr r12,[sp,#0x18]
    push {r0-r12,lr}
    cpy r0,r1  @ AT collider
    cpy r1,r12 @ AC collider
    bl hookabi_value_Actor_CollisionATvsAC
    cmp r0,#0x1
    pop {r0-r12,lr}
    bxeq lr
    b 0x3192E4

HOOK CollisionCheck_SetAll_Once
    cpy r5,r2
    push {r0-r12,lr}
    bl hookabi_value_HyperActors_GetExtraUpdate
    cmp r0,#0x1
    pop {r0-r12,lr}
    addeq lr,#0x8
    bx lr

HOOK GanonDrawMasterSword
    ldrh r0,[r4,#0x2E]
    push {r0-r12,lr}
    bl hookabi_value_SaveFile_SwordlessPatchesEnabled
    cmp r0,#0x0
    pop {r0-r12,lr}
    bxeq lr
    strb r10,[r4,#0x0] @ delete MS effect
    bx lr

HOOK SetFWPlayerParams
    push {r0-r9,r11-r12,lr}
    bl hookabi_value_Grotto_ChooseFWPlayerParams
    mov r10,r0
    pop {r0-r9,r11-r12,lr}
    bx lr

HOOK AboutToPickUpActor
    ldrh r0,[r7]
    push {r0-r12,lr}
    mov r0,r7
    bl hookabi_value_Player_CanPickUpThisActor
    cmp r0,#0x0
    pop {r0-r12,lr}
    subeq lr,lr,#0x8
    bx lr

HOOK TargetReticleColor
    mov r4,#0x0
    push {r0-r12,lr}
    cpy r0,r6 @ Target Context
    bl hookabi_value_Fairy_SetTargetReticleColor
    cmp r0,#0x0
    pop {r0-r12,lr}
    bxeq lr @ no custom Navi colors, return to original code
    b ret_TargetReticleColor @ colors applied, skip original code

HOOK TargetPointerColor
    ldr r0,[r6,#0x120]
    push {r0-r12,lr}
    cpy r0,r6 @ Target Context
    cpy r1,r4 @ Targeted actor
    bl hookabi_value_Fairy_SetTargetPointerColor
    cmp r0,#0x0
    pop {r0-r12,lr}
    bxeq lr @ no custom Navi colors, return to original code
    b ret_TargetPointerColor @ colors applied, skip original code

HOOK ShadowShip_Speed
    add r0,r4,#0x6C
    push {r0-r12,lr}
    vpush {s0-s1}
    bl hookabi_value_ShadowShip_HasActiveStalfos
    vpop {s0-s1}
    cmp r0,#0x1
    mov r0,#0x5
    vmov s2,r0
    vcvt.f32.s32 s2,s2
    pop {r0-r12,lr}
    vmul.f32 s1,s2
    bxeq lr
    vmul.f32 s0,s2
    bx lr

HOOK MaskSalesmanBorrowMask
    ldrsh r1,[r6,#0x1C]
    push {r0-r12,lr}
    cpy r0,r1
    bl hookabi_void_SaveFile_BorrowMask
    pop {r0-r12,lr}
    bx lr

HOOK MaskSalesmanGiveMaskOfTruth
    orr r1,r1,#0x400
    push {r0-r12,lr}
    mov r0,#0x22 @ Mask of Truth SI id
    bl hookabi_void_SaveFile_BorrowMask
    pop {r0-r12,lr}
    bx lr

HOOK OoBBombchuOne
    cmp r5,#0x0
    bxeq lr
    ldrsh r0,[r5,#0xA]
    bx lr

HOOK OoBBombchuTwo
    cmp r5,#0x0
    bxeq lr
    ldrsh r0,[r5,#0xC]
    bx lr

HOOK OoBBombchuThree
    cmp r5,#0x0
    bxeq lr
    ldrsh r0,[r5,#0xE]
    bx lr

HOOK BombchuShopInfinitePurchases
    ldrsh r3,[r1,#0x1C]
    push {r0-r12,lr}
    bl hookabi_value_Settings_IsLogicVanilla
    cmp r0,#0x1
    pop {r0-r12,lr}
    movne r3,#0x0 @ Skip setting itemGetInf flag
    b 0x188D40

HOOK CamRoll
    push {r0,lr}
    mov r0,#0x0
    strh r0,[r5,#0xA2]
    pop {r0}
    blx r1
    push {r0}
    ldrh r0,[r5,#0xA2]
    bl hookabi_value_IceTrap_CamRoll
    strh r0,[r5,0xA2]
    pop {r0,lr}
    bx lr

HOOK CamUpdate
    push {r0-r12,lr}
    cpy r0,r1
    bl hookabi_value_Camera_FreeCamEnabled
    cmp r0,#0x0
    pop {r0-r12,lr}
    cpyeq r6,r0
    bxeq lr
    bl hookabi_void_Camera_FreeCamUpdate
    ldmia sp!,{r4-r11,pc}

HOOK Sheik_GetTextID
    push {r0, r2-r12, lr}
    cpy r0,r4
    bl hookabi_value_Sheik_GetTextID
    cpy r1,r0
    pop {r0, r2-r12, lr}
    b 0x2A4B4C

HOOK OnActorSetup_SceneChange
    cpy r4,r5
    push {r0-r12, lr}
    cpy r0,r5
    cpy r1,r6
    bl hookabi_value_ActorSetup_OverrideEntry
    cmp r0,#0x1
    pop {r0-r12, lr}
    # Continue like normal
    bxne lr
    # Iterate actor entry pointer and skip
    add r5,#0x10
    b ret_OnActorSetup_SceneChange

HOOK AfterActorSetup_SceneChange
    strb r0,[r7,#0xC03]
    push {r0-r12, lr}
    bl hookabi_void_ActorSetup_Extra
    pop {r0-r12, lr}
    bx lr

HOOK OnActorSetup_RoomChange
    cpy r4,r6
    push {r0-r12, lr}
    cpy r0,r6
    cpy r1,r7
    bl hookabi_value_ActorSetup_OverrideEntry
    cmp r0,#0x1
    pop {r0-r12, lr}
    # Continue like normal
    bxne lr
    # Iterate actor entry pointer and skip
    add r6,#0x10
    b ret_OnActorSetup_RoomChange

HOOK AfterActorSetup_RoomChange
    strb r10,[r8,#0xC03]
    push {r0-r12, lr}
    bl hookabi_void_ActorSetup_Extra
    pop {r0-r12, lr}
    bx lr

HOOK RandomGsLoc_CustomTangibilityCheck
    sub sp,sp,#0x18
    push {r0-r12, lr}
    mov r1,#0x1
    mov r2,#0x0
    bl hookabi_value_Gs_HasAltLoc
    cmp r0,#0x1
    pop {r0-r12, lr}
    # Has original location, run original function
    bne 0x3415B4
    push {r0-r12, lr}
    bl hookabi_value_Gs_CustomTangibilityCheck
    cmp r0,#0x1
    pop {r0-r12, lr}
    # Return true
    beq 0x341BDC
    # Return false
    b 0x341BEC

HOOK RandomGsLoc_CustomTokenSpawnOffset
    vadd.f32 s0,s3,s0
    push {r0-r12, lr}
    cpy r0,r4
    mov r1,#0x1
    mov r2,#0x0
    bl hookabi_value_Gs_HasAltLoc
    cmp r0,#0x1
    pop {r0-r12, lr}
    bxne lr
    push {r0-r12, lr}
    cpy r0,r4
    bl hookabi_value_Gs_GetCustomTokenSpawnPos
    vldr s0,[r0,#0x0]
    vldr s1,[r0,#0x4]
    vldr s2,[r0,#0x8]
    pop {r0-r12, lr}
    bx lr

HOOK RandomGsLoc_BlockSpawn_Crate
    push {r0-r12, lr}
    cpy r0,r4
    mov r1,#0x2
    mov r2,#0x1
    bl hookabi_value_Gs_HasAltLoc
    cmp r0,#0x1
    pop {r0-r12, lr}
    # If true
    beq 0x3BCAA4
    # If false
    orr r0,r2,#0x8000
    bx lr

HOOK RandomGsLoc_BlockSpawn_Tree
    push {r0-r12, lr}
    cpy r0,r4
    mov r1,#0x3
    mov r2,#0x1
    bl hookabi_value_Gs_HasAltLoc
    cmp r0,#0x1
    pop {r0-r12, lr}
    # If true
    beq 0x16C47C
    # If false
    orr r0,r0,#0xE000
    bx lr

HOOK RandomGsLoc_BlockSpawn_Soil
    push {r0-r12, lr}
    cpy r0,r4
    mov r1,#0x4
    mov r2,#0x1
    bl hookabi_value_Gs_HasAltLoc
    cmp r0,#0x1
    pop {r0-r12, lr}
    # If true
    beq 0x3BF6D0
    # If false
    ldrh r0,[r4,#0x1C]
    bx lr

HOOK RandomGsLoc_SkipSoilJingle
    push {r0-r12, lr}
    mov r1,#0x4
    mov r2,#0x1
    bl hookabi_value_Gs_HasAltLoc
    cmp r0,#0x1
    pop {r0-r12, lr}
    # If true
    beq 0x15DC34
    # If false
    ldrsh r0,[r0,#0x1C]
    bx lr

HOOK ActorDraw
    push {r0-r12, lr}
    bl hookabi_void_Actor_rDraw
    pop {r0-r12, lr}
    bx lr

HOOK FlyingPotCollision
    strh r0,[r4,#0xBE]
    push {r0-r12, lr}
    cpy r0,r4 @ Actor
    bl hookabi_value_EnTuboTrap_OnImpact
    cmp r0,#0x1
    pop {r0-r12, lr}
    bne 0x11DEE4 @ Skip collision checks and return
    bx lr

HOOK FlyingTileCollision
    cpy r0,r5
    push {r0-r12, lr}
    cpy r0,r4 @ Actor
    bl hookabi_value_EnYukabyun_OnImpact
    cmp r0,#0x1
    pop {r0-r12, lr}
    addne lr,lr,#0x8 @ Skip setting actionFunc
    bx lr

HOOK ShabomAfterDamagePlayer
    push {r0-r12, lr}
    bl hookabi_value_Shabom_CheckEnemySoul
    cmp r0,#0x0
    pop {r0-r12, lr}
    beq 0x3B511C @ Skip popping
    strh r10,[r5,#0x80]
    bx lr

HOOK DodongoAfterSwallowBomb
    mov r1,#0xA
    push {r0-r12, lr}
    cpy r0,r4 @ Actor
    bl hookabi_value_Dodongos_AfterSwallowBomb_Normal
    cmp r0,#0x0
    pop {r0-r12, lr}
    bne 0x11E4F4
    bx lr

HOOK BabyDodongoAfterSwallowBomb
    mov r3,#0x8
    push {r0-r12, lr}
    cpy r0,r4 @ Actor
    bl hookabi_value_Dodongos_AfterSwallowBomb_Baby
    cmp r0,#0x0
    pop {r0-r12, lr}
    bne 0x1C4370
    bx lr

HOOK OcarinaNoteButtonsDraw
    push {r0-r12, lr}
    bl hookabi_void_OcarinaNotes_MoveButtons
    pop {r0-r12, lr}
    mov r2,r2,lsl #0x1 @ original code
    cmp r2,#0x0        @ original code
    mov r3,r3,lsl #0x1 @ original code
    bx lr

HOOK OcarinaNoteButtonsPress
    cpy r0,r6
    push {r1-r12, lr}
    bl hookabi_value_OcarinaNotes_HandleInputs
    pop {r1-r12, lr}
    bx lr

HOOK HandleTextControlCode
    ldrb r0,[r6,#0x4] @ Control Code identifier
    push {r0-r12, lr}
    cpy r1,r5 @ Text Object
    cpy r2,r3 @ Unk pointer
    bl hookabi_value_Message_HandleTextControlCode
    cmp r0,#0x0
    pop {r0-r12, lr}
    bxeq lr    @ Not a custom control char, resume base game code
    b 0x2E0ED4 @ Handled custom control char, skip base game code

HOOK CheckForTextControlCode
    push {r1-r12, lr}
    cpy r2,r5 @ Text Object
    cpy r3,r9 @ Char Index (loop counter)
    bl hookabi_value_Message_rCheckForControlCodes
    pop {r1-r12, lr}
    bx lr

HOOK PlayInit
    push {r0-r12, lr}
    bl hookabi_void_before_Play_Init
    pop {r0-r12, lr}
    cpy r5,r0
    bx lr

HOOK DeleteEquipment
    push {r0-r12, lr}
    cpy r0,r1 @ equipment type
    cpy r1,r4 @ equipment value
    bl hookabi_value_Equipment_OverrideDeletion
    cmp r0,#0x0
    pop {r0-r12, lr}
    strheq r2,[r12,#0xB6]
    cmp r1,#0x2
    bx lr

HOOK GetQuickItem
    ldrb r1,[r6,#0x0]
    push {r0-r12, lr}
    cpy r0,r1 @ item id
    bl hookabi_void_ItemOverride_GetQuickItem
    pop {r0-r12, lr}
    bx lr

HOOK GetObjectEntry_Generic
    push {r1-r12, lr}
    @ r0 = slot
    bl hookabi_value_Object_GetEntry
    pop {r1-r12, lr}
    bx lr

HOOK GetObjectEntry_33AB24
    push {r1-r12, lr}
    ldr r0,[r4,#0x4]
    ldr r0,[r0,r5,lsl #0x3] @ objectId
    bl hookabi_value_Object_FindEntryOrSpawn
    pop {r1-r12, lr}
    bx lr

HOOK ExtendObjectGetSlot
    push {r1-r12, lr}
    cpy r0,r1 @ objectId
    bl hookabi_value_ExtendedObject_GetSlot
    pop {r1-r12, lr}
    bx lr

HOOK OverrideObjectIsLoaded
    push {r1-r12, lr}
    @ r0,r1 = ObjectContext,slot
    bl hookabi_value_Object_IsLoaded
    pop {r1-r12, lr}
    bx lr

HOOK OverrideObjectIsLoadedForCutscenes
    push {r1-r12, lr}
    @ r0,r1 = ObjectContext,slot
    bl hookabi_value_Object_IsLoaded_ForCutscenes
    pop {r1-r12, lr}
    bx lr

HOOK AfterObjectListCommand
    push {r0-r12, lr}
    bl hookabi_void_ExtendedObject_AfterObjectListCommand
    pop {r0-r12, lr}
    mov r0,#0x1
    bx lr

HOOK GetObjectEntry_34F270
    push {r1-r12, lr}
    @ r0 = slot
    bl hookabi_value_Object_GetEntry
    pop {r1-r12, lr}
    b 0x34F274

HOOK AltHeadersCommand
    add r2,r7,r1
    push {r0-r12, lr}
    cpy r0,r2 @ alt headers pointers list
    bl hookabi_void_Scene_GetLoadedLayer
    pop {r0-r12, lr}
    bx lr

HOOK GohmaLarvaDeathSignalParent
    ldr r1,[r4,#0x124]
    @ if parent pointer is null, skip
    @ setting childrenGohmaState
    cmp r1,#0x0
    addeq lr,lr,#0x10
    bx lr

HOOK GohmaEggDeathSignalParent
    ldr r0,[r4,#0x124]
    @ if parent pointer is null, skip
    @ setting childrenGohmaState
    cmp r0,#0x0
    addeq lr,lr,#0x10
    bx lr

HOOK StalchildDespawn_13DB68
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_value_Stalchild_CanDespawn
    cmp r0,#0x1
    pop {r0-r12, lr}
    addne lr,lr,#0x10 @ skip other checks, don't despawn
    bxne lr
    cmp r0,r1 @ base game code
    bx lr

HOOK StalchildDespawn_366338
    cmp r0,#0x0
    bxne lr @ doesn't despawn
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_value_Stalchild_CanDespawn
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK SkullwalltulaAttack_35F834
    cpy r5,r0
    b SkullwalltulaAttack

HOOK SkullwalltulaAttack_35F328
    cpy r4,r0
SkullwalltulaAttack:
    push {r0-r12, lr}
    @ r0 = actor
    bl hookabi_value_Skullwalltula_ShouldAttack
    cmp r0,#0x0
    pop {r0-r12, lr}
    bxlt lr @ proceed with vanilla checks
    movne r0,#0x1 @ attack
    moveq r0,#0x0 @ don't attack
    pop {r4-r7,pc} @ return and skip vanilla checks

HOOK SkullwalltulaTargetRotation
    sxth r0,r0
    push {r1-r12, lr}
    cpy r1,r4 @ actor
    bl hookabi_value_Skullwalltula_GetTargetRotation
    pop {r1-r12, lr}
    bx lr

HOOK SkullKidPoacherSawCheck
    cmp r1,#0x32
    bxge lr @ higher than poacher's saw, resume vanilla code
    push {r0-r12, lr}
    @ less than poacher's saw, only kill actor if enemizer is off
    bl hookabi_value_SkullKid_IsRandomized
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK LeeverSandCheck
    cmpne r0,#0x7
    bxeq lr
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_value_Leever_ShouldSurviveOutsideSand
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK LeeverAfterSink
    push {r0-r12, lr}
    @ r0 = actor
    bl hookabi_void_Leever_AfterSink
    pop {r0-r12, lr}
    bx lr

HOOK LeeverAfterDie
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_void_Leever_AfterDie
    pop {r0-r12, lr}
    cmp r0,#0x0
    bx lr

HOOK PlayerCheckVoidOut
    ldrb r0,[r4,#0x2]  @ actor->type
    cmp r0,#0x2        @ ACTORTYPE_PLAYER
    addne lr,lr,#0x17C @ Dark Link, skip void out (USA: 0x132CE4)
    cmpeq r8,#0x0      @ Normal Player, continue as normal
    bx lr

HOOK EnBlkobj_SpawnDarkLink
    push {r1-r12, lr}
    cpy r0,r4 @ EnBlkobj actor
    bl hookabi_value_DarkLink_Spawn
    pop {r1-r12, lr}
    bx lr

HOOK EnBlkobj_FindDarkLink
    push {r1-r12, lr}
    bl hookabi_value_DarkLink_IsAlive
    pop {r1-r12, lr}
    bx lr

HOOK EnEncount1_SpawnStalchildWolfos
    cpy r1,r9
    push {r0-r12, lr}
    cpy r0,r9       @ this EnEncount1
    add r1,sp,#0xC  @ actorId (r3)
    add r2,sp,#0x44 @ params (Stack[0xc])
    bl hookabi_void_EnemySpawner_OverrideSpawnedActor
    pop {r0-r12, lr}
    bx lr

HOOK EnEncount1_SpawnLeever
    push {r0-r12, lr}
    cpy r0,r5       @ this EnEncount1
    add r1,sp,#0xC  @ actorId (r3)
    add r2,sp,#0x20 @ params (r8)
    bl hookabi_void_EnemySpawner_OverrideSpawnedActor
    pop {r0-r12, lr}
    str r8,[sp,#0xC]
    bx lr

HOOK EnEncount1_SetLeeverAimType
    push {r0}
    ldrh r0,[r7] @ actor->id
    cmp r0,#0x1C @ Leever actor id
    pop {r0}
    strheq r0,[r1,r7] @ Set aimType only if actor is Leever
    bx lr

HOOK DarkLinkPlayerRecoil
    cmp r1,r6
    bxne lr @ not recoiling
    push {r0-r12, lr}
    bl hookabi_value_DarkLink_ShouldOverridePlayerRecoilSpeed
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK BabyDodongoWallCheck
    tst r0,#0x8
    bxeq lr @ no wall detected, return
    push {r0-r12, lr}
    @ Prevent burrowing for soulless baby dodongos.
    cpy r0,r4
    bl hookabi_value_EnemySouls_IsInvulnerable
    cmp r0,#0x1
    pop {r0-r12, lr}
    bx lr

HOOK PeahatLarvaGroundCheck
    tsteq r1,#0x1
    bxeq lr @ did not hit ground
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_value_EnemySouls_IsInvulnerable
    cmp r0,#0x1
    pop {r0-r12, lr}
    @ Prevent death when hitting ground without soul.
    bx lr

HOOK RedeadCanFreezePlayer
    cmp r1,r0
    bxgt lr @ far away, resume vanilla checks
    ldr r0,const_RedeadMaxYDist
    vldr s0,[r4,#0x9C] @ actor->yDistToPlayer
    vabs.f32 s0,s0
    vmov r1,s0
    cmp r1,r0 @ add check on Y distance
    bx lr
const_RedeadMaxYDist:
    .float 200.0

HOOK DeadHandHandCanGrabPlayer
    cmp r0,r1
    bxgt lr @ far away, resume vanilla checks
    vpush {s0}
    ldr r0,const_DeadHandHandMaxYDist
    vldr s0,[r4,#0x9C] @ actor->yDistToPlayer
    vabs.f32 s0,s0
    vmov r1,s0
    cmp r1,r0 @ add check on Y distance
    vpop {s0}
    bx lr
const_DeadHandHandMaxYDist:
    .float 100.0

HOOK GerudoBattleMusic
    push {r0-r12, lr}
    bl hookabi_value_GerudoFighter_IsRandomized
    cmp r0,#0x0
    pop {r0-r12, lr}
    beq 0x34F724 @ start battle music
    bx lr

@ This fixes a base game bug where, after going through a room transition, actors under
@ certain conditions will get killed on every frame and never deleted.
@ Fig's explanation from OoT Discord: https://discord.com/channels/82938430371139584/82991320754294784/1002187734946947113
@ With Enemy Randomizer, these "zombie" enemy actors can prevent a room from being cleared.
HOOK FixActorKillLoop
    bxne lr @ object is loaded, continue without killing actor
    push {r0-r12, lr}
    @ check if actor is already killed so that it doesn't get
    @ killed again and it gets properly deleted instead
    cpy r0,r4 @ actor
    bl hookabi_value_Actor_IsKilled
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK AfterInvalidatingRoomObjects
    push {r0-r12, lr}
    bl hookabi_void_ExtendedObject_InvalidateRoomObjects
    pop {r0-r12, lr}
    ldr r0,[sp,#0x18]
    bx lr

HOOK DrawHeartIcon
    push {r0-r12, lr}
    cpy r0,r1 @ heart icon index
    bl hookabi_value_Gloom_ShouldDrawHeartBorder
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

HOOK OcarinaNoteSound_Player
    push {r0,r2,r3,lr}
    bl 0x2CFE00 @ original instruction, sets vibrato
    mov r0,#0x0 @ default ocarina instrument
    bl hookabi_value_OcarinaNotes_OverrideInstrument
    cpy r1,r0
    pop {r0,r2,r3,lr}
    b 0x2CFD24 @ set instrument

HOOK OcarinaNoteSound_Npc
    and r1,r0,#0xFF @ instrument
    push {r0, r2-r12, lr}
    cpy r0,r1
    bl hookabi_value_OcarinaNotes_OverrideInstrument
    cpy r1,r0
    pop {r0, r2-r12, lr}
    bx lr

HOOK Item00GiveAutomaticItemDrop
    ldrsh r0,[r4,#0x1c]
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_value_ItemOverride_GetItemDrop
    cmp r0,#0x0
    pop {r0-r12, lr}
    addne lr,lr,#0x8 @ Item overridden, skip Item_Give
    bx lr

HOOK Item00GiveCollectedItemDrop
    ldrsh r0,[r5,#0x1c]
    push {r0-r12, lr}
    cpy r0,r5 @ actor
    bl hookabi_value_ItemOverride_GetItemDrop
    cmp r0,#0x0
    pop {r0-r12, lr}
    addne lr,lr,#0x8 @ Item overridden, skip Item_Give
    bx lr

HOOK GanonFinalBlow
    push {r0-r12, lr}
    bl hookabi_void_Ganon_OnFinalBlow
    pop {r0-r12, lr}
    mov r1,#0x1
    bx lr

HOOK PlayerBonk
    strh r8,[r7,#0x38]
    push {r0-r12, lr}
    bl hookabi_void_Player_OnBonk
    pop {r0-r12, lr}
    bx lr

HOOK CmbManagerInit
    push {r0-r12, lr}
    @ r0 = cmbManager
    cpy r1,r4 @ zarInfo
    cpy r2,r5 @ cmbIndex
    bl hookabi_void_SoullessModels_BeforeCmbManagerInit
    pop {r0-r12, lr}
    ldr r1,[r4,#0x4C]
    bx lr

HOOK ActorInit
    push {r0-r12, lr}
    bl hookabi_void_Actor_rInit
    pop {r0-r12, lr}
    bx lr

HOOK BusinessScrubCheckFlags
    push {r0-r12, lr}
    cpy r0,r4 @ scrub actor
    bl hookabi_value_EnShopnnuts_rCheckFlags
    cmp r0,#0x0
    pop {r0-r12, lr}
    addeq lr,lr,#0x0C @  0: skip checks, don't kill actor
    addgt lr,lr,#0x10 @  1: kill actor
    cmplt r1,#0x2     @ -1: resume vanilla checks
    bx lr

HOOK SetupDoorShutter
    push {r0-r12, lr}
    cpy r0,r4 @ actor
    bl hookabi_value_DoorShutter_CheckSoullessEnemies
    cmp r0,#0x0
    pop {r0-r12, lr}
    bx lr

@ Return to the intercepted file-menu instruction after releasing UDS resources.
HOOK HostShutdown
    push {lr}
    bl hookabi_void_Multiplayer_Shutdown
    pop {lr}
    b 0x42f9d0
