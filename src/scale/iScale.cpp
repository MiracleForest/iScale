#include "scale/iScale.h"
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/runtime/RuntimeOverload.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/mod/RegisterHelper.h>
#include <mc/deps/ecs/gamerefs_entity/EntityRegistry.h>
#include <mc/deps/ecs/strict/StrictEntityContext.h>
#include <mc/deps/vanilla_components/AABBShapeComponent.h>
#include <mc/deps/vanilla_components/ActorDataBoundingBoxComponent.h>
#include <mc/deps/vanilla_components/ActorDataDirtyFlagsComponent.h>
#include <mc/deps/vanilla_components/ActorDataFlagComponent.h>
#include <mc/deps/vanilla_components/OffsetsComponent.h>
#include <mc/deps/vanilla_components/PlayerIsSleepingFlagComponent.h>
#include <mc/entity/components/IsHorizontalPoseFlagComponent.h>
#include <mc/entity/components/PlayerInputRequestComponent.h>
#include <mc/entity/components/VanillaOffsetComponent.h>
#include <mc/entity/systems/PlayerBoundingBoxStateUpdateSystem.h>
#include <mc/entity/systems/VanillaOffsetSystem.h>
#include <mc/entity/utilities/GetAttachPositionUtility.h>
#include <mc/nbt/CompoundTagVariant.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/util/VanillaOffsetSystemUtil.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/material/Material.h>

