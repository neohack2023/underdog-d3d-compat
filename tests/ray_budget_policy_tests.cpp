#include "underdog/RayBudgetController.h"
#include <iostream>
#include <limits>
using namespace underdog;
static int failures=0;
static void check(bool cond,const char* label) {
    if(!cond){std::cerr<<"FAIL "<<label<<"\n";++failures;}
}
static RayFrameSample comfortable(double ray=0.1) {
    RayFrameSample s{};
    s.frameMs=22.0;s.baseFrameMs=20.0;s.rayGpuMs=ray;
    s.timestampsValid=true;s.baselineValid=true;s.deviceHealthy=true;
    return s;
}
int main(){
    RayBudgetConfig cfg{};
    cfg.enabled=true;
    cfg.frameBudgetMs=33.333;cfg.rayBudgetMs=0.75;
    cfg.recoveryFrames=3;cfg.overloadFrames=2;cfg.cooldownFrames=2;
    RayBudgetController c(cfg);
    check(c.plan(128,128).rate==RayRate::Off,"fail closed before telemetry");
    auto ok=comfortable();
    check(c.observe(ok)==RayRate::Off,"warmup 1");
    check(c.observe(ok)==RayRate::Off,"warmup 2");
    check(c.observe(ok)==RayRate::Quarter,"warmup 3 enables quarter");
    auto p=c.plan(128,128);
    check(p.rate==RayRate::Quarter&&p.scheduledRays==4096&&p.sourcePixels==16384,"quarter cap");
    check(c.plan(256,256).rate==RayRate::Off,"large scene disables");
    check(c.plan(65536,65536).rate==RayRate::Off,"dimensions overflow guarded");
    check(c.plan(128,0).rate==RayRate::Off,"zero height guarded");
    c.setHistoryValid(true);
    check(c.plan(128,128,true).historyReusable,"history flag opt in");
    ok.sceneChanged=true;
    c.observe(ok);
    check(!c.plan(128,128,true).historyReusable,"scene changed history invalidated");
    ok.sceneChanged=false;
    for(int i=0;i<6;++i)c.observe(ok); // first 2 frames are cooldown after promotion
    check(c.rate()==RayRate::Half,"recovery climbs to half");
    auto over=comfortable(1.25);over.frameMs=33.9;
    c.observe(over);
    check(c.rate()==RayRate::Half,"one spike no thrash");
    c.observe(over);
    check(c.rate()==RayRate::Quarter,"two spikes downshift");
    c.observe(over);c.observe(over);
    check(c.rate()==RayRate::Off,"two more spikes disable");
    for(int i=0;i<10;++i)c.observe(ok);
    check(c.rate()!=RayRate::Off,"recovers after stable feedback");
    auto bad=ok;bad.timestampsValid=false;
    check(c.observe(bad)==RayRate::Off,"lost timestamp fails closed");
    for(int i=0;i<5;++i)c.observe(ok);
    check(c.rate()!=RayRate::Off,"restarted after valid samples");
    bad=ok;bad.baseFrameMs=32;
    check(c.observe(bad)==RayRate::Off,"protect main-frame baseline");
    bad=ok;bad.deviceHealthy=false;
    check(c.observe(bad)==RayRate::Off,"device loss");
    bad=ok;bad.frameMs=std::numeric_limits<double>::quiet_NaN();
    check(c.observe(bad)==RayRate::Off,"nan guard");
    bad=ok;bad.rayGpuMs=std::numeric_limits<double>::infinity();
    check(c.observe(bad)==RayRate::Off,"infinite timing guard");
    cfg.enabled=false;
    RayBudgetController disabled(cfg);
    for(int i=0;i<10;++i)disabled.observe(ok);
    check(disabled.rate()==RayRate::Off,"explicit opt in required");
    cfg.enabled=true;
    cfg.reservedHeadroomMs=std::numeric_limits<double>::quiet_NaN();
    RayBudgetController invalidConfig(cfg);
    for(int i=0;i<6;++i)invalidConfig.observe(ok);
    check(invalidConfig.rate()==RayRate::Off,"invalid headroom fails closed");
    std::cout<<"RAY_BUDGET_POLICY "<<(failures?"FAIL":"PASS")<<" failures="<<failures<<"\n";
    return failures?1:0;
}
