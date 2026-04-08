#pragma once
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/NativeMod.h>

class Actor;

namespace mif::scale {

class iScale {
public:
    struct GetCameraOffsetHook;
    struct AutoStepHook;
    struct ActorLoadHook;
    struct ActorSaveHook;
    struct ActorInitComponentHook;

    using Hooks = ll::memory::
        HookRegistrar<GetCameraOffsetHook, AutoStepHook, ActorLoadHook, ActorSaveHook, ActorInitComponentHook>;

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

private:
    ll::mod::NativeMod&              mSelf;
    ll::SmallDenseMap<Actor*, float> mScales;
};

} // namespace mif::scale