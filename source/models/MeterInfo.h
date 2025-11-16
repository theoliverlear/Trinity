#pragma once

struct MeterInfo
{
    float* levelPtr { nullptr };   // Pointer to value owned by the editor (displayTotal/Low/Mid/High)
    const char* label { nullptr }; // Static label text
};