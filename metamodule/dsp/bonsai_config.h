// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "effect_config.h"
namespace surge_mm
{
struct BonsaiSettings : EffectSettings
{
    int bass_mode{};
};
struct BonsaiConfig : EffectConfig
{
    using EffectStorage = BonsaiSettings;
    struct BaseClass
    {
        EffectStorage *settings;
        BaseClass(GlobalStorage *, EffectStorage *s, ValueStorage *) : settings(s)
        {
        }
    };
    static float floatValueAt(const BaseClass *const, const ValueStorage *const v, int i)
    {
        return v[i];
    }
    static int intValueAt(const BaseClass *const, const ValueStorage *const v, int i)
    {
        return int(std::round(v[i]));
    }
    static float floatValueExtendedAt(const BaseClass *const b, const ValueStorage *const v, int i)
    {
        return v[i] * (i == 1 && b->settings->extended[i] ? 3.f : 1.f);
    }
    static float temposyncRatio(GlobalStorage *s, EffectStorage *e, int i)
    {
        return EffectConfig::temposyncRatio(s, e, i);
    }
    static bool isDeactivated(EffectStorage *e, int i)
    {
        return e->deactivated[i];
    }
    static bool isExtended(EffectStorage *e, int i)
    {
        return e->extended[i];
    }
    static int deformType(EffectStorage *e, int i)
    {
        return i == 1 ? e->bass_mode : 0;
    }
};
} // namespace surge_mm
