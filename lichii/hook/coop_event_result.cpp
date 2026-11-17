// SPDX-License-Identifier: GPL-3.0-or-later
// Row 98(2) result only: see coop_event_result.h.
#include "coop_event_result.h"

#include <algorithm>
#include <cstdio>

#include "coop/protocol/codec.hpp"
#include "coop_live_events.h"
#include "coop_logic_set.h"

namespace CoopEventResult {

namespace {
// Generated: tools/puppet_pack/eye_switches.py --romfs <romfs> --places lichii/hook/coop_live_places.cpp
// --dungeon lichii/hook/coop_dungeon.cpp --results (189 askers of 186 events, 147 live, 42 asker; 12 events a field
// asks). The comment says the class and why (for class asker: the result the room reads from the asker).
// clang-format off
constexpr Result kResults[] = {
    {"BottomOfTheWell_08_Enemy_WipedOut", "BottomOfTheWell", {-0.0f, 92.3f, -108.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"BottomOfTheWell_SilverToken_Appear", "BottomOfTheWell", {-12.9f, 95.9f, -7.5f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Start; Obj_silverRupeeCounter.TokenVisible; unshared flag HaveEncountered_Token
    {"BottomOfTheWell_SilverToken_Unlock", "BottomOfTheWell", {-17.6f, 95.9f, -12.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"Dm_DodongoCave_0010", "DodongoCavern", {-49.0f, 115.5f, -45.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"Dm_FireTemple_0029", "FireTemple", {-46.9f, 186.5f, -22.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"Dm_ForestTemple_0014", "ForestTemple", {60.3f, 115.8f, 0.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"Dm_InsideGanonCastle_0035", "InsideGanonCastle", {-10.5f, 108.0f, -47.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"DodongoCavern_Gimmick_CandleStickShutter", "DodongoCavern", {32.5f, 108.0f, -44.5f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_DodongoCave_SwitchShutter_A_01.LockPlusZ; Obj_SwitchShutter_A_06.IsDisableBlend; Obj_SwitchShutter_A_06.Open; unshared flag DodongoCavern_NaviHelp_08_00
    {"DodongoCavern_Gimmick_DefeatLizalfos01", "DodongoCavern", {84.9f, 100.5f, -54.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"DodongoCavern_Gimmick_DefeatLizalfos02", "DodongoCavern", {94.3f, 114.9f, -49.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"DodongoCavern_Gimmick_EyeSwitch", "DodongoCavern", {68.2f, 127.3f, -6.6f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_DodongoCave_FireWall_A_01.RequestExtinguish; Obj_DodongoCave_FireWall_A_01.StartBurnSign; unshared flag DodongoCavern_NaviHelp_14_00
    {"DodongoCavern_Gimmick_StatueSwitch", "DodongoCavern", {80.0f, 102.0f, -6.5f}, "EventBool+EventFinish"},  // asker (EventRequestActor): unshared flag DodongoCavern_NaviHelp_02_00
    {"DodongoCavern_Gimmick_SwitchShortcut", "DodongoCavern", {19.0f, 118.0f, -23.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"DodongoCavern_Gimmick_SwitchShutter_Room01", "DodongoCavern", {16.0f, 103.8f, -23.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"DodongoCavern_Gimmick_SwitchShutter_Room12", "DodongoCavern", {-43.5f, 116.0f, -8.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"DodongoCavern_Gimmick_SwitchShutter_Room19", "DodongoCavern", {14.8f, 100.3f, -77.9f}, "EventBool+EventFinish"},  // asker (EventRequestActor): unshared flag DodongoCavern_NaviHelp_19_04
    {"FireTemple_06_EyeSwitch", "FireTemple", {36.7f, 150.8f, 3.9f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_SwitchShutter_A_02.IsDisableBlend
    {"FireTemple_15_OpenShutter", "FireTemple", {7.3f, 103.5f, -12.0f}, "EventBool"},  // asker (EventRequestActor): Obj_EnemyCounter_A_01.Finish
    {"FireTemple_LargePillarFallCamera", "FireTemple", {-25.4f, 143.2f, -3.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"FireTemple_MovingFireWallCamera_Room16", "FireTemple", {28.0f, 149.1f, 17.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"FireTemple_PutOutFireWallTrap", "FireTemple", {-61.3f, 156.2f, -15.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"FireTemple_Room24_FocusOnGimmick", "FireTemple", {-72.2f, 149.9f, 3.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"FireTemple_SwitchFireCamera_Room04", "FireTemple", {93.1f, 141.0f, 16.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"FireTemple_SwitchFireCamera_Room12", "FireTemple", {-67.5f, 187.3f, 14.2f}, ""},  // live (EventRequestActor): results through shared flags
    {"FocusGimmick_BlueTentacleGimmick", "InsideJabuJabuBelly", {0.4f, 103.5f, -147.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"FocusGimmick_GreenTentacleGimmick", "InsideJabuJabuBelly", {-2.2f, 104.4f, -85.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"FocusGimmick_RedTentacleGimmick0", "InsideJabuJabuBelly", {32.8f, 101.2f, -139.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"FocusOnGimmick_FireTemple_FlareDancer02", "FireTemple", {-7.0f, 102.0f, -35.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"FocusOnGimmick_InsideJabuJabuBelly_02", "InsideJabuJabuBelly", {20.0f, 79.5f, -35.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"FocusOnGimmick_JabujabuLift", "InsideJabuJabuBelly", {-1.0f, 77.5f, -41.2f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room02_ShutterOpen", "ForestTemple", {16.4f, 101.7f, -12.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room04_OpenDoor_AppearTBox", "ForestTemple", {0.0f, 105.7f, -60.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room11_TBoxAppear", "ForestTemple", {-52.8f, 115.0f, 2.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room12_WaterLevelDrop", "ForestTemple", {-29.1f, 107.4f, -34.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room17_BigTBoxAppear", "ForestTemple", {0.2f, 114.7f, -66.2f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room17_ShutterOpen", "ForestTemple", {-7.5f, 121.5f, -59.5f}, "EventBool"},  // asker (EventRequestActor): unshared flag ForestTemple_NaviHelp_17_03
    {"ForestTemple_Room17_TBoxAppear_CandleLight", "ForestTemple", {-19.3f, 114.7f, -63.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room17_TBoxAppear_CandleLight_ShutterOpen", "ForestTemple", {19.3f, 114.7f, -63.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room17_TwoStalfosAppear", "ForestTemple", {4.8f, 132.8f, -60.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room21_ShutterOpen", "ForestTemple", {-58.2f, 105.1f, -60.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room22_FallingCeiling", "ForestTemple", {55.9f, 120.3f, -43.9f}, "EventBool"},  // asker (EventRequestActor): Obj_ForestTemple_FallingCeiling_A_01.BootUp; Obj_ForestTemple_FallingCeiling_A_01.IsForceFall
    {"ForestTemple_Room22_IceMelts", "ForestTemple", {56.5f, 137.7f, -10.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room22_ShutterOpen", "ForestTemple", {54.3f, 104.9f, -19.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room22_ShutterOpen_TBoxAppear", "ForestTemple", {-68.0f, 107.0f, -26.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room25_CandleLight_ShutterOpen", "ForestTemple", {53.3f, 106.0f, -3.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room25_FirstTimeBLock", "FireTemple", {-46.9f, 153.9f, 0.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room25_FirstTimeBLock", "ForestTemple", {-16.2f, 101.4f, -14.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room27_CandleLight", "ForestTemple", {-10.4f, 61.0f, 6.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room27_IceMelts", "ForestTemple", {23.8f, 65.5f, -3.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"ForestTemple_Room27_ShutterOpen", "ForestTemple", {0.0f, 65.7f, -19.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_00_FallTreasureBox", "GerudoTrainingGround", {2.5f, 99.5f, -9.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_01_AppearTreasureBox_From00", "GerudoTrainingGround", {-51.0f, 98.1f, -13.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_02From03_SilverToken_Appear", "GerudoTrainingGround", {-63.8f, 105.9f, -29.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_02From03_SilverToken_Unlock", "GerudoTrainingGround", {-64.1f, 105.9f, -34.6f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"GerudoTrainingGround_02_SilverToken_Appear", "GerudoTrainingGround", {-66.1f, 111.9f, -44.3f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter.TokenVisible; OperateGameDataAllocAutogenGameData.IsWrite; unshared flag HaveEncountered_Token
    {"GerudoTrainingGround_02_SilverToken_Unlock", "GerudoTrainingGround", {-66.3f, 111.9f, -48.9f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish; OperateGameDataAllocAutogenGameData.IsWrite
    {"GerudoTrainingGround_03_AppaerTreasureBox", "GerudoTrainingGround", {-53.8f, 104.0f, -88.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_03_ShutterOpen", "GerudoTrainingGround", {-36.3f, 119.5f, -92.9f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_SwitchShutter_A_02.IsDisableBlend
    {"GerudoTrainingGround_04_ActStoneGimmick", "GerudoTrainingGround", {13.1f, 99.9f, -87.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_05_AppaerTreasureBox", "GerudoTrainingGround", {43.5f, 101.9f, -89.5f}, "EventBool+EventBoolEX1+EventFinish"},  // asker (EventRequestActor): TriggerGameSafetyStop.IsDrive
    {"GerudoTrainingGround_05_DisappearFire", "GerudoTrainingGround", {44.9f, 107.6f, -84.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_05_ShutterOpen", "GerudoTrainingGround", {34.4f, 108.5f, -89.8f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_SwitchShutter_A_02.IsDisableBlend; TriggerGameSafetyStop.IsDrive
    {"GerudoTrainingGround_06_DisappearFire", "GerudoTrainingGround", {32.8f, 107.6f, -35.6f}, "EventBool"},  // asker (EventRequestActor): Obj_GerudoTrainingGround_FireWall_A_01.StartBurnSign
    {"GerudoTrainingGround_06_SilverToken_Appear", "GerudoTrainingGround", {54.7f, 110.1f, -66.9f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Start; Obj_silverRupeeCounter.TokenVisible; unshared flag HaveEncountered_Token
    {"GerudoTrainingGround_06_SilverToken_Unlock", "GerudoTrainingGround", {59.5f, 110.1f, -71.6f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish; OperateGameDataAllocAutogenGameData.IsWrite
    {"GerudoTrainingGround_08_FenceFallDown", "GerudoTrainingGround", {0.7f, 101.1f, -45.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_09_AppaerTreasureBox", "GerudoTrainingGround", {71.6f, 101.9f, -42.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_09_SilverToken_Unlock", "GerudoTrainingGround", {76.4f, 101.9f, -47.2f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"GerudoTrainingGround_10_AppearTreasureBox", "GerudoTrainingGround", {-44.2f, 104.2f, -124.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_10_AppearTreasureBox", "GerudoTrainingGround", {-43.6f, 109.9f, -137.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_10_FallTreasureBox", "GerudoTrainingGround", {-51.4f, 103.5f, -131.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_Room04_StoneGimmick", "GerudoTrainingGround", {14.4f, 110.0f, -82.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"GerudoTrainingGround_Room07_TreasureAppear", "GerudoTrainingGround", {44.4f, 98.7f, -1.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"IceCavern_Gimmick_BlueFireShowup", "IceCavern", {19.1f, 115.2f, -69.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"IceCavern_Room01_DefeatEnemy", "IceCavern", {-5.8f, 109.5f, 26.2f}, ""},  // live (EventRequestActor): results through shared flags
    {"IceCavern_Room05_SilverToken_Appear", "IceCavern", {11.0f, 107.9f, -24.0f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter_03.Start; Obj_silverRupeeCounter_03.TokenVisible; unshared flag HaveEncountered_Token
    {"IceCavern_Room05_SilverToken_Unlock", "IceCavern", {14.9f, 107.9f, -24.0f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_IceCave_RotateBlade_A_01.IsResetAngle
    {"IceCavern_Room06_SilverToken_Appear", "IceCavern", {-49.1f, 113.0f, -8.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"IceCavern_Room06_SilverToken_Unlock", "IceCavern", {-49.2f, 107.3f, -7.7f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_silverRupeeCounter_05.Vanish
    {"InsideGanonCastle_DefeatEnemy", "InsideGanonCastle", {0.0f, 139.6f, -22.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_DefeatEnemy_02", "InsideGanonCastle", {-2.2f, 146.9f, -18.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_DefeatEnemy_03", "InsideGanonCastle", {2.3f, 149.7f, -24.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_AppaerTreasureBox", "InsideGanonCastle", {-39.2f, 103.9f, 15.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_AppaerTreasureBox_02", "InsideGanonCastle", {24.4f, 105.5f, 22.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_AppaerTreasureBox_08", "InsideGanonCastle", {47.2f, 103.9f, -103.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_AppaerTreasureBox_12", "InsideGanonCastle", {-38.2f, 99.8f, -21.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_AppearExtendingPlatform_03", "InsideGanonCastle", {39.7f, 102.8f, 41.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_BarrierRemove_Fire", "InsideGanonCastle", {-40.0f, 141.0f, -78.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_BarrierRemove_Forest", "InsideGanonCastle", {50.3f, 119.2f, 66.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_BarrierRemove_Light", "InsideGanonCastle", {-92.9f, 117.9f, -21.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_BarrierRemove_Shadow", "InsideGanonCastle", {30.8f, 129.5f, -75.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_BarrierRemove_Spirit", "InsideGanonCastle", {-37.0f, 122.6f, 45.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_BarrierRemove_Water", "InsideGanonCastle", {68.2f, 118.0f, -26.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_EnemyAppear01", "InsideGanonCastle", {-35.2f, 109.1f, 38.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_EnemyAppear02", "InsideGanonCastle", {-28.5f, 110.2f, 45.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_EnemyAppear03", "InsideGanonCastle", {-24.5f, 110.0f, 35.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_FocusOnGimmick_08_01", "InsideGanonCastle", {27.7f, 114.5f, -59.1f}, "EventBool"},  // asker (EventRequestActor): Obj_InsideGanonCastle_GlassTruth_A_05.IsOn
    {"InsideGanonCastle_Dungeon_ShutterOpen_02_01", "InsideGanonCastle", {26.2f, 111.2f, 25.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_ShutterOpen_06_01", "InsideGanonCastle", {84.6f, 107.9f, -27.2f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_ShutterOpen_08_01", "InsideGanonCastle", {46.3f, 109.1f, -126.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_ShutterOpen_17_01", "InsideGanonCastle", {-46.4f, 114.6f, 24.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_Dungeon_ShutterOpen_17_02", "InsideGanonCastle", {-29.4f, 114.1f, 25.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_SilverToken_Appear_Room03", "InsideGanonCastle", {35.9f, 109.9f, 38.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_SilverToken_Appear_Room10", "InsideGanonCastle", {-22.4f, 112.5f, -57.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_SilverToken_Appear_Room14", "InsideGanonCastle", {-66.2f, 108.1f, -19.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_SilverToken_Appear_Room16", "InsideGanonCastle", {-19.3f, 111.7f, 17.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideGanonCastle_SilverToken_Unlock_Room03", "InsideGanonCastle", {37.8f, 109.9f, 32.4f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"InsideGanonCastle_SilverToken_Unlock_Room10", "InsideGanonCastle", {-24.1f, 112.5f, -51.0f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"InsideGanonCastle_SilverToken_Unlock_Room14", "InsideGanonCastle", {-61.6f, 108.1f, -14.4f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"InsideGanonCastle_SilverToken_Unlock_Room16", "InsideGanonCastle", {-12.8f, 111.7f, 15.3f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"InsideJabuJabuBellyRoom01StepSwitch", "InsideJabuJabuBelly", {7.3f, 109.7f, -48.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBellyRoom05Swich", "InsideJabuJabuBelly", {26.8f, 106.4f, -57.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBellyRoom07StepSwitch", "InsideJabuJabuBelly", {15.0f, 100.7f, -138.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBellyRoom07StepSwitch02", "InsideJabuJabuBelly", {-12.5f, 98.1f, -136.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBellyRoom14StepSwitch", "InsideJabuJabuBelly", {32.0f, 78.0f, -69.6f}, "EventBool+EventBoolEX1+EventFinish"},  // asker (EventRequestActor): Obj_InsideJabu_Jabu_StepSwitch_A_04.BootUp; unshared flag Gimmick_Insidejabujabu_down_MoveWater
    {"InsideJabuJabuBellyRoom14StepSwitch_Under", "InsideJabuJabuBelly", {26.5f, 78.0f, -69.6f}, "EventBool+EventBoolEX1+EventFinish"},  // asker (EventRequestActor): Obj_InsideJabu_Jabu_StepSwitch_A_04.BootUp; unshared flag Gimmick_Insidejabujabu_Updown_MoveWater
    {"InsideJabuJabuBelly_FocusGimmick_Tentacle", "InsideJabuJabuBelly", {-25.4f, 94.1f, -181.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBelly_FocusOnGimmick", "InsideJabuJabuBelly", {-1.5f, 92.6f, -24.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBelly_Room12_TreasureBoxAppear", "InsideJabuJabuBelly", {23.8f, 92.2f, -174.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideJabuJabuBelly_TreasureBoxAppear", "InsideJabuJabuBelly", {-29.1f, 92.3f, -131.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideTheDekuTree_Gimmick_CandleAndSpiderWeb", "InsideTheDekuTree", {-10.0f, 82.5f, -9.5f}, "EventBool"},  // asker (EventRequestActor): Obj_InsideTheDekuTree_SpiderWeb_A_02.Burn; Obj_StepSwitch_A_05.BootUp; Obj_SwitchCandleStick_A_02.Burn; Obj_SwitchCandleStick_A_02.ForceControl; OperateGameDataAllocAutogenGameData.IsWrite
    {"InsideTheDekuTree_Gimmick_FallingLadder", "InsideTheDekuTree", {-37.5f, 117.8f, 12.5f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_InsideTheDekuTree_FallingLadder_A_01.IsOn
    {"InsideTheDekuTree_Gimmick_RoomD_PlatformUp", "InsideTheDekuTree", {-22.6f, 122.6f, -3.8f}, "EventBool"},  // asker (EventRequestActor): Obj_InsideTheDekuTree_ElevatorLift_A_01.IsOn; Obj_InsideTheDekuTree_ElevatorLift_A_02.IsOn; Obj_InsideTheDekuTree_ElevatorLift_A_03.IsOn
    {"InsideTheDekuTree_Gimmick_RoomF_EyeSwitch", "InsideTheDekuTree", {-14.5f, 82.7f, 27.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideTheDekuTree_Gimmick_RoomG_WaterLevelDrop", "InsideTheDekuTree", {-23.5f, 81.0f, 34.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"InsideTheDekuTree_Gimmick_RoomH_CandleStick", "InsideTheDekuTree", {-63.6f, 86.0f, 47.2f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_01_BeakStatue_Clear", "ShadowTemple", {51.7f, 107.3f, -0.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_03_EnemyAnnihilation_Clear", "ShadowTemple", {2.6f, 93.9f, -24.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_04_EnemyAnnihilation_Clear", "ShadowTemple", {-74.4f, 97.3f, -10.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_07_EnemyAnnihilation_Clear", "ShadowTemple", {96.5f, 90.4f, 20.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_10_EnemyAnnihilation_Clear", "ShadowTemple", {168.8f, 64.8f, 98.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_13_EnemyAnnihilation_DOWN_Clear", "ShadowTemple", {39.3f, 61.2f, 34.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_13_EnemyAnnihilation_UP_Clear", "ShadowTemple", {20.8f, 64.7f, 34.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_15_EnemyAnnihilation_Clear", "ShadowTemple", {122.8f, 56.6f, 6.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_16_EnemyAnnihilation_Clear", "ShadowTemple", {103.4f, 52.0f, -17.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_19_EnemyAnnihilation_Clear", "ShadowTemple", {-180.9f, 49.2f, -64.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_21_EnemyAnnihilation_Clear", "ShadowTemple", {-181.9f, 44.3f, -14.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_Gimmick_Focus_LargeJar_Key", "ShadowTemple", {36.3f, 56.2f, 33.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_Room06_SilverToken_Appear", "ShadowTemple", {91.1f, 95.1f, -25.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_Room06_SilverToken_Unlock", "ShadowTemple", {95.0f, 95.1f, -25.7f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter_04.Vanish
    {"ShadowTemple_Room09_SilverToken_Appear", "ShadowTemple", {48.8f, 64.4f, 80.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_Room09_SilverToken_Unlock", "ShadowTemple", {44.9f, 64.4f, 80.0f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter_04.Vanish
    {"ShadowTemple_Room12_SilverToken_Appear", "ShadowTemple", {48.5f, 60.5f, 33.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"ShadowTemple_Room12_SilverToken_Unlock", "ShadowTemple", {46.2f, 60.5f, 32.6f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter.Vanish
    {"SpiritTemple_FaceIsBroken_Effect", "SpiritTemple", {-3.2f, 141.6f, -65.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_03_BoardNetFall", "SpiritTemple", {-55.0f, 107.3f, -51.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_04_EyeSwitchOn", "SpiritTemple", {-31.3f, 107.9f, -81.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_05_SilverToken_Appear", "SpiritTemple", {-9.9f, 112.7f, -59.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_05_SilverToken_Unlock", "SpiritTemple", {-9.9f, 112.7f, -63.0f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter_06.Vanish
    {"SpiritTemple_Level_Room_07_SunSwitchOn", "SpiritTemple", {-32.2f, 123.8f, -48.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_08_SwitchOn_01", "SpiritTemple", {-17.1f, 131.7f, -44.1f}, "EventBool+EventFinish"},  // asker (EventRequestActor): Obj_SpiritTemple_SwitchShutter_A_01.LockMinusZ; Obj_SwitchShutter_A_06.IsDisableBlend; Obj_SwitchShutter_A_06.Open
    {"SpiritTemple_Level_Room_10_SilverToken_Appear", "SpiritTemple", {-45.5f, 141.3f, -13.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_10_SilverToken_Unlock", "SpiritTemple", {-45.5f, 141.3f, -17.1f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter_02.Vanish
    {"SpiritTemple_Level_Room_10_SwitchOn_01", "SpiritTemple", {-59.0f, 132.8f, -27.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_16_SilverToken_Appear", "SpiritTemple", {47.3f, 108.2f, -48.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_16_SilverToken_Unlock", "SpiritTemple", {47.3f, 108.2f, -52.3f}, "EventBool"},  // asker (EventRequestActor): Obj_silverRupeeCounter_02.Vanish
    {"SpiritTemple_Level_Room_19_SwitchOn_01", "SpiritTemple", {31.4f, 120.0f, -41.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_19_SwitchOn_01", "SpiritTemple", {40.9f, 119.2f, -47.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_19_wallmaster", "SpiritTemple", {41.9f, 125.7f, -41.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_30_BoardNetFall", "SpiritTemple", {19.8f, 159.5f, -77.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Level_Room_32_SwitchOn_01", "SpiritTemple", {-23.8f, 159.9f, -49.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpiritTemple_Room19_SunSwitchShutterOpen", "SpiritTemple", {27.1f, 120.7f, -47.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room03_OpenShutter", "SpiritTemple", {-33.6f, 104.8f, -32.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room04_OpenShutter", "SpiritTemple", {-31.7f, 108.1f, -77.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room05_AppearTreasure", "SpiritTemple", {-15.3f, 104.7f, -58.0f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room07_AppearTreasure", "SpiritTemple", {-39.2f, 119.2f, -41.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room08_AppearTreasureBox", "SpiritTemple", {0.0f, 114.2f, -55.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room08_OpenShutter", "SpiritTemple", {1.5f, 118.4f, -51.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room12_OpenShutter", "SpiritTemple", {-48.0f, 136.4f, 24.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room14_OpenShutter", "SpiritTemple", {33.4f, 104.8f, -34.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room22_OpenShutter", "SpiritTemple", {47.2f, 131.8f, -17.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room22_StepSwitch", "SpiritTemple", {46.5f, 134.0f, -25.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room23_StepSwitch", "SpiritTemple", {56.7f, 131.6f, -16.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room23_SunSwitch", "SpiritTemple", {55.9f, 133.0f, -33.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room25_OpenShutter", "SpiritTemple", {55.7f, 134.1f, 23.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room30_StepSwitch", "SpiritTemple", {23.1f, 150.9f, -71.4f}, ""},  // live (EventRequestActor): results through shared flags
    {"SpritTemple_Room31_OpenShutter", "SpiritTemple", {-3.0f, 158.1f, -46.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"StoneOfWater_ElevatedFloor", "InsideJabuJabuBelly", {-40.7f, 84.7f, -109.9f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTempleRoom08HitSwitch", "WaterTemple", {-39.0f, 96.0f, -21.6f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTempleRoom14StepSwitch", "WaterTemple", {1.5f, 100.0f, -109.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTempleRoom22Clear", "WaterTemple", {-83.5f, 135.5f, -69.6f}, "EventBool+EventFinish"},  // asker (EventRequestActor): TriggerChallengeSetStep.IsDrive
    {"WaterTempleRoom22Close", "WaterTemple", {-70.2f, 135.5f, -80.8f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTemple_EyeSwitch_Focus", "WaterTemple", {-33.5f, 118.9f, -40.7f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTemple_EyeSwitch_Focus", "WaterTemple", {0.7f, 116.1f, -29.3f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTemple_FocusGimmick_Room18", "WaterTemple", {41.5f, 100.0f, -4.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTemple_FocusonGimmick_02", "WaterTemple", {-6.7f, 89.6f, -4.5f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTemple_Room06_Waterlevelrise_FocusOnGimmick", "WaterTemple", {-71.8f, 121.2f, -15.1f}, ""},  // live (EventRequestActor): results through shared flags
    {"WaterTemple_Room17_FocusOnGimmick", "WaterTemple", {25.0f, 102.3f, -4.7f}, "EventBool"},  // asker (EventRequestActor): Obj_SwitchShutter_A_02.IsDisableBlend
    {"WaterTemple_Waterlevelrise_FocusOnGimmick", "WaterTemple", {-35.4f, 103.8f, 17.4f}, "EventBool+EventFinish"},  // asker (EventRequestActor): unshared flag WaterTemple_Room03_FocusOnGimmick
    {"DodongoCavern_OpenShutterPlusZ_FocusOnEnemy", "DodongoCavern", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"DodongoCavern_OpenShutterPlusZ_FocusOnEnemy_02", "DodongoCavern", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"FireTemple_OpenShutterPlusZ_FocusOnEnemy", "FireTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"FireTemple_OpenShutterPlusZ_FocusOnEnemy_02", "FireTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"FireTemple_OpenShutterPlusZ_FocusOnEnemy_02_B", "FireTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"ForestTemple_OpenDoor_Room01To02", "ForestTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"ForestTemple_OpenShutterPlusZ_FocusOnEnemy", "ForestTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"ForestTemple_OpenShutterPlusZ_FocusOnEnemy_02", "ForestTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"InsideGanonCastle_OpenShutterPlusZ_FocusOnEnemy", "InsideGanonCastle", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"InsideGanonCastle_OpenShutterPlusZ_FocusOnEnemy_02", "InsideGanonCastle", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"InsideGanonCastle_OpenShutterPlusZ_FocusOnEnemy_03", "InsideGanonCastle", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
    {"WaterTemple_DarkLink_DoorMovement", "WaterTemple", {0.0f, 0.0f, 0.0f}, ""},  // live (no asker actor: a field asks it)
};

constexpr Guard kGuards[] = {
    {"DodongoCavern_CandleStick", "DodongoCavern", "DodongoCavern_Gimmick_CandleStickShutter"},
    {"GerudoTrainingGround_Dungeon_Room05_DefeatEnemy", "GerudoTrainingGround", "GerudoTrainingGround_05_AppaerTreasureBox"},
    {"Gimmick_DefeatEnemy_Open_FireTemple_02_CounterOff", "FireTemple", "FireTemple_15_OpenShutter"},
    {"IceCavern_Room05_Unlocked", "IceCavern", "IceCavern_Room05_SilverToken_Unlock"},
    {"IceCavern_Room06_Unlocked", "IceCavern", "IceCavern_Room06_SilverToken_Unlock"},
    {"InsideJabuJabuBelly_Room14_PlayedSwitchEvent_Lower", "InsideJabuJabuBelly", "InsideJabuJabuBellyRoom14StepSwitch_Under"},
    {"InsideJabuJabuBelly_Room14_PlayedSwitchEvent_Upper", "InsideJabuJabuBelly", "InsideJabuJabuBellyRoom14StepSwitch"},
    {"InsideTheDekuTree_RoomC_Cleared", "InsideTheDekuTree", "InsideTheDekuTree_Gimmick_FallingLadder"},
    {"SpiritTemple_Room08_Unlocked_00", "SpiritTemple", "SpiritTemple_Level_Room_08_SwitchOn_01"},
    {"WaterTemple_Room22_Cleared", "WaterTemple", "WaterTempleRoom22Clear"},
};
// clang-format on

float Dist2(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

std::uint32_t PlayedHash(std::string_view event) {
    return coop::protocol::flag_hash("HavePlayedEvent." + std::string(event));
}

bool Has(const std::vector<std::uint32_t>& list, std::uint32_t hash) {
    return std::find(list.begin(), list.end(), hash) != list.end();
}
}  // namespace

std::span<const Result> Results() { return kResults; }

std::span<const Guard> Guards() { return kGuards; }

std::vector<const Result*> AskerLines(std::string_view event, std::string_view scene,
                                      const std::optional<std::array<float, 3>>& link) {
    std::vector<const Result*> out;
    for (const auto& r : kResults) {
        if (r.event == event && r.scene == scene && !r.outputs.empty()) out.push_back(&r);
    }
    if (link) {
        std::stable_sort(out.begin(), out.end(), [&](const Result* x, const Result* y) {
            return Dist2(x->at, *link) < Dist2(y->at, *link);
        });
    }
    return out;
}

bool IsLive(std::string_view event) {
    bool any = false;
    for (const auto& r : kResults) {
        if (r.event != event) continue;
        if (!r.outputs.empty()) return false;
        any = true;
    }
    return any;
}

std::vector<std::string_view> SplitOutputs(std::string_view outputs) {
    std::vector<std::string_view> out;
    while (!outputs.empty()) {
        const auto plus = outputs.find('+');
        out.push_back(outputs.substr(0, plus));
        if (plus == std::string_view::npos) break;
        outputs.remove_prefix(plus + 1);
    }
    return out;
}

bool Hold::Releases(std::uint32_t hash) const { return Has(released, hash); }

bool Hold::Waits(std::uint32_t hash) const { return Has(waiting, hash); }

Hold HoldFor(std::string_view scene, const std::set<std::string>& applied,
             const std::function<bool(const std::string&)>& played) {
    Hold h;
    for (const auto& event : applied) {
        h.released.push_back(PlayedHash(event));
        for (const auto& g : kGuards) {
            if (g.event == event) h.released.push_back(coop::protocol::flag_hash(g.flag));
        }
        for (const auto& g : CoopEyeSwitch::Guards()) {
            if (g.event == event) h.released.push_back(coop::protocol::flag_hash(g.flag));
        }
    }
    for (const auto& g : kGuards) {
        const std::string event(g.event);
        if (g.scene != scene || applied.contains(event) || (played && played(event))) continue;
        const auto hash = coop::protocol::flag_hash(g.flag);
        if (!h.Releases(hash)) h.waiting.push_back(hash);
    }
    return h;
}

bool Setter::Want(const std::string& event, const std::string& scene, std::optional<std::array<float, 3>> link) {
    if (AskerLines(event, scene).empty()) return false;
    std::lock_guard lock(mutex_);
    if (applied_.contains(event) || ended_.contains(event) ||
        std::any_of(wanted_.begin(), wanted_.end(), [&](const Try& t) { return t.event == event; })) {
        return true;  // set, given up or being set this visit
    }
    Try t;
    t.event = event;
    t.scene = scene;
    t.link = link;
    wanted_.push_back(std::move(t));
    return true;
}

void Setter::Visit(const std::string& scene, const std::function<bool(const std::string&)>& played) {
    std::lock_guard lock(mutex_);
    if (!scene.empty() && scene != scene_) {  // a new visit: the room loads again, nothing of ours is set in it
        scene_ = scene;
        applied_.clear();
        ended_.clear();
    }
    // The front may be mid-set on the calc thread: it ends there (Frame sees the scene or the played flag) so its
    // execute's after-call stays paired; only the ones behind it are dropped here.
    for (auto it = wanted_.size() > 1 ? wanted_.begin() + 1 : wanted_.end(); it != wanted_.end();) {
        if (it->scene != scene_ || (played && played(it->event))) {
            notes_.push_back("result: " + it->event + ": dropped (another scene, or our game played it)");
            it = wanted_.erase(it);
        } else {
            ++it;
        }
    }
    if (!wanted_.empty() && (wanted_.front().scene != scene_ || (played && played(wanted_.front().event)))) {
        wanted_.front().until_ms = 1;  // Frame ends it as timed out ("not set here")
    }
}

std::set<std::string> Setter::Applied() {
    std::lock_guard lock(mutex_);
    return applied_;
}

std::vector<std::string> Setter::TakeNotes() {
    std::lock_guard lock(mutex_);
    std::vector<std::string> out;
    out.swap(notes_);
    return out;
}

bool Setter::EventRunning(CoopPuppet::Guest& guest, std::uint64_t main) {
    namespace le = CoopLiveEvents::addr;
    std::uint64_t manager = 0;
    std::uint64_t session = 0;
    if (!guest.Read(main + le::kEventStatePtr, &manager, 8) || manager == 0) return false;
    return guest.Read(manager + le::kEventSession, &session, 8) && session != 0;
}

void Setter::End(const std::string& why, bool applied) {
    const Try& t = wanted_.front();
    notes_.push_back("result: " + t.event + ": " + why);
    if (t.scene == scene_) (applied ? applied_ : ended_).insert(t.event);
    wanted_.pop_front();
}

std::optional<CoopPuppet::Call> Setter::Step(CoopPuppet::Guest& guest, bool after_call, std::uint64_t result,
                                             std::uint64_t main, std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    if (after_call) {
        const Next was = next_;
        next_ = Next::none;
        if (was != Next::malloc) return std::nullopt;  // the execute: one set a frame
        if (result == 0) {
            failed_ = true;
            while (!wanted_.empty()) End("the game's malloc gave nothing: not set, its flags stay held", false);
            return std::nullopt;
        }
        block_ = result;
    }
    if (main == 0 || wanted_.empty()) return std::nullopt;
    if (failed_) {
        while (!wanted_.empty()) End("no set-bool block: not set, its flags stay held", false);
        return std::nullopt;
    }
    if (block_ == 0) {
        if (after_call) return std::nullopt;
        next_ = Next::malloc;
        return CoopLogicSet::Malloc(main);
    }
    return Frame(guest, main, now_ms);
}

std::optional<CoopPuppet::Call> Setter::Frame(CoopPuppet::Guest& guest, std::uint64_t main, std::uint64_t now_ms) {
    Try& t = wanted_.front();
    if (t.until_ms == 0) t.until_ms = now_ms + kTryMs;
    const auto lines = AskerLines(t.event, t.scene, t.link);
    if (now_ms > t.until_ms || lines.empty()) {
        char text[200];
        std::snprintf(text, sizeof text,
                      "not set here (%s; output %zu: %d set-bools, read 0 after a set on %d frames): its flags stay "
                      "held until we leave the dungeon or load",
                      t.asker == 0 ? "our asker not found" : "an output did not stay 1", t.output, t.sets,
                      t.zeros_after_set);
        End(text, false);
        return std::nullopt;
    }
    if (EventRunning(guest, main)) return std::nullopt;  // an event of ours runs: its flow owns the askers now
    // Our asker: the first line (nearest our Link) with an asker at its placement. Once found, only that one.
    const Result* line = nullptr;
    std::uint64_t asker = 0;
    for (const Result* l : lines) {
        const auto here = find_(guest, main, CoopLiveEvents::kAskerNames, l->at, CoopLiveEvents::kAskerNearM);
        if (here.empty()) continue;
        if (t.asker != 0 && std::find(here.begin(), here.end(), t.asker) == here.end()) continue;
        line = l;
        asker = t.asker != 0 ? t.asker : here.front();
        break;
    }
    if (line == nullptr) {
        if (t.asker != 0) {  // its graph deleted it after our set (EventFinish -> Logic_Delete): the result ran
            End("our asker is gone after our set (its room graph took the result)", true);
        }
        return std::nullopt;  // not loaded yet: wait (until kTryMs)
    }
    if (t.asker == 0) {
        t.asker = asker;
        char text[200];
        std::snprintf(text, sizeof text, "result: %s: our asker 0x%llx, setting %.*s (another player played it; no replay)",
                      t.event.c_str(), static_cast<unsigned long long>(asker), static_cast<int>(line->outputs.size()),
                      line->outputs.data());
        notes_.emplace_back(text);
    }
    const auto outputs = SplitOutputs(line->outputs);
    if (t.output >= outputs.size()) {
        End("set (" + std::string(line->outputs) + ")", true);
        return std::nullopt;
    }
    const std::uint32_t key = coop::protocol::flag_hash(outputs[t.output]);
    const auto plug = CoopLogicSet::BoolPlug(guest, asker, key);
    if (!plug) return std::nullopt;  // its logic not read yet: wait (until kTryMs)
    if (plug->value == std::uint8_t{1}) {
        if (++t.ones >= kApplyFrames) {
            notes_.push_back("result: " + t.event + ": our asker's " + std::string(outputs[t.output]) + " is 1");
            ++t.output;
            t.ones = 0;
            t.sets = 0;
            t.zeros_after_set = 0;
            if (t.output >= outputs.size()) End("set (" + std::string(line->outputs) + ")", true);
        }
        return std::nullopt;
    }
    t.ones = 0;
    if (t.sets > 0) ++t.zeros_after_set;
    if (!CoopLogicSet::WriteNode(guest, block_, main, asker, key)) return std::nullopt;
    ++t.sets;
    next_ = Next::execute;
    return CoopLogicSet::Execute(main, block_);
}

}  // namespace CoopEventResult
