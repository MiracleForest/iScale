#include "scale/iScale.h"
#include <cmath>
#include <ll/api/Versions.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/runtime/RuntimeOverload.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/ecs/gamerefs_entity/EntityRegistry.h>
#include <mc/deps/nbt/CompoundTagVariant.h>
#include <mc/deps/vanilla_components/AABBShapeComponent.h>
#include <mc/deps/vanilla_components/ActorDataBoundingBoxComponent.h>
#include <mc/deps/vanilla_components/ActorDataDirtyFlagsComponent.h>
#include <mc/deps/vanilla_components/ActorDataFlagComponent.h>
#include <mc/deps/vanilla_components/OffsetsComponent.h>
#include <mc/deps/vanilla_components/PlayerIsSleepingFlagComponent.h>
#include <mc/deps/vanilla_components/StateVectorComponent.h>
#include <mc/entity/components/ActorRotationComponent.h>
#include <mc/entity/components/ApplyGravityComponent.h>
#include <mc/entity/components/FallDistanceComponent.h>
#include <mc/entity/components/IsHorizontalPoseFlagComponent.h>
#include <mc/entity/components/MobEffectsComponent.h>
#include <mc/entity/components/MobJumpComponent.h>
#include <mc/entity/components/PlayerInputRequestComponent.h>
#include <mc/entity/components/ShouldUpdateBoundingBoxRequestComponent.h>
#include <mc/entity/components/TriggerJumpRequestComponent.h>
#include <mc/entity/components/VanillaOffsetComponent.h>
#include <mc/entity/components_json_legacy/JumpControlComponent.h>
#include <mc/entity/systems/PlayerBoundingBoxStateUpdateSystem.h>
#include <mc/entity/systems/VanillaOffsetSystem.h>
#include <mc/entity/systems/mob_jump_from_ground_system_impl/MobJumpFromGroundSystemImpl.h>
#include <mc/entity/systems/movement/mob/MobMovementGravity.h>
#include <mc/entity/utilities/GetAttachPositionUtility.h>
#include <mc/entity/utilities/JumpPreventionResult.h>
#include <mc/entity/utilities/JumpPreventionUtility.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/util/VanillaOffsetSystemUtil.h>
#include <mc/world/Minecraft.h>
#include <mc/world/actor/ai/control/JumpControl.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/effect/MobEffect.h>
#include <mc/world/effect/MobEffectInstance.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/material/Material.h>
#include <mc/world/phys/AABB.h>
// #include <mc/deps/ecs/strict/StrictEntityContext.h>

ShouldUpdateBoundingBoxRequestComponent::UpdateFromValue::UpdateFromValue()                       = default;
ShouldUpdateBoundingBoxRequestComponent::UpdateFromValue::UpdateFromValue(UpdateFromValue const&) = default;
ShouldUpdateBoundingBoxRequestComponent::UpdateFromValue&
ShouldUpdateBoundingBoxRequestComponent::UpdateFromValue::operator=(UpdateFromValue const&) = default;
class StrictEntityContext {
public:
    EntityId mEntity;
    uint     mRegistryId;
};

