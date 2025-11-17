#pragma once

struct MeterDbScaleSpec
{
    float minDb { -120.0f };
    float maxDb { 0.0f };

    static const float* ticks() noexcept
    {
        static const float values[] = {
            -120.0f, -90.0f, -60.0f, -48.0f, -36.0f, -30.0f, -24.0f,
            -18.0f, -12.0f, -9.0f, -6.0f, -3.0f, 0.0f
        };
        return values;
    }

    static int tickCount() noexcept
    {
        return 13;
    }

    static bool isLabeledTick(float db) noexcept
    {
        return db == 0.0f
            || db == -3.0f
            || db == -6.0f
            || db == -12.0f
            || db == -30.0f
            || db == -60.0f
            || db == -120.0f;
    }
};
