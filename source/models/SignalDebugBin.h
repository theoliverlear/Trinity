//
// Created by olive on 11/20/2025.
//

#ifndef TRINITY_SIGNALDEBUGBIN_H
#define TRINITY_SIGNALDEBUGBIN_H
#include <atomic>
#include <vector>

class SignalDebugBin
{
public:
    std::atomic<bool> debugCaptureEnabled { false };
    std::vector<float> debugTailBinsPreSmooth;
    std::vector<float> debugTailBinsPostSmooth;
    std::vector<float> debugTailBinsPostTaper;
    std::vector<float> debugBandsPreBandSmooth;
};


#endif //TRINITY_SIGNALDEBUGBIN_H