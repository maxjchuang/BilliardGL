// Bounded diagnostic: compile the exact production comparison helper in an
// isolated Core 4.1 context, without running the scene or changing its state.
#import <Cocoa/Cocoa.h>
#include <OpenGL/gl3.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
GLuint shader(GLenum type,const std::string& source) {
    GLuint id=glCreateShader(type);const char* text=source.c_str();
    glShaderSource(id,1,&text,nullptr);glCompileShader(id);GLint ok=0;
    glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){std::array<char,4096> log{};glGetShaderInfoLog(id,log.size(),nullptr,log.data());throw std::runtime_error(log.data());}
    return id;
}
}

int main(int argc,const char** argv) {
    @autoreleasepool {
      try {
        if(argc!=2)throw std::runtime_error("usage: shadow_compare_gpu_boundary modern_renderer.cpp");
        std::ifstream in(argv[1]);const std::string source((std::istreambuf_iterator<char>(in)),{});
        const auto first=source.find("float shadowDepthCompare(");
        const auto last=source.find("float shadowVisibility(){",first);
        if(first==std::string::npos||last==std::string::npos)throw std::runtime_error("production helper source missing");
        NSOpenGLPixelFormatAttribute attrs[]={NSOpenGLPFAOpenGLProfile,
          static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),NSOpenGLPFAAccelerated,0};
        NSOpenGLPixelFormat* format=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        NSOpenGLContext* context=[[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
        if(!context)throw std::runtime_error("Core4.1 unavailable");[context makeCurrentContext];
        const GLuint vs=shader(GL_VERTEX_SHADER,"#version 330 core\nvoid main(){vec2 p=vec2(gl_VertexID==1?3:-1,gl_VertexID==2?3:-1);gl_Position=vec4(p,0,1);}");
        const std::string fragment="#version 330 core\nuniform sampler2D uShadowMap;uniform int uShadowCompareBilinear;uniform float uReceiver;layout(location=0)out vec4 result;\n"+
          source.substr(first,last-first)+
          "void main(){float uv=0.25+floor(gl_FragCoord.x)*0.125;float value=shadowCompareFiltered(vec2(uv,0.5),uReceiver);result=vec4(value,0,0,1);}";
        const GLuint fs=shader(GL_FRAGMENT_SHADER,fragment);
        const GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
        GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("boundary program link failed");
        GLuint vao=0,depth=0,color=0,fbo=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glGenTextures(1,&depth);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,depth);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,2,2,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,4,1,0,GL_RGBA,GL_FLOAT,nullptr);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("boundary framebuffer incomplete");
        glUseProgram(program);glUniform1i(glGetUniformLocation(program,"uShadowMap"),0);glBindTexture(GL_TEXTURE_2D,depth);glViewport(0,0,4,1);
        const auto check=[&](const char* label,std::array<float,4> depths,float receiver,bool bilinear,std::array<float,4> expected){
          glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,2,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
          glUniform1i(glGetUniformLocation(program,"uShadowCompareBilinear"),bilinear?1:0);
          glUniform1f(glGetUniformLocation(program,"uReceiver"),receiver);glDrawArrays(GL_TRIANGLES,0,3);
          std::array<float,16> values{};glReadPixels(0,0,4,1,GL_RGBA,GL_FLOAT,values.data());
          if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("boundary GL error");
          for(int i=0;i<4;++i)if(std::abs(values[i*4]-expected[i])>1e-5f)throw std::runtime_error(std::string(label)+" mismatch");
          std::cout << label << " actual=["<<values[0]<<','<<values[4]<<','<<values[8]<<','<<values[12]<<"] PASS\n";
        };
        check("empty_map_fail_lit",{1,1,1,1},.9f,true,{1,1,1,1});
        check("fully_occluded",{.2f,.2f,.2f,.2f},.5f,true,{0,0,0,0});
        check("equal_depth_no_self_acne",{.5f,.5f,.5f,.5f},.5f,true,{1,1,1,1});
        check("positive_receiver_bias_lit",{.5f,.5f,.5f,.5f},.5f-.00225f,true,{1,1,1,1});
        check("bilinear_compare_boundary",{.2f,.8f,.2f,.8f},.5f,true,{0,.25f,.5f,.75f});
        check("nearest_reference_boundary",{.2f,.8f,.2f,.8f},.5f,false,{0,0,1,1});
        glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&depth);glDeleteTextures(1,&color);glDeleteVertexArrays(1,&vao);
        glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);[NSOpenGLContext clearCurrentContext];
        return 0;
      } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
    }
}
