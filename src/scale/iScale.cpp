#include "scale/iScale.h"
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/RegisterHelper.h>
#include <mc/deps/ecs/gamerefs_entity/EntityRegistry.h>
#include <mc/deps/ecs/strict/StrictEntityContext.h>
#include <mc/deps/vanilla_components/AABBShapeComponent.h>
#include <mc/deps/vanilla_components/ActorDataBoundingBoxComponent.h>
#include <mc/deps/vanilla_components/ActorDataFlagComponent.h>
#include <mc/deps/vanilla_components/PlayerIsSleepingFlagComponent.h>
#include <mc/entity/components/ActorOwnerComponent.h>
#include <mc/entity/components/IsHorizontalPoseFlagComponent.h>
#include <mc/entity/components/VanillaOffsetComponent.h>
#include <mc/entity/systems/VanillaOffsetSystem.h>
#include <mc/entity/systems/auto_step_system/AutoStepSystem.h>
#include <mc/util/VanillaOffsetSystemUtil.h>

class EntityModifierBase {
public:
    gsl::not_null<EntityRegistry*> mRegistry;
};
template <>
class EntityModifier<HasAutoSteppedComponent> : public EntityModifierBase {};

namespace mif::scale {
    
using namespace ll::memory_literals;

LL_TYPE_INSTANCE_HOOK(
    iScale::GetCameraOffsetHook,
    HookPriority::Normal,
    VanillaOffsetSystem,
    "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 ?? ?? ?? ?? 48 8B DA"_sig,
    void,
    StrictExecutionContext<
        Filter<PlayerComponent, ActorMovementTickNeededComponent>,
        Read<OffsetsComponent, ActorDataFlagComponent, PlayerIsSleepingFlagComponent>,
        Write<VanillaOffsetComponent>,
        AddRemove<IsHorizontalPoseFlagComponent>,
        GlobalRead<BaseGameVersionComponent>,
        GlobalWrite<>,
        EntityFactoryT<>>& context
) {
    origin(context);

    // clang-format off
    auto& registry = ll::memory::dAccess<gsl::not_null<EntityRegistry*>>(&context, 0)->mRegistry;
    for (auto entityId : registry->view<ActorDataFlagComponent, VanillaOffsetComponent, ActorDataBoundingBoxComponent, ActorOwnerComponent>()) {
        auto offset = static_cast<decltype(&VanillaOffsetSystemUtil::getCameraOffset)>(
            "48 89 5C 24 ?? 48 89 7C 24 ?? 49 8B 00"_sig.resolve()
        )(
            registry->get<ActorDataFlagComponent>(entityId),
            std::bit_cast<Optional<PlayerIsSleepingFlagComponent const>>(Optional<PlayerIsSleepingFlagComponent>{
                &registry->storage<PlayerIsSleepingFlagComponent>(entityId),
                entityId
            }),
            std::bit_cast<Optional<IsHorizontalPoseFlagComponent const>>(Optional<IsHorizontalPoseFlagComponent>{
                &registry->storage<IsHorizontalPoseFlagComponent>(entityId),
                entityId
            }),
            0.35f
        );

        // mCurrentOffset 为y轴负偏移量
        // 这里的 offset * registry->get<ActorDataBoundingBoxComponent>(entityId).mValue[0] 是算出缩放后的偏移量
        // 然后减去原本就会偏移的y轴坐标 得到实际偏移量
        // 为什么是 = 不是 += 或 -= 呢？问mj去，史山代码
        registry->get<VanillaOffsetComponent>(entityId).mCurrentOffset->y = -(
            offset * registry->get<ActorDataBoundingBoxComponent>(entityId).mValue[0] - offset
        );
    }
    // clang-format on
}

LL_AUTO_STATIC_HOOK(
    TestHook,
    HookPriority::Normal,
    // &AutoStepSystem::doAutoStepSystemImpl,
    "48 8B C4 48 89 58 ?? 48 89 70 ?? 55 57 41 54 41 56 41 57 48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 49 8B F9"_sig,
    void,
    void*                                   autoStepRequest,
    StrictEntityContext const&              entity,
    MaxAutoStepComponent const&             maxAutoStepComponent,
    MoveRequestComponent&                   moveRequestComponent,
    AABBShapeComponent&                     aabbShapeComponent,
    SubBBsComponent&                        subBBsComponent,
    EntityModifier<HasAutoSteppedComponent> mod
) {
    origin(
        autoStepRequest,
        entity,
        maxAutoStepComponent,
        moveRequestComponent,
        aabbShapeComponent,
        subBBsComponent,
        mod
    );

    auto& registry       = mod.mRegistry->mRegistry;
    auto* scaleComponent = registry->try_get<ActorDataBoundingBoxComponent>(entity.mEntity);
    if (!scaleComponent) return;

    auto& aabb     = *aabbShapeComponent.mAABB;
    auto  center   = (aabb.min + aabb.max) * 0.5f;
    auto  halfSize = (aabb.max - aabb.min) * 0.5f * scaleComponent->mValue[0];
    // clang-format off
    aabb = AABB{
        { center.x - halfSize.x, aabb.min.y, center.z - halfSize.z },
        { center.x + halfSize.x, aabb.min.y + (aabb.max.y - aabb.min.y) * scaleComponent->mValue[0], center.z + halfSize.z }
    };
    // clang-format on
    if (aabb.min.y > aabb.max.y) std::swap(aabb.min.y, aabb.max.y);
}

iScale& iScale::getInstance() {
    static iScale instance;
    return instance;
}

bool iScale::load() { return true; }

bool iScale::enable() {
    ll::memory::HookRegistrar<GetCameraOffsetHook>::hook();
    return true;
}

bool iScale::disable() { return true; }

bool iScale::unload() {
    ll::memory::HookRegistrar<GetCameraOffsetHook>::unhook();
    return true;
}

LL_REGISTER_MOD(iScale, iScale::getInstance());

} // namespace mif::scale