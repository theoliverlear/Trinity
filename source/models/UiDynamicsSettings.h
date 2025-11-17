#pragma once

struct UiDynamicsSettings
{
    // Magnitude smoothing
    float attackCoeff { 0.35f };  // faster rise
    float releaseCoeff { 0.08f }; // slower fall

    // Peak-hold behaviour
    float peakHoldDecay { 0.97f }; // per update decay

    // Feature toggles
    bool smoothingEnabled { true };
    bool peakHoldEnabled { true };
};
