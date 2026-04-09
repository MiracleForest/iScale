#pragma once
#include <ll/api/event/ListenerBase.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/NativeMod.h>

class Actor;
class EntityId;

namespace mif::scale {

class iScale {
public:
    struct GetCameraOffsetHook;
    struct TickCameraOffsetHook;
    struct ActorLoadHook;
    struct ActorSaveHook;
    struct ActorInitComponentHook;
    struct TickBoundingBoxHook;

    using Hooks = ll::memory::
        HookRegistrar<TickCameraOffsetHook, ActorLoadHook, ActorSaveHook, ActorInitComponentHook, TickBoundingBoxHook>;

public:
    static iScale& getInstance();

    iScale() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

    static void  setScale(Actor& entity, float scale);
    static float getScale(Actor& entity);
    static float getScale(entt::basic_registry<EntityId>& registry, EntityId const& entityId);

private:
    ll::mod::NativeMod&              mSelf;
    ll::event::ListenerPtr           mListener;
    ll::SmallDenseMap<Actor*, float> mScales;
};

} // namespace mif::scale