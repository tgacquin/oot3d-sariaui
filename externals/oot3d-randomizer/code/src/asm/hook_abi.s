@ Assembly-to-C boundary for injected hooks (ARMv6K, softfp).
@ The game can enter a mid-function hook with a 4-byte-aligned stack and
@ live caller-saved integer/VFP registers or flags. Preserve everything except
@ the declared scalar result in r0; outer hooks still apply their own outputs.
@ All audited targets take at most four 32-bit register arguments. Stack
@ arguments, 64-bit/aggregate returns and hard-float calls need another bridge.
.arm
.syntax unified
.section .text.randomizer_hook_abi,"ax",%progbits

.macro ABI_BODY kind, value
.type hookabi_dispatch_\kind, %function
hookabi_dispatch_\kind:
    push {r0-r3,r5,r12}
    mov r5,sp
    bic sp,sp,#7
    vpush {d0-d7}
    mrs r12,cpsr
    vmrs lr,fpscr
    push {r12,lr}
    blx r4
.if \value
    str r0,[r5] @ replace saved r0 with the declared return value
.endif
    pop {r12,lr}
    msr CPSR_fs,r12
    vmsr fpscr,lr
    vpop {d0-d7}
    mov sp,r5
    pop {r0-r3,r5,r12}
    pop {r4,pc}
.size hookabi_dispatch_\kind, .-hookabi_dispatch_\kind
.endm
ABI_BODY void, 0
ABI_BODY value, 1

.macro ABI_BRIDGE target, kind
.global hookabi_\kind\()_\target
.type hookabi_\kind\()_\target, %function
hookabi_\kind\()_\target:
    push {r4,lr}
    ldr r4,1f
    b hookabi_dispatch_\kind
1:  .word \target
.size hookabi_\kind\()_\target, .-hookabi_\kind\()_\target
.endm