namespace mif::scale {

using namespace ll::memory_literals;

// 修复摄像机偏移
LL_TYPE_INSTANCE_HOOK(
    iScale::TickCameraOffsetHook,
    HookPriority::Normal,
    VanillaOffsetSystem,
    // VanillaOffsetSystem::tick
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
    for (auto entityId : registry->view<ActorDataFlagComponent, VanillaOffsetComponent, OffsetsComponent, ActorDataBoundingBoxComponent>()) {
        auto& vanillaOffsetComponent = registry->get<VanillaOffsetComponent>(entityId);
        auto& offsetsComponent       = registry->get<OffsetsComponent>(entityId);
        auto  scale                  = getScale(registry, entityId);
        auto cameraOffset = static_cast<decltype(&VanillaOffsetSystemUtil::getCameraOffset)>(
            "48 89 5C 24 ?? 48 89 7C 24 ?? 49 8B 00"_sig.resolve()
        )(
            registry->get<ActorDataFlagComponent>(entityId),
            std::bit_cast<Optional<PlayerIsSleepingFlagComponent const>>(
                Optional<PlayerIsSleepingFlagComponent>{
                    &registry->storage<PlayerIsSleepingFlagComponent>(entityId),
                    entityId
                }
            ),
            std::bit_cast<Optional<IsHorizontalPoseFlagComponent const>>(
                Optional<IsHorizontalPoseFlagComponent>{
                    &registry->storage<IsHorizontalPoseFlagComponent>(entityId),
                    entityId
                }
            ),
            0.35f
        );
        if (mUnk98f57d.as<bool>()) { // mUnk98f57d -> mIsClientSide
            auto prevY           = vanillaOffsetComponent.mPreviousOffset->y;
            auto originalNewY    = vanillaOffsetComponent.mCurrentOffset->y;
            auto heightOffset    = offsetsComponent.mHeightOffset;
            auto climbingOffsetY = vanillaOffsetComponent.mClimbingOffset->y;

            auto originalCameraOffset = prevY + heightOffset - climbingOffsetY - 2.0f * originalNewY; // 逆推原本的y
            auto scaledCameraOffset   = originalCameraOffset * scale;                                 // 缩放原本的y
            auto scaledNewY = (prevY + heightOffset - scaledCameraOffset - climbingOffsetY) / 2.0f;   // 计算缩放后的y

            vanillaOffsetComponent.mCurrentOffset->y = scaledNewY;
        } else {
            vanillaOffsetComponent.mCurrentOffset->y = offsetsComponent.mHeightOffset - scale * (offsetsComponent.mHeightOffset - cameraOffset);
        }
    }
    // clang-format on
}

// 数据持久化-加载缩放
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

// 数据持久化-保存缩放
LL_TYPE_INSTANCE_HOOK(iScale::ActorSaveHook, HookPriority::Low, Actor, &Actor::save, bool, CompoundTag& entityTag) {
    if (auto result = origin(entityTag); result) {
        entityTag["Scale"] = getScale(*this);
        return true;
    }
    return false;
}

// 数据持久化-初始化时再重新加载一次缩放
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

// 修复自动爬行/潜行
LL_TYPE_STATIC_HOOK(
    iScale::TickBoundingBoxHook,
    HookPriority::Normal,
    PlayerBoundingBoxStateUpdateSystem,
    // &PlayerBoundingBoxStateUpdateSystem::tickPlayerBoundingBoxStateUpdateSystem,
    "48 8B C4 48 89 58 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 4D 8B F0 4C 8B E2"_sig,
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
    auto& registry = ll::memory::dAccess<gsl::not_null<EntityRegistry*>>(&executionContext, 0)->mRegistry;
    if (!registry->any_of<ActorDataBoundingBoxComponent>(context.mEntity)) {
        return origin(context, aabb, request, executionContext, sneakHeight, views, region);
    }
    auto scale = getScale(registry, context.mEntity);

    static auto shrink = [](AABB& aabb, Vec3 offset) {
        aabb.min += offset;
        aabb.max -= offset;

        Vec3::forEachComponent([&]<typename axis_type, size_t iter> {
            auto& min = aabb.min.template get<axis_type, iter>();
            auto& max = aabb.max.template get<axis_type, iter>();

            if (min > max) {
                auto mid = (min + max) * 0.5f;
                min      = mid;
                max      = mid;
            }
        });
    };

    static auto doesIntersect = [](IConstBlockSource const&          blockSource,
                                   AABB const&                       testAABB,
                                   const GetCollisionShapeInterface* entity,
                                   std::vector<AABB>&                storage,
                                   bool                              withUnloadedChunks) {
        blockSource.fetchCollisionShapes(storage, testAABB, withUnloadedChunks, entity, nullptr);

        return std::ranges::any_of(storage, [&](auto& shape) { return shape.intersects(testAABB); });
    };

    // clang-format off
    request.mBreathingPoint = reinterpret_cast<decltype(&GetAttachPositionUtility::getPassengerAttachPos)>(
        "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 83 EC ?? F3 0F 10 84 24"_sig.resolve()
    )(
        context,
        views,
        SharedTypes::Legacy::ActorLocation::BreathingPoint,
        0.0f
    );
    // clang-format on

    {
        auto& material             = region.getMaterial(*request.mBreathingPoint);
        request.mBreathingInAir    = material.mType == MaterialType::Air;
        request.mBreathingInLiquid = material.mLiquid;
    }

    struct CollisionProxy {
    public:
        uintptr_t           mVTable{reinterpret_cast<uintptr_t>(ll::sys_utils::getImageRange().data()) + 0x845EA88};
        StrictEntityContext mEntity;
        decltype(&executionContext) mExecutionContext;

    public:
        CollisionProxy(StrictEntityContext context, decltype(&executionContext) executionContext)
        : mEntity(std::move(context)),
          mExecutionContext(executionContext) {}

    public:
        GetCollisionShapeInterface* getCollisionShape() { return reinterpret_cast<GetCollisionShapeInterface*>(this); }
    } collisionProxy{context, &executionContext};

    std::vector<AABB> blockStorage;

    {
        auto standingAABB   = aabb.mAABB;
        standingAABB->max.y = standingAABB->min.y + 1.8f * scale;
        shrink(standingAABB, {0.01f});
        if (doesIntersect(region, standingAABB, collisionProxy.getCollisionShape(), blockStorage, false)) {
            request.mUnblockedToStand = false;
        }
    }
    {
        auto sneakingAABB   = aabb.mAABB;
        sneakingAABB->max.y = sneakingAABB->min.y + sneakHeight * scale;
        shrink(sneakingAABB, {0.01f});
        if (doesIntersect(region, sneakingAABB, collisionProxy.getCollisionShape(), blockStorage, false)) {
            request.mUnblockedToSneak = false;
        }
    }
    {
        auto crawlingAABB   = aabb.mAABB;
        crawlingAABB->max.y = crawlingAABB->min.y + 0.6f * scale;
        shrink(crawlingAABB, {0.01f});
        if (doesIntersect(region, crawlingAABB, collisionProxy.getCollisionShape(), blockStorage, false)) {
            request.mUnblockedToCrawl = false;
        }
    }
}

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
    entity.mEntityContext->getOrAddComponent<ActorDataBoundingBoxComponent>()
        .mValue[std::to_underlying(ActorDataBoundingBoxComponent::Type::Scale)] = scale;
    entity.mEntityContext->getOrAddComponent<ActorDataDirtyFlagsComponent>().mDirtyFlags->set(
        std::to_underlying(ActorFlags::ShowBottom),
        true
    );
    auto& aabb = *entity.mBuiltInComponents->mAABBShapeComponent->mAABB;
    aabb.max.y = aabb.min.y + 1.8f * scale;
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