#pragma once
#include <ll/api/mod/NativeMod.h>

namespace mif::scale {

class iScale {
public:
    struct GetCameraOffsetHook;

public:
    static iScale& getInstance();

    iScale() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace mif::scale