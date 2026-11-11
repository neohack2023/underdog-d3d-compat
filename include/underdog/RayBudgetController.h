// Underdog experimental adaptive ray workload policy v0.1 (MIT).
// Control-plane only: no driver emulation, GPU dispatch, or quality reconstruction.
// Fail-closed: unknown timing, device errors, and baseline overload disable rays.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace underdog {
enum class RayRate : uint8_t { Off=0, Quarter=1, Half=2, Full=3 };

struct RayBudgetConfig {
    bool enabled=false;             // explicit opt-in, no automatic game interception
    double frameBudgetMs=33.333;     // example 30-FPS envelope, NOT a GPU benchmark
    double rayBudgetMs=0.75;        // app-assigned marginal ray GPU budget
    double reservedHeadroomMs=2.0;  // protect baseline frame rendering
    uint32_t maxRaysPerFrame=4096;
    unsigned overloadFrames=2;
    unsigned recoveryFrames=30;
    unsigned cooldownFrames=12;
};
struct RayFrameSample {
    double frameMs=0;       // complete app frame including ray work, measured outside this class
    double baseFrameMs=0;   // separately measured no-ray baseline, same scene/conditions
    double rayGpuMs=0;      // GPU timestamp-delta for bounded ray dispatch
    bool timestampsValid=false;
    bool deviceHealthy=true;
    bool baselineValid=false;
    bool sceneChanged=false; // invalidate reuse outside the controller
};
struct RayPlan {
    RayRate rate=RayRate::Off;
    uint32_t scheduledRays=0;
    uint32_t sourcePixels=0;
    bool historyReusable=false; // requires external motion/depth/normal validity too
    const char* reason="not enabled";
};

class RayBudgetController {
    RayBudgetConfig config_{};
    RayRate rate_=RayRate::Off;
    unsigned overload_=0, recovered_=0, cooldown_=0;
    bool validated_=false;
    bool historyValid_=false;
    static bool finiteNonnegative(double n) noexcept {return std::isfinite(n) && n>=0.0;}
    static unsigned divisor(RayRate r) noexcept {
        return r==RayRate::Full?1u:r==RayRate::Half?2u:r==RayRate::Quarter?4u:0u;
    }
    void disable() noexcept {
        rate_=RayRate::Off; overload_=recovered_=cooldown_=0;validated_=false;historyValid_=false;
    }
    static RayRate down(RayRate r) noexcept {
        return r==RayRate::Full?RayRate::Half:r==RayRate::Half?RayRate::Quarter:RayRate::Off;
    }
    static RayRate up(RayRate r) noexcept {
        return r==RayRate::Off?RayRate::Quarter:r==RayRate::Quarter?RayRate::Half:RayRate::Full;
    }
public:
    explicit RayBudgetController(RayBudgetConfig cfg={}):config_(cfg){}
    void reset() noexcept {disable();}
    RayRate rate() const noexcept{return rate_;}

    // Supply *real GPU timestamps*, not CPU submission or GPU-fence wait times.
    // This policy cannot guarantee the driver will complete in the budget:
    // applications must cap dispatch, bound waits and preserve normal rendering.
    RayRate observe(const RayFrameSample& sample) noexcept {
        if(!config_.enabled || !sample.deviceHealthy || !sample.timestampsValid ||
           !sample.baselineValid || !finiteNonnegative(sample.frameMs) ||
           !finiteNonnegative(sample.baseFrameMs) ||
           !finiteNonnegative(sample.rayGpuMs) ||
           !std::isfinite(config_.frameBudgetMs) ||
           !std::isfinite(config_.rayBudgetMs) ||
           config_.frameBudgetMs<=0.0 || config_.rayBudgetMs<=0.0 ||
           config_.maxRaysPerFrame==0) {
            disable();
            return rate_;
        }
        validated_=true;
        if(sample.sceneChanged)historyValid_=false;
        const bool baselineCrowded=sample.baseFrameMs+config_.reservedHeadroomMs>=config_.frameBudgetMs;
        const bool overloaded=baselineCrowded ||
            sample.frameMs>config_.frameBudgetMs || sample.rayGpuMs>config_.rayBudgetMs;
        const bool comfortable=!baselineCrowded &&
            sample.frameMs<config_.frameBudgetMs*0.80 &&
            sample.rayGpuMs<config_.rayBudgetMs*0.65;
        if(overloaded){
            recovered_=0;
            if(baselineCrowded) {disable();return rate_;}
            if(++overload_>=std::max(1u,config_.overloadFrames)){
                rate_=down(rate_);
                overload_=0;
                cooldown_=config_.cooldownFrames;
                historyValid_=false;
            }
            return rate_;
        }
        overload_=0;
        if(cooldown_>0){--cooldown_;recovered_=0;return rate_;}
        if(comfortable){
            if(++recovered_>=std::max(1u,config_.recoveryFrames)){
                rate_=up(rate_);
                recovered_=0;
                cooldown_=config_.cooldownFrames;
            }
        } else recovered_=0;
        return rate_;
    }

    // Exact per-frame worst-case active pixel cap; no fallback to unbounded full dispatch.
    // Rate denominators represent a 1/4 or 1/2 interleaved ray schedule, NOT
    // a promise of a complete, denoised output image.
    RayPlan plan(uint32_t width,uint32_t height,bool historyAvailable=false) const noexcept {
        if(!validated_ || rate_==RayRate::Off || !config_.enabled || width==0 || height==0)
            return {RayRate::Off,0,0,false,"disabled or uncalibrated"};
        const uint64_t px=uint64_t(width)*height;
        if(px>UINT32_MAX)return {RayRate::Off,0,0,false,"dimensions exceed supported range"};
        RayRate selected=rate_;
        unsigned div=divisor(selected);
        uint64_t rays=(px+div-1)/div;
        while(rays>config_.maxRaysPerFrame && selected!=RayRate::Off){
            selected=down(selected);div=divisor(selected);
            if(div)rays=(px+div-1)/div;
        }
        if(selected==RayRate::Off)return {RayRate::Off,0,static_cast<uint32_t>(px),false,"ray cap exceeded"};
        return {selected,static_cast<uint32_t>(rays),static_cast<uint32_t>(px),
                historyAvailable && historyValid_,"scheduled; GPU time not guaranteed"};
    }
    // Caller may mark history valid after independently validating motion/depth
    // and completing a trace; scene changes always invalidate it.
    void setHistoryValid(bool valid) noexcept {historyValid_=valid && validated_ && rate_!=RayRate::Off;}
};
} // namespace underdog
