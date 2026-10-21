#include "launch_options.h"
#include <cstdlib>
#include <iostream>

void expect(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
billiardgl::LaunchOptions parse(std::initializer_list<const char*> values,billiardgl::LaunchPolicy policy=billiardgl::LaunchPolicy::Compatibility){std::vector<char*> args;for(const char* v:values)args.push_back(const_cast<char*>(v));return billiardgl::parseLaunchOptions(static_cast<int>(args.size()),args.data(),policy);}
int main(){
 auto normal=parse({"Billiards"}); expect(normal.ok&&normal.mode==billiardgl::RunMode::Interactive,"default interactive");
 expect(normal.runtime.rendererBackend==billiardgl::RendererBackendSelection::Legacy,"legacy renderer is the migration default");
 auto legacy=parse({"Billiards","--renderer","legacy"}); expect(legacy.ok&&legacy.runtime.rendererBackend==billiardgl::RendererBackendSelection::Legacy,"explicit legacy renderer accepted");
 auto modern=parse({"Billiards","--renderer","modern"}); expect(modern.ok&&modern.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,"modern renderer accepted by launch configuration");
 auto product=parse({"Billiards","--product-mode","--settings-path","settings.json"});
 expect(product.ok&&product.productMode&&product.productSettingsPath=="settings.json"&&product.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,"product lifecycle is an explicit modern opt-in");
 expect(!normal.productMode,"legacy lifecycle remains the default migration gate");
 expect(!parse({"Billiards","--settings-path","settings.json"}).ok,"settings path is scoped to product mode");
 expect(!parse({"Billiards","--product-mode","--renderer","legacy"}).ok,"product lifecycle rejects legacy renderer");
 expect(!parse({"Billiards","--product-mode","--automation","--rendered"}).ok,"product lifecycle cannot enter automation event paths");
 expect(!parse({"Billiards","--product-mode","--screenshot","frame.ppm"}).ok,"legacy screenshot capture remains outside product lifecycle");
 expect(!parse({"Billiards","--product-mode","--ai-opponent"}).ok,"product shell owns AI configuration");
 auto performance=parse({"Billiards","--product-mode","--width","1920","--height","1080","--product-frame-performance","report.json"});
 expect(performance.ok&&performance.productFramePerformancePath=="report.json","visible product frame performance mode is explicit");
 expect(!parse({"Billiards","--product-frame-performance","report.json"}).ok,"frame performance requires product mode");
 expect(!parse({"Billiards","--product-mode","--product-frame-performance"}).ok,"frame performance requires a report path");
 expect(!parse({"Billiards","--renderer","future"}).ok,"unknown renderer rejected");
 expect(!parse({"Billiards","--renderer"}).ok,"missing renderer rejected");
 auto headless=parse({"Billiards","--automation","--transport","stdio","--headless"}); expect(headless.ok&&headless.mode==billiardgl::RunMode::AutomationHeadless,"headless accepted");
 auto rendered=parse({"Billiards","--automation","--transport","stdio","--rendered"}); expect(rendered.ok&&rendered.mode==billiardgl::RunMode::AutomationRendered,"rendered accepted");
 auto profile=parse({"Billiards","--print-physics-profile"}); expect(profile.ok&&profile.printPhysicsProfile,"profile query accepted");
 auto ai=parse({"Billiards","--ai-opponent","--ai-difficulty","expert"});
 expect(ai.ok&&ai.aiOpponent&&ai.aiDifficulty=="expert"&&ai.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,"AI opponent selects modern product path and difficulty");
 expect(!parse({"Billiards","--ai-difficulty","impossible"}).ok,"unknown AI difficulty rejected");
 expect(!parse({"Billiards","--automation","--headless","--ai-opponent"}).ok,"automation configures AI through protocol commands");
 auto sized=parse({"Billiards","--width","1920","--height","1080","--screenshot","frame.ppm"});
 expect(sized.ok&&sized.runtime.width==1920&&sized.runtime.height==1080,"explicit dimensions accepted");
 expect(!parse({"Billiards","--width","319"}).ok,"undersized width rejected");
 expect(!parse({"Billiards","--height","not-a-number"}).ok,"invalid height rejected");
 expect(!parse({"Billiards","--width"}).ok,"missing width rejected");
 expect(!parse({"Billiards","--print-physics-profile","--automation","--headless"}).ok,"profile query is exclusive");
 expect(!parse({"Billiards","--headless"}).ok,"headless requires automation");
 expect(!parse({"Billiards","--automation","--transport","tcp","--headless"}).ok,"unknown transport rejected");
 using billiardgl::LaunchPolicy;
 const auto shipping=parse({"Billiards"},LaunchPolicy::Product);
 expect(shipping.ok&&shipping.productMode&&shipping.mode==billiardgl::RunMode::Interactive&&
   shipping.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,"production no-argument entry is modern product");
 expect(billiardgl::RuntimeConfig{}.rendererBackend==billiardgl::RendererBackendSelection::Legacy,
   "production policy must not change shared runtime/physics fixture default");
 const auto retired=parse({"Billiards","--renderer","legacy"},LaunchPolicy::Product);
 expect(!retired.ok&&retired.error=="legacy renderer retired from production; use BilliardsLegacyCompatibility for historical tests",
   "production rejects fixed-pipeline selection without fallback");
 expect(!parse({"Billiards","--renderer","legacy","--renderer","modern"},LaunchPolicy::Product).ok,
   "later modern flag cannot undo retired legacy request");
 const auto productionHeadless=parse({"Billiards","--automation","--headless"},LaunchPolicy::Product);
 expect(productionHeadless.ok&&!productionHeadless.productMode&&productionHeadless.mode==billiardgl::RunMode::AutomationHeadless,
   "production headless remains independent from product window");
 const auto productionRendered=parse({"Billiards","--automation","--rendered"},LaunchPolicy::Product);
 expect(productionRendered.ok&&!productionRendered.productMode&&productionRendered.mode==billiardgl::RunMode::AutomationRendered&&
   productionRendered.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,"production rendered automation is modern and not product lifecycle");
 for(const char* scene:{"default","help","aim","after-shot"}){
   const auto capture=parse({"Billiards","--screenshot","frame.ppm","--screenshot-scene",scene},LaunchPolicy::Product);
   expect(capture.ok&&!capture.productMode&&capture.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,
     "production standalone screenshots use independent modern route");
 }
 const auto productionProfile=parse({"Billiards","--print-physics-profile"},LaunchPolicy::Product);
 expect(productionProfile.ok&&productionProfile.printPhysicsProfile&&!productionProfile.productMode,
   "production profile query remains CPU-only");
 expect(parse({"Billiards","--settings-path","settings.json"},LaunchPolicy::Product).ok,
   "production default product accepts settings path");
 expect(parse({"Billiards","--product-frame-performance","report.json"},LaunchPolicy::Product).ok,
   "production default visible product accepts performance evidence path");
 expect(!parse({"Billiards","--product-mode","--automation","--rendered"},LaunchPolicy::Product).ok,
   "production explicit product cannot enter automation");
 expect(!parse({"Billiards","--product-mode","--screenshot","frame.ppm"},LaunchPolicy::Product).ok,
   "production explicit product cannot use independent screenshot path");
 expect(!parse({"Billiards","--headless"},LaunchPolicy::Product).ok,
   "production still requires automation for headless");
 expect(!parse({"Billiards","--width","319"},LaunchPolicy::Product).ok,
   "production retains dimension validation");
 const auto productionAi=parse({"Billiards","--ai-opponent"},LaunchPolicy::Product);
 expect(productionAi.ok&&productionAi.aiOpponent&&!productionAi.productMode&&
   productionAi.runtime.rendererBackend==billiardgl::RendererBackendSelection::Modern,
   "historical direct AI shortcut remains modern-only diagnostic route");
 std::cout<<"launch_options compatibility=PASS production_routes=PASS shared_runtime_default_unchanged=PASS\n";
 return 0;
}
