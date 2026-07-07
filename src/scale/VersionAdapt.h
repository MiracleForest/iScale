#pragma once

// clang-format off

#if defined(MC_VERSION_1_21_60_10) || defined(MC_VERSION_1_26_10_4)
    #define TICK_CAMERA_OFFSET_TARGET      &VanillaOffsetSystem::$tick
    #define TICK_BOUNDING_BOX_TARGET       &PlayerBoundingBoxStateUpdateSystem::tickPlayerBoundingBoxStateUpdateSystem
    #define MOB_JUMP_HOOK_TARGET           &MobJumpFromGroundSystemImpl::mobJumpFromGround
    #define TICK_AIR_GRAVITY_HOOK_TARGET   &MobMovementGravity::tickAirGravity

    #define GET_CAMERA_OFFSET_PTR          VanillaOffsetSystemUtil::getCameraOffset
    #define GET_PASSENGER_ATTACH_POS_PTR   GetAttachPositionUtility::getPassengerAttachPos
    #define GET_JUMP_PREVENTION_PTR        JumpPreventionUtility::getJumpPrevention

    #define COLLISION_PROXY_VTABLE_OFFSET  0x45D2848

#elif defined(MC_VERSION_1_21_93_1)
    #define TICK_CAMERA_OFFSET_TARGET      "4C 8B DC 49 89 5B ?? 49 89 73 ?? 57 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 ?? ?? ?? ?? 48 8B 02"_sig
    #define TICK_BOUNDING_BOX_TARGET       "48 8B C4 48 89 58 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 4D 8B F8"_sig
    #define MOB_JUMP_HOOK_TARGET           "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 55 41 54 41 55 41 56 41 57 48 8B EC 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 48 8B 7"_sig
    #define TICK_AIR_GRAVITY_HOOK_TARGET   "48 8B 42 ?? 4D 8B D8"_sig

    #define GET_CAMERA_OFFSET_PTR          reinterpret_cast<decltype(&VanillaOffsetSystemUtil::getCameraOffset)>("48 89 5C 24 ?? 41 8B 40"_sig.resolve())
    #define GET_PASSENGER_ATTACH_POS_PTR   reinterpret_cast<decltype(&GetAttachPositionUtility::getPassengerAttachPos)>("48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 83 EC ?? F3 0F 10 84 24"_sig.resolve())
    #define GET_JUMP_PREVENTION_PTR        reinterpret_cast<decltype(&JumpPreventionUtility::getJumpPrevention)>("48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 ?? 48 8B 74 24"_sig.resolve())

    #define COLLISION_PROXY_VTABLE_OFFSET  0x50DAD28

#elif defined(MC_VERSION_1_21_132_3)
    #define TICK_CAMERA_OFFSET_TARGET      "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 ?? ?? ?? ?? 48 8B DA"_sig
    #define TICK_BOUNDING_BOX_TARGET       "48 8B C4 48 89 58 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 4D 8B F0 4C 8B E2"_sig
    #define MOB_JUMP_HOOK_TARGET           "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 48 8B 7D"_sig
    #define TICK_AIR_GRAVITY_HOOK_TARGET   "48 8B 4A ?? F6 C1"_sig

    #define GET_CAMERA_OFFSET_PTR          reinterpret_cast<decltype(&VanillaOffsetSystemUtil::getCameraOffset)>("48 89 5C 24 ?? 48 89 7C 24 ?? 49 8B 00"_sig.resolve())
    #define GET_PASSENGER_ATTACH_POS_PTR   reinterpret_cast<decltype(&GetAttachPositionUtility::getPassengerAttachPos)>("48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 83 EC ?? F3 0F 10 84 24"_sig.resolve())
    #define GET_JUMP_PREVENTION_PTR        reinterpret_cast<decltype(&JumpPreventionUtility::getJumpPrevention)>("48 89 5C 24 ?? 48 89 6C 24 ?? 56 57 41 56 48 83 EC ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 ?? 48 8B B4 24 ?? ?? ?? ?? 49 8B F9"_sig.resolve())

    #define COLLISION_PROXY_VTABLE_OFFSET  (ll::isClient() ? 0x845EA88 : 0x5D74FA8)

#else
    #error "Unsupported Minecraft version"
#endif

// clang-format on