#include "launch_options.h"
#include <cstdlib>
#include <iostream>

void expect(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
billiardgl::LaunchOptions parse(std::initializer_list<const char*> values){std::vector<char*> args;for(const char* v:values)args.push_back(const_cast<char*>(v));return billiardgl::parseLaunchOptions(static_cast<int>(args.size()),args.data());}
int main(){
 auto normal=parse({"Billiards"}); expect(normal.ok&&normal.mode==billiardgl::RunMode::Interactive,"default interactive");
 auto headless=parse({"Billiards","--automation","--transport","stdio","--headless"}); expect(headless.ok&&headless.mode==billiardgl::RunMode::AutomationHeadless,"headless accepted");
 auto rendered=parse({"Billiards","--automation","--transport","stdio","--rendered"}); expect(rendered.ok&&rendered.mode==billiardgl::RunMode::AutomationRendered,"rendered accepted");
 auto profile=parse({"Billiards","--print-physics-profile"}); expect(profile.ok&&profile.printPhysicsProfile,"profile query accepted");
 expect(!parse({"Billiards","--print-physics-profile","--automation","--headless"}).ok,"profile query is exclusive");
 expect(!parse({"Billiards","--headless"}).ok,"headless requires automation");
 expect(!parse({"Billiards","--automation","--transport","tcp","--headless"}).ok,"unknown transport rejected");
 return 0;
}