namespace mif::scale {

using namespace ll::memory_literals;

// 修复摄像机偏移
LL_TYPE_INSTANCE_HOOK(
    iScale::TickCameraOffsetHook,
    HookPriority::Normal,
    VanillaOffsetSystem,
    &VanillaOffsetSystem::$tick,
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
    for (auto entityId : registry->view<ActorDataFlagComponent, VanillaOffsetComponent, OffsetsComponent, ActorDataBoundingBoxComponent>()) {
        auto& vanillaOffsetComponent = registry->get<VanillaOffsetComponent>(entityId);
        auto& offsetsComponent       = registry->get<OffsetsComponent>(entityId);
        auto  scale                  = getScale(registry, entityId);
        auto cameraOffset = [](
            ActorDataFlagComponent const& actorDataFlag,
            Optional<PlayerIsSleepingFlagComponent const> const& isSleepingFlag,
            Optional<IsHorizontalPoseFlagComponent const> const& isHorizontalFlag,
            float sneakHeight
        ) {
            if (isHorizontalFlag.mEnTTStorage) return 0.4f;
            if (actorDataFlag.getStatusFlag(ActorFlags::Sneaking)) return 1.62001f - sneakHeight;
            if (isSleepingFlag.mEnTTStorage) return 0.2f;
            return 1.62001f;
        }(
            registry->get<ActorDataFlagComponent>(entityId),
            std::bit_cast<Optional<PlayerIsSleepingFlagComponent const>>(
                Optional<PlayerIsSleepingFlagComponent>{
                    &registry->storage<PlayerIsSleepingFlagComponent>(),
                    entityId
                }
            ),
            std::bit_cast<Optional<IsHorizontalPoseFlagComponent const>>(
                Optional<IsHorizontalPoseFlagComponent>{
                    &registry->storage<IsHorizontalPoseFlagComponent>(),
                    entityId
                }
            ),
            0.35f
        );

        if (!mIsClientSide) {
            vanillaOffsetComponent.mCurrentOffset->y = (offsetsComponent.mHeightOffset - cameraOffset) * scale + cameraOffset * (1 - scale);
        }
#if defined (LL_PLAT_C)
        else {
            auto prevY           = vanillaOffsetComponent.mPreviousOffset->y;
            auto originalNewY    = vanillaOffsetComponent.mCurrentOffset->y;
            auto heightOffset    = offsetsComponent.mHeightOffset;
            auto climbingOffsetY = vanillaOffsetComponent.mClimbingOffset->y;

            auto originalCameraOffset = prevY + heightOffset - climbingOffsetY - 2.0f * originalNewY; // 逆推原本的y
            auto scaledCameraOffset   = originalCameraOffset * scale;                                 // 缩放原本的y
            auto scaledNewY = (prevY + heightOffset - scaledCameraOffset - climbingOffsetY) / 2.0f;   // 计算缩放后的y

            vanillaOffsetComponent.mCurrentOffset->y = scaledNewY;
        }
#endif
    }
    // clang-format on
}

// 数据持久化-加载缩放
LL_TYPE_INSTANCE_HOOK(
    iScale::ActorLoadHook,
    HookPriority::Normal,
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

// 数据持久化-保存缩放
LL_TYPE_INSTANCE_HOOK(iScale::ActorSaveHook, HookPriority::Normal, Actor, &Actor::save, bool, CompoundTag& entityTag) {
    if (auto result = origin(entityTag); result) {
        entityTag["Scale"] = getScale(*this);
        return true;
    }
    return false;
}

// 数据持久化-初始化时再重新加载一次缩放
LL_TYPE_INSTANCE_HOOK(
    iScale::ActorInitComponentHook,
    HookPriority::Normal,
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

// 修复自动爬行/潜行
LL_TYPE_STATIC_HOOK(
    iScale::TickBoundingBoxHook,
    HookPriority::Low,
    PlayerBoundingBoxStateUpdateSystem,
    &PlayerBoundingBoxStateUpdateSystem::tickPlayerBoundingBoxStateUpdateSystem,
    void,
    StrictEntityContext const&   context,
    AABBShapeComponent const&    aabb,
    PlayerInputRequestComponent& request,
    StrictExecutionContext<
        Filter<
            CanStandOnSnowFlagComponent,
            HasLightweightFamilyFlagComponent,
            HorseFlagComponent,
            MobFlagComponent,
            ParrotFlagComponent,
            VehicleComponent,
            CamelFlagComponent,
            PlayerComponent>,
        Read<
            AABBShapeComponent,
            MovementAbilitiesComponent,
            ActorTypeComponent,
            FallDistanceComponent,
            PassengerComponent,
            ActorGameTypeComponent,
            ActorDataFlagComponent,
            VehicleComponent,
            ActorRotationComponent,
            MobBodyRotationComponent,
            RenderRotationComponent,
            StandAnimationComponent,
            OffsetsComponent,
            VanillaOffsetComponent,
            PassengerRenderingRidingOffsetComponent,
            DimensionTypeComponent>,
        Write<PlayerInputRequestComponent>,
        AddRemove<>,
        GlobalRead<ExternalDataComponent, LocalConstBlockSourceFactoryComponent>,
        GlobalWrite<>,
        EntityFactoryT<>>&        executionContext,
    float                         sneakHeight,
    GetAttachPositionViews const& views,
    IConstBlockSource const&      region
) {
    using namespace ll::memory_literals;
    auto& registry = ll::memory::dAccess<gsl::not_null<EntityRegistry*>>(&executionContext, 0)->mRegistry;
    if (!registry->any_of<ActorDataBoundingBoxComponent>(context.mEntity)) {
        return origin(context, aabb, request, executionContext, sneakHeight, views, region);
    }
    auto scale = getScale(registry, context.mEntity);

    static auto doesIntersect = [](IConstBlockSource const&          blockSource,
                                   AABB const&                       testAABB,
                                   GetCollisionShapeInterface const* entity,
                                   std::vector<AABB>&                storage,
                                   bool                              withUnloadedChunks) {
        blockSource.fetchCollisionShapes(storage, testAABB, withUnloadedChunks, entity, nullptr);

        return std::ranges::any_of(storage, [&](auto& shape) { return shape.intersects(testAABB); });
    };

    // clang-format off
    request.mBreathingPoint = GetAttachPositionUtility::getPassengerAttachPos(
        context,
        views,
        SharedTypes::Legacy::ActorLocation::BreathingPoint,
        0.0f
    );
    // clang-format on

    {
        auto& material             = region.getMaterial(*request.mBreathingPoint);
        request.mBreathingInAir    = material.mType == SharedTypes::v1_26_20::MaterialType::Air;
        request.mBreathingInLiquid = material.mLiquid;
    }

    struct CollisionProxy {
    public:
        void*               mVTable{reinterpret_cast<void* (*)()>("?$vftable@?$GetCollisionShapeEntityProxy@V?$Strict"
                                                                  "ExecutionContext@U?$Filter@UCanStandOnSnowFlagComp"
                                                                  "onent@@UHasLightweightFamilyFlagComponent@@UHorseF"
                                                                  "lagComponent@@UMobFlagComponent@@UParrotFlagCompon"
                                                                  "ent@@UVehicleComponent@@UCamelFlagComponent@@UPlay"
                                                                  "erComponent@@@@U?$Read@UAABBShapeComponent@@UMovem"
                                                                  "entAbilitiesComponent@@UActorTypeComponent@@UFallD"
                                                                  "istanceComponent@@UPassengerComponent@@UActorGameT"
                                                                  "ypeComponent@@UActorDataFlagComponent@@UVehicleCom"
                                                                  "ponent@@UActorRotationComponent@@UMobBodyRotationC"
                                                                  "omponent@@URenderRotationComponent@@UStandAnimatio"
                                                                  "nComponent@@UOffsetsComponent@@UVanillaOffsetCompo"
                                                                  "nent@@UPassengerRenderingRidingOffsetComponent@@UD"
                                                                  "imensionTypeComponent@@@@U?$Write@UPlayerInputRequ"
                                                                  "estComponent@@@@U?$AddRemove@$$V@@U?$GlobalRead@UE"
                                                                  "xternalDataComponent@@ULocalConstBlockSourceFactor"
                                                                  "yComponent@@@@U?$GlobalWrite@$$V@@U?$EntityFactory"
                                                                  "T@$$V@@@@@@SAPEAPEAXXZ"_sym.resolve())()};
        StrictEntityContext mEntity;
        decltype(&executionContext) mContext;

    public:
        CollisionProxy(StrictEntityContext context, decltype(&executionContext) executionContext)
        : mEntity(context),
          mContext(executionContext) {}

    public:
        GetCollisionShapeInterface* getCollisionShape() { return reinterpret_cast<GetCollisionShapeInterface*>(this); }
    } collisionProxy{context, &executionContext};

    std::vector<AABB> blockStorage;

    {
        auto standingAABB   = aabb.mAABB;
        standingAABB->max.y = standingAABB->min.y + 1.8f * scale;
        if (doesIntersect(
                region,
                standingAABB->shrink(Vec3{0.01f}),
                collisionProxy.getCollisionShape(),
                blockStorage,
                false
            )) {
            request.mUnblockedToStand = false;
        }
    }
    {
        auto sneakingAABB   = aabb.mAABB;
        sneakingAABB->max.y = sneakingAABB->min.y + sneakHeight * scale;
        if (doesIntersect(
                region,
                sneakingAABB->shrink(Vec3{0.01f}),
                collisionProxy.getCollisionShape(),
                blockStorage,
                false
            )) {
            request.mUnblockedToSneak = false;
        }
    }
    {
        auto crawlingAABB   = aabb.mAABB;
        crawlingAABB->max.y = crawlingAABB->min.y + 0.6f * scale;
        if (doesIntersect(
                region,
                crawlingAABB->shrink(Vec3{0.01f}),
                collisionProxy.getCollisionShape(),
                blockStorage,
                false
            )) {
            request.mUnblockedToCrawl = false;
        }
    }
}

// 跳跃高度修改
LL_STATIC_HOOK(
    iScale::MobJumpHook,
    HookPriority::Normal,
    &MobJumpFromGroundSystemImpl::mobJumpFromGround,
    void,
    StrictEntityContext const&                   entity,
    AABBShapeComponent const&                    aabbShapeComponent,
    ActorDataFlagComponent const&                actorDataFlagComponent,
    ActorRotationComponent const&                actorRotationComponent,
    MobEffectsComponent const&                   mobEffectsComponent,
    JumpControlComponent const*                  jumpControlComponent,
    MobJumpComponent&                            mobJumpComponent,
    StateVectorComponent&                        stateVectorComponent,
    EntityModifier<TriggerJumpRequestComponent>& modifier,
    IConstBlockSource const&                     region,
    bool                                         isSquid
) {
    // 记录起跳起始位置
    mobJumpComponent.mJumpStartPos = stateVectorComponent.mPos;

    // clang-format off
    auto preventionResult = JumpPreventionUtility::getJumpPrevention(
        true,
        *aabbShapeComponent.mAABB,
        *stateVectorComponent.mPos,
        region
    );
    // clang-format on

    auto jumpPower = [&]() {
        if (isSquid) return 0.2f;
        if (jumpControlComponent && jumpControlComponent->mJumpControl) {
            return jumpControlComponent->mJumpControl->getJumpPower(*jumpControlComponent);
        }
        return 0.42f;
    }();
    auto effectAmplifier = [&]() {
        auto* mobEFfect = MobEffect::JUMP();
        if (mobEffectsComponent.mMobEffects->size() > mobEFfect->mId) {
            auto& mobEffectInstance = (*mobEffectsComponent.mMobEffects)[mobEFfect->mId];
            if (mobEffectInstance.mId != MobEffectInstance::NO_EFFECT().mId) {
                return (static_cast<float>(mobEffectInstance.mAmplifier) + 1.0f) * 0.1f;
            }
        }
        return 0.0f;
    }();
    auto& registry = *ll::memory::dAccess<gsl::not_null<EntityRegistry*>>(&modifier, 0)->mRegistry;

    auto newVerticalSpeed = (preventionResult.mJumpIsPrevented ? 0.6f : 1.0f) * (effectAmplifier + jumpPower)
                          * getScale(registry, entity.mEntity);

    if (newVerticalSpeed > stateVectorComponent.mPosDelta->y) {
        stateVectorComponent.mPosDelta->y = newVerticalSpeed;
    }

    if (actorDataFlagComponent.mValue.test(std::to_underlying(ActorFlags::Sprinting))) {
        auto yawRad = actorRotationComponent.mRot->y * 0.017453292f;

        stateVectorComponent.mPosDelta->x -= std::sin(yawRad) * 0.2f;
        stateVectorComponent.mPosDelta->z += std::cos(yawRad) * 0.2f;
    }

    (void)registry.get_or_emplace<TriggerJumpRequestComponent>(entity.mEntity, preventionResult);
}

#ifdef LL_PLAT_S
// 重力
LL_STATIC_HOOK(
    iScale::TickAirGravityHook,
    HookPriority::Normal,
    &MobMovementGravity::tickDefaultGravity,
    void,
    StrictEntityContext const&             context,
    ActorDataFlagComponent const&          synchedActorData,
    MobEffectsComponent const&             mobEffects,
    StateVectorComponent const&            stateVector,
    EntityModifier<ApplyGravityComponent>& modifier
) {
    auto& registry             = *ll::memory::dAccess<gsl::not_null<EntityRegistry*>>(&modifier, 0)->mRegistry;
    auto  scale                = getScale(registry, context.mEntity);
    auto  hasGlidingOrSwimming = synchedActorData.mValue.test(std::to_underlying(ActorFlags::InScaffolding))
                              || synchedActorData.mValue.test(std::to_underlying(ActorFlags::OverScaffolding));
    auto  hasGravity           = synchedActorData.mValue.test(std::to_underlying(ActorFlags::HasGravity));
    auto  hasExtraFlag         = synchedActorData.mValue.test(std::to_underlying(ActorFlags::DescendThroughBlock));
    auto& component            = registry.get_or_emplace<ApplyGravityComponent>(context.mEntity);

    if (hasGravity && (!hasGlidingOrSwimming || !hasExtraFlag)) {
        if (stateVector.mPosDelta->y < 0.0f && [&]() {
            auto* mobEFfect = MobEffect::SLOW_FALLING();
            if (mobEffects.mMobEffects->size() > mobEFfect->mId) {
                auto& mobEffectInstance = (*mobEffects.mMobEffects)[mobEFfect->mId];
                if (mobEffectInstance.mId != MobEffectInstance::NO_EFFECT().mId) {
                    return true;
                }
            }
            return false;
        }()) {
            component.mResetFallDistance = true;
            component.mGravity           = 0.01f * scale;
        } else {
            component.mGravity = 0.08f * scale;
        }
    }
}
#endif

iScale& iScale::getInstance() {
    static iScale instance;
    return instance;
}

bool iScale::load() {
    mListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerStartedEvent>([](auto&) {
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
    Hooks::hook();
    return true;
}

bool iScale::enable() { return true; }

bool iScale::disable() { return true; }

bool iScale::unload() {
    ll::event::EventBus::getInstance().removeListener(mListener);
    Hooks::unhook();
    return true;
}

void iScale::setScale(Actor& entity, float scale) {
    auto& entityContext = entity.getEntityContext();

    entityContext.getOrAddComponent<ActorDataBoundingBoxComponent>()
        .mValue[std::to_underlying(ActorDataBoundingBoxComponent::Type::Scale)] = scale;
    entityContext.getOrAddComponent<ActorDataDirtyFlagsComponent>().mDirtyFlags->set(
        std::to_underlying(ActorFlags::ShowBottom),
        true
    );
    entityContext.getOrAddComponent<ShouldUpdateBoundingBoxRequestComponent>()
        .mUpdate->emplace<ShouldUpdateBoundingBoxRequestComponent::UpdateFromDefinition>();
}

float iScale::getScale(Actor& entity) {
    return entity.mEntityContext->tryGetComponent<ActorDataBoundingBoxComponent>()
        .transform([](auto&& component) {
        return component.mValue[std::to_underlying(ActorDataBoundingBoxComponent::Type::Scale)];
    }).value_or(1.0f);
}

float iScale::getScale(entt::basic_registry<EntityId>& registry, EntityId const& entityId) {
    if (auto* component = registry.try_get<ActorDataBoundingBoxComponent>(entityId); component) {
        return component->mValue[std::to_underlying(ActorDataBoundingBoxComponent::Type::Scale)];
    }
    return 1.0f;
}

LL_REGISTER_MOD(iScale, iScale::getInstance());

} // namespace mif::scale