@ Keep this list checked against C declarations with hook_abi_audit.py.
ABI_BRIDGE ActorSetup_Extra, void
ABI_BRIDGE ActorSetup_OverrideEntry, value
ABI_BRIDGE Actor_CollisionATvsAC, value
ABI_BRIDGE Actor_IsKilled, value
ABI_BRIDGE Actor_rDraw, void
ABI_BRIDGE Actor_rInit, void
ABI_BRIDGE Actor_rUpdate, void
ABI_BRIDGE BgGjyoBridge_CheckCondition, value
ABI_BRIDGE Bgm_OverrideFanfare, value
ABI_BRIDGE BossChallenge_Enter, void
ABI_BRIDGE BossChallenge_ExitMenu, void
ABI_BRIDGE Camera_FreeCamEnabled, value
ABI_BRIDGE Camera_FreeCamUpdate, void
ABI_BRIDGE Chest_OverrideAnimation, value
ABI_BRIDGE Chest_OverrideIceSmoke, value
ABI_BRIDGE Cutscene_BlueWarpOverride, void
ABI_BRIDGE Cutscene_CheckEponasSongFlag, value
ABI_BRIDGE Cutscene_CheckLullabyFlag, value
ABI_BRIDGE Cutscene_CheckSariasSongFlag, value
ABI_BRIDGE Cutscene_OverrideBolero, void
ABI_BRIDGE Cutscene_OverrideEponasSong, void
ABI_BRIDGE Cutscene_OverrideFairyReward, void
ABI_BRIDGE Cutscene_OverrideLACS, void
ABI_BRIDGE Cutscene_OverrideLullaby, void
ABI_BRIDGE Cutscene_OverrideMinuet, void
ABI_BRIDGE Cutscene_OverrideNocturne, void
ABI_BRIDGE Cutscene_OverridePrelude, value
ABI_BRIDGE Cutscene_OverrideRequiem, value
ABI_BRIDGE Cutscene_OverrideSariasSong, void
ABI_BRIDGE Cutscene_OverrideSerenade, void
ABI_BRIDGE Cutscene_OverrideSongOfStorms, void
ABI_BRIDGE Cutscene_OverrideSongOfTime, void
ABI_BRIDGE Cutscene_OverrideSunsSong, void
ABI_BRIDGE Cutscene_SerenadeCheckChestFlag, value
ABI_BRIDGE DarkLink_IsAlive, value
ABI_BRIDGE DarkLink_ShouldOverridePlayerRecoilSpeed, value
ABI_BRIDGE DarkLink_Spawn, value
ABI_BRIDGE Dodongos_AfterSwallowBomb_Baby, value
ABI_BRIDGE Dodongos_AfterSwallowBomb_Normal, value
ABI_BRIDGE DoorOfTime_RequirementCheck, value
ABI_BRIDGE DoorShutter_CheckSoullessEnemies, value
ABI_BRIDGE Door_CheckToDeleteCustomModels, void
ABI_BRIDGE Draw_PreSwapBuffers, void
ABI_BRIDGE Dungeon_CheckDekuTreeClear, value
ABI_BRIDGE Dungeon_GetCurrentDungeonMode, value
ABI_BRIDGE EnBomBowlMan_CheckExplosives, value
ABI_BRIDGE EnCow_BottleCheck, value
ABI_BRIDGE EnCow_ItemOverride, value
ABI_BRIDGE EnDs_GetTextID, value
ABI_BRIDGE EnDs_ItemOverride, value
ABI_BRIDGE EnDs_SetRewardFlag, void
ABI_BRIDGE EnDs_ShouldIgnoreBottle, value
ABI_BRIDGE EnDu_CheckRewardFlag, value
ABI_BRIDGE EnFr_rSetReward, void
ABI_BRIDGE EnGm_CheckRewardFlag, value
ABI_BRIDGE EnGm_ItemOverride, value
ABI_BRIDGE EnGm_SetRewardFlag, void
ABI_BRIDGE EnGm_UseCustomText, value
ABI_BRIDGE EnGo2_rBiggoronSetTextId, void
ABI_BRIDGE EnGo2_rGetTextIdGoronDmtBiggoron, value
ABI_BRIDGE EnHs_CheckForShouldDespawn, void
ABI_BRIDGE EnJs_CheckRewardFlag, value
ABI_BRIDGE EnJs_SetRewardFlag, void
ABI_BRIDGE EnKo_CheckFadoCanSpawnInLostWoods, value
ABI_BRIDGE EnKo_CheckForestTempleBeat, value
ABI_BRIDGE EnKo_CheckOpenForest, value
ABI_BRIDGE EnKz_CheckMovedFlag, value
ABI_BRIDGE EnKz_SetTradedPrescriptionFlag, void
ABI_BRIDGE EnMd_GetTextKokiriForest, value
ABI_BRIDGE EnMd_ShouldSpawn, value
ABI_BRIDGE EnMk_SetTradedEyeballFrogFlag, void
ABI_BRIDGE EnMs_ModifyBeansBought, value
ABI_BRIDGE EnMs_ModifyPrice, value
ABI_BRIDGE EnMs_Purchase, void
ABI_BRIDGE EnNiwLady_CheckCuccoAmount, value
ABI_BRIDGE EnOwl_FastCutscene, void
ABI_BRIDGE EnPoRelay_CheckChestFlag, void
ABI_BRIDGE EnShopnnuts_rCheckFlags, value
ABI_BRIDGE EnTa_GetCastleTextbox, value
ABI_BRIDGE EnTk_SetRewardFlag, void
ABI_BRIDGE EnToryo_SetTradedSawFlag, void
ABI_BRIDGE EnTuboTrap_OnImpact, value
ABI_BRIDGE EnYukabyun_OnImpact, value
ABI_BRIDGE EnableFW, void
ABI_BRIDGE EnemySouls_IsInvulnerable, value
ABI_BRIDGE EnemySpawner_OverrideSpawnedActor, void
ABI_BRIDGE EntranceCutscene_ShouldPlay, value
ABI_BRIDGE Entrance_CheckEpona, void
ABI_BRIDGE Entrance_DeathInGanonBattle, void
ABI_BRIDGE Entrance_EnteredLocation, void
ABI_BRIDGE Entrance_IsLostWoodsBridge, value
ABI_BRIDGE Entrance_OverrideDynamicExit, void
ABI_BRIDGE Entrance_OverrideNextIndex, value
ABI_BRIDGE Entrance_OverrideSpawnScene, void
ABI_BRIDGE Entrance_SetGameOverEntrance, void
ABI_BRIDGE Entrance_SetSavewarpEntrance, void
ABI_BRIDGE Entrance_UpdateMQFlag, void
ABI_BRIDGE Equipment_OverrideDeletion, value
ABI_BRIDGE ExtendedObject_AfterObjectListCommand, void
ABI_BRIDGE ExtendedObject_Clear, void
ABI_BRIDGE ExtendedObject_GetSlot, value
ABI_BRIDGE ExtendedObject_InvalidateRoomObjects, void
ABI_BRIDGE FairyPickupHealAmount, void
ABI_BRIDGE FairyReviveHealAmount, value
ABI_BRIDGE FairyUseHealAmount, value
ABI_BRIDGE Fairy_SetTargetPointerColor, value
ABI_BRIDGE Fairy_SetTargetReticleColor, value
ABI_BRIDGE Fairy_StoreTargetActorType, void
ABI_BRIDGE Fog_OverrideState, void
ABI_BRIDGE FrogSongTimerMultiplier, value
ABI_BRIDGE GK_SetDurability, value
ABI_BRIDGE GameOverMenu_OverrideAction, value
ABI_BRIDGE GameOverMenu_ShouldSkip, value
ABI_BRIDGE Ganon_CSEquipMS, void
ABI_BRIDGE Ganon_GiveMSMidFight, void
ABI_BRIDGE Ganon_OnFinalBlow, void
ABI_BRIDGE GearMenu_GetRewardHint, value
ABI_BRIDGE GerudoFighter_IsRandomized, value
ABI_BRIDGE Gfx_AwakeCallback, void
ABI_BRIDGE Gfx_SleepQueryCallback, void
ABI_BRIDGE Gfx_Update, void
ABI_BRIDGE Gloom_ShouldDrawHeartBorder, value
ABI_BRIDGE Grotto_ChooseFWPlayerParams, value
ABI_BRIDGE Grotto_ForceGrottoReturn, void
ABI_BRIDGE Grotto_ForceGrottoReturnOnSpecialEntrance, void
ABI_BRIDGE Grotto_ForceRegularVoidOut, void
ABI_BRIDGE Grotto_OverrideActorEntrance, void
ABI_BRIDGE Grotto_SetupReturnInfoOnFWReturn, void
ABI_BRIDGE Gs_CustomTangibilityCheck, value
ABI_BRIDGE Gs_GetCustomTokenSpawnPos, value
ABI_BRIDGE Gs_HasAltLoc, value
ABI_BRIDGE Hints_AddSariasSongHint, void
ABI_BRIDGE Hints_CanReadHints, value
ABI_BRIDGE Hints_GetHintsSetting, value
ABI_BRIDGE Hints_GetNextSariasSongHint, value
ABI_BRIDGE Hookshot_GetZRotation, value
ABI_BRIDGE HyperActors_GetExtraUpdate, value
ABI_BRIDGE IceTrap_CamRoll, value
ABI_BRIDGE IceTrap_IsCurseActive, value
ABI_BRIDGE IceTrap_IsSlashHitboxDisabled, value
ABI_BRIDGE IceTrap_RandomizeButtons, void
ABI_BRIDGE IceTrap_ReverseStick, void
ABI_BRIDGE ItemEffect_EquipMasterSword, void
ABI_BRIDGE ItemOverride_DrawGetItem_ShouldSkipRotation, value
ABI_BRIDGE ItemOverride_EditDrawGetItemAfterMatrixUpdate, void
ABI_BRIDGE ItemOverride_EditDrawGetItemAfterModelSpawn, void
ABI_BRIDGE ItemOverride_EditDrawGetItemBeforeModelSpawn, void
ABI_BRIDGE ItemOverride_GetDrawItem, value
ABI_BRIDGE ItemOverride_GetItemDrop, value
ABI_BRIDGE ItemOverride_GetOverheadItem, value
ABI_BRIDGE ItemOverride_GetQuickItem, void
ABI_BRIDGE ItemOverride_GetSkulltulaToken, value
ABI_BRIDGE ItemOverride_GiveSariasGift, value
ABI_BRIDGE ItemOverride_OfferGetItem, void
ABI_BRIDGE ItemOverride_OverrideGiDrawIdPlusOne, value
ABI_BRIDGE Item_ConvertBombDrop, value
ABI_BRIDGE ItemsMenu_Draw, void
ABI_BRIDGE Jabu_SkipOpeningCutscene, void
ABI_BRIDGE KakGate_CheckToFixBug, value
ABI_BRIDGE Leever_AfterDie, void
ABI_BRIDGE Leever_AfterSink, void
ABI_BRIDGE Leever_ShouldSurviveOutsideSand, value
ABI_BRIDGE MagicWind_CheckSet, value
ABI_BRIDGE MagicWind_Unset, void
ABI_BRIDGE Message_GetCustomEntry, value
ABI_BRIDGE Message_GetCustomText, value
ABI_BRIDGE Message_HandleTextControlCode, value
ABI_BRIDGE Message_rCheckForControlCodes, value
ABI_BRIDGE Model_DrawByActor, value
ABI_BRIDGE Model_OverrideMesh, value
ABI_BRIDGE Multiplayer_OnFileLoad, void
ABI_BRIDGE Multiplayer_Sync_UpdatePrevActorFlags, void
ABI_BRIDGE Navi_GetNotificationOption, value
ABI_BRIDGE NoHealFromBombchuBowlingPrize, value
ABI_BRIDGE NoHealFromHealthUpgrades, value
ABI_BRIDGE ObjHamishi_HitCount, value
ABI_BRIDGE ObjKibako_CheckRuto, value
ABI_BRIDGE Object_FindEntryOrSpawn, value
ABI_BRIDGE Object_GetEntry, value
ABI_BRIDGE Object_IsLoaded, value
ABI_BRIDGE Object_IsLoaded_ForCutscenes, value
ABI_BRIDGE OcarinaNotes_HandleInputs, value
ABI_BRIDGE OcarinaNotes_MoveButtons, void
ABI_BRIDGE OcarinaNotes_OverrideInstrument, value
ABI_BRIDGE Pedestal_PickUpMasterSword, void
ABI_BRIDGE Player_CanPickUpThisActor, value
ABI_BRIDGE Player_EditAndRetrieveCMB, value
ABI_BRIDGE Player_GetCustomTunicCMAB, value
ABI_BRIDGE Player_GetSpeedMultiplier, value
ABI_BRIDGE Player_IsAdult, value
ABI_BRIDGE Player_OnBonk, void
ABI_BRIDGE Player_SetChildCustomTunicCMAB, void
ABI_BRIDGE Player_ShouldApplyAdultItemsCMABs, value
ABI_BRIDGE Player_ShouldUseSlingshot, value
ABI_BRIDGE SaveFile_AfterLoadGame, void
ABI_BRIDGE SaveFile_BecomeAdult, value
ABI_BRIDGE SaveFile_BeforeCopy, void
ABI_BRIDGE SaveFile_BeforeLoadGame, void
ABI_BRIDGE SaveFile_BorrowMask, void
ABI_BRIDGE SaveFile_CheckForPocketCuccoHatch, value
ABI_BRIDGE SaveFile_CheckForWeirdEggHatch, value
ABI_BRIDGE SaveFile_CheckGerudoToken, value
ABI_BRIDGE SaveFile_GetIgnoreMaskReactionOption, value
ABI_BRIDGE SaveFile_Init, void
ABI_BRIDGE SaveFile_LoadFileSwordless, void
ABI_BRIDGE SaveFile_MaskSlotValue, value
ABI_BRIDGE SaveFile_OnGameOver, void
ABI_BRIDGE SaveFile_RestoreChildEquips, value
ABI_BRIDGE SaveFile_SaveChildBButton, void
ABI_BRIDGE SaveFile_SaveExtSaveData, void
ABI_BRIDGE SaveFile_SwapFaroresWind, void
ABI_BRIDGE SaveFile_SwordlessPatchesEnabled, value
ABI_BRIDGE SaveMenu_IgnoreOpen, value
ABI_BRIDGE Scarecrow_CheckToBeActivated, value
ABI_BRIDGE Scene_GetLoadedLayer, void
ABI_BRIDGE Scene_Init, void
ABI_BRIDGE SendDroppedBottleContents, void
ABI_BRIDGE SetBGM, value
ABI_BRIDGE SetSFX, value
ABI_BRIDGE SetTimeTraveled, void
ABI_BRIDGE Settings_ApplyDamageMultiplier, value
ABI_BRIDGE Settings_BowAsChild, value
ABI_BRIDGE Settings_GetChestMinigameOption, value
ABI_BRIDGE Settings_GetSongReplaysOption, value
ABI_BRIDGE Settings_IsIsgEnabled, value
ABI_BRIDGE Settings_IsLogicVanilla, value
ABI_BRIDGE Settings_IsMasterQuestDungeon, value
ABI_BRIDGE Settings_IsTurboText, value
ABI_BRIDGE Settings_SunsSongEndCloseTextbox, void
ABI_BRIDGE Shabom_CheckEnemySoul, value
ABI_BRIDGE ShadowShip_HasActiveStalfos, value
ABI_BRIDGE Sheik_GetTextID, value
ABI_BRIDGE Shop_CheckCanBuyBombchus, value
ABI_BRIDGE ShortenRainbowBridgeCS, void
ABI_BRIDGE ShotSun_CheckChestFlag, value
ABI_BRIDGE ShouldSkipMasterSwordCutscene, value
ABI_BRIDGE SkullKid_IsRandomized, value
ABI_BRIDGE Skullwalltula_GetTargetRotation, value
ABI_BRIDGE Skullwalltula_ShouldAttack, value
ABI_BRIDGE SoullessModels_BeforeCmbManagerInit, void
ABI_BRIDGE Stalchild_CanDespawn, value
ABI_BRIDGE TimeTravelAdvanceCutsceneTimer, void
ABI_BRIDGE TitleCard_rUpdate, void
ABI_BRIDGE Triforce_IsWaitingForText, value
ABI_BRIDGE after_GlobalContext_Update, void
ABI_BRIDGE before_GlobalContext_Update, void
ABI_BRIDGE before_Play_Init, void
ABI_BRIDGE forceTrailEffectUpdate, value
ABI_BRIDGE isFishing, value
ABI_BRIDGE updateBoomerangTrailEffect, value
ABI_BRIDGE updateChuTrailColors, value
ABI_BRIDGE updateSwordTrailColors, void

ABI_BRIDGE Multiplayer_Shutdown, void
