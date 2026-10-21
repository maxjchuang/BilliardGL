// A bounded visual diagnostic reuses the established sparse scene fixtures
// and capture helpers; it never implements physics or changes product state.
#define main retainedSliceSmokeMain
#include "../spikes/render_core_profile/modern_renderer_slice_smoke.mm"
#undef main

int main(int argc,const char** argv) {
  @autoreleasepool {
    try {
      if(argc!=2)throw std::runtime_error("usage: ball_rect_specular_motion output_dir");
      const std::filesystem::path output(argv[1]);std::filesystem::create_directories(output);
      NSOpenGLPixelFormatAttribute attrs[]={NSOpenGLPFAOpenGLProfile,
        static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),NSOpenGLPFAAccelerated,0};
      NSOpenGLPixelFormat* format=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
      NSOpenGLContext* context=[[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
      if(!context)throw std::runtime_error("Core4.1 unavailable");[context makeCurrentContext];
      billiardgl::render::ModernRendererOptions options;
      options.msaaSamples=4;options.debugErrors=true;
      options.quality=billiardgl::render::RenderQuality::Medium;
      options.enablePocketIndirectBounce=true;
      options.lookdevManifestPath=std::string(BILLIARDGL_SOURCE_ROOT)+"/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
      const std::string phase2=phase2AssetRoot();
      options.pbrParametersPath=phase2+"focused_practice_hall_pbr_v1.params.json";
      options.pbrAssetManifestPath=phase2+"focused_practice_hall_modular_v1.asset.json";
      options.lightingRigPath=phase2+"focused_practice_hall_lighting_rig_v1.json";
      options.gltfScenePath=phase2+"focused_practice_hall_modular_v1.gltf";
      options.venueScenePath=phase2+"focused_practice_hall_venue_v1.gltf";
      const auto base=heroBallSnapshot();
      std::string error;
      billiardgl::render::ModernRenderer renderer(options);
      if(!renderer.initialize({1920,1080,true},error))throw std::runtime_error(error);
      const auto capture=[&](const char* label,const billiardgl::PresentationSnapshot& frame) {
        renderer.render(frame);
        if(!renderer.capture({(output/(std::string(label)+".ppm")).string(),1920,1080},error))throw std::runtime_error(error);
        std::cout<<label<<" camera_eye="<<frame.camera.eye.x<<','<<frame.camera.eye.y<<','<<frame.camera.eye.z
          <<" ball8="<<frame.balls[8].position.x<<','<<frame.balls[8].position.y<<','<<frame.balls[8].position.z<<'\n';
      };
      capture("base",base);
      auto minus=base,plus=base;minus.camera.eye.x-=6;plus.camera.eye.x+=6;
      capture("camera_minus6cm",minus);capture("camera_plus6cm",plus);
      minus=base;plus=base;minus.balls[8].position.x-=1;plus.balls[8].position.x+=1;
      capture("ball8_minus1cm",minus);capture("ball8_plus1cm",plus);
      renderer.shutdown();
      // The mask uses its own shaders and must not poison a later normal
      // picture. Guide/UI are still drawn but own black/other pixels.
      options.objectClassMaskPass=true;
      billiardgl::render::ModernRenderer mask(options);
      if(!mask.initialize({1920,1080,true},error))throw std::runtime_error(error);
      mask.render(base);
      if(!mask.capture({(output/"base-object-mask.ppm").string(),1920,1080},error))throw std::runtime_error(error);
      const auto aim=aimSnapshot();mask.render(aim);
      if(!mask.capture({(output/"aim-object-mask.ppm").string(),1920,1080},error))throw std::runtime_error(error);
      mask.shutdown();
      options.objectClassMaskPass=false;
      billiardgl::render::ModernRenderer restored(options);
      if(!restored.initialize({1920,1080,true},error))throw std::runtime_error(error);
      restored.render(base);
      if(!restored.capture({(output/"base-after-mask.ppm").string(),1920,1080},error))throw std::runtime_error(error);
      restored.shutdown();
      if(checksum(output/"base.ppm")!=checksum(output/"base-after-mask.ppm"))throw std::runtime_error("mask mode changed normal picture");
      std::cout<<"normal_after_mask_byte_identical=PASS bounded_motion_frames=5\n";
      [NSOpenGLContext clearCurrentContext];return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
  }
}
