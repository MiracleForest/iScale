#include "scale/iScale.h"
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/runtime/RuntimeOverload.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/RegisterHelper.h>
#include <mc/deps/ecs/gamerefs_entity/EntityRegistry.h>
#include <mc/deps/ecs/strict/StrictEntityContext.h>
#include <mc/deps/vanilla_components/AABBShapeComponent.h>
#include <mc/deps/vanilla_components/ActorDataBoundingBoxComponent.h>
#include <mc/deps/vanilla_components/ActorDataDirtyFlagsComponent.h>
#include <mc/deps/vanilla_components/ActorDataFlagComponent.h>
#include <mc/deps/vanilla_components/PlayerIsSleepingFlagComponent.h>
#include <mc/entity/components/ActorOwnerComponent.h>
#include <mc/entity/components/IsHorizontalPoseFlagComponent.h>
#include <mc/entity/components/VanillaOffsetComponent.h>
#include <mc/entity/systems/VanillaOffsetSystem.h>
#include <mc/entity/systems/auto_step_system/AutoStepSystem.h>
#include <mc/nbt/CompoundTagVariant.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/util/VanillaOffsetSystemUtil.h>
#include <mc/world/actor/player/Player.h>

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

LL_STATIC_HOOK(
    iScale::AutoStepHook,
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

LL_TYPE_INSTANCE_HOOK(
    iScale::ActorLoadHook,
    HookPriority::Low,
    Actor,
    &Actor::$load,
    bool,
    CompoundTag const& tag,
    DataLoadHelper&    dataLoadHelper
) {
    if (auto result = origin(tag, dataLoadHelper); result) {
        if (tag.contains("Scale", Tag::Type::Float)) {
            auto scale = static_cast<float>(tag["Scale"]);
            setScale(*this, scale);
            getInstance().mScales.emplace(this, scale);
        }
        return true;
    }
    return false;
}

LL_TYPE_INSTANCE_HOOK(iScale::ActorSaveHook, HookPriority::Low, Actor, &Actor::save, bool, CompoundTag& entityTag) {
    if (auto result = origin(entityTag); result) {
        entityTag["Scale"] = getScale(*this);
        return true;
    }
    return false;
}

LL_TYPE_INSTANCE_HOOK(
    iScale::ActorInitComponentHook,
    HookPriority::Low,
    Actor,
    &Actor::$initializeComponents,
    void,
    ActorInitializationMethod   method,
    VariantParameterList const& params
) {
    origin(method, params);
    auto& scales = getInstance().mScales;
    if (auto it = scales.find(this); it != scales.end()) {
        setScale(*this, it->second);
        scales.erase(it);
    }
}

iScale& iScale::getInstance() {
    static iScale instance;
    return instance;
}

bool iScale::load() {
    ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerStartedEvent>([](auto&) {
        ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("scale", "缩放玩家大小", CommandPermissionLevel::GameDirectors)
            .runtimeOverload()
            .required("target", ll::command::ParamKind::Player)
            .optional("scale", ll::command::ParamKind::Float)
            .execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::RuntimeCommand const& self) {
            auto targets = self["target"].get<ll::command::ParamKind::Player>().results(origin);
            if (targets.empty()) return output.error("没有指定玩家");

            if (self["scale"].has_value()) { // set
                std::vector<std::string> names;
                auto                     scale = self["scale"].get<ll::command::ParamKind::Float>();
                for (auto* target : targets) {
                    setScale(*target, scale);
                    names.push_back(target->getRealName());
                }
                output.success("已将 {0} 的缩放设置为 {1}", fmt::join(names, ", "), scale);
            } else { // get
                ll::SmallDenseMap<float, std::vector<std::string>> scaleToNames;
                for (auto* target : targets) {
                    scaleToNames[getScale(*target)].push_back(target->getRealName());
                }
                for (auto& [scale, names] : scaleToNames) {
                    output.success("{0} 的缩放为 {1}", fmt::join(names, ", "), scale);
                }
            }
        });
    });
    return true;
}

bool iScale::enable() {
    Hooks::hook();
    return true;
}

bool iScale::disable() { return true; }

bool iScale::unload() {
    Hooks::unhook();
    return true;
}

void iScale::setScale(Actor& entity, float scale) {
    entity.mEntityContext->getOrAddComponent<ActorDataBoundingBoxComponent>().mValue[0] = scale;
    entity.mEntityContext->getOrAddComponent<ActorDataDirtyFlagsComponent>().mDirtyFlags->set(
        std::to_underlying(ActorFlags::ShowBottom),
        true
    );
}

float iScale::getScale(Actor& entity) {
    return entity.mEntityContext->tryGetComponent<ActorDataBoundingBoxComponent>()
        .transform([](auto&& component) {
        return component.mValue[0];
    }).value_or(1.0f);
}

LL_REGISTER_MOD(iScale, iScale::getInstance());

} // namespace mif::scale