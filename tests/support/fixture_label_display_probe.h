#pragma once
#include "nanovg_canvas_text.h"
#include "projectutils.h"
#include "../../viewer_common/gl_framebuffer_capture_target.h"
#include "../../viewer3d/render/opengl_state_guard.h"
#include "fixture_label_name.h"
#include "typography/composition.h"
#include "typography/font_resources.h"
#include <iostream>
#include <stdexcept>

// Compare the shared adapter with the established scene raster halo at subpixel
// origins. Aggregate ink area alone misses a misregistered contour and fill.
inline void CheckFixtureLabelRasterRegistration() {
  viewer3d::render::OpenGLStateGuard state;
  GLfloat clear[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
  glcapture::FramebufferCaptureTarget target;
  if (!target.Initialize(256,160)) throw std::runtime_error("Registration framebuffer failed");
  std::unique_ptr<NVGcontext,decltype(&nvgDeleteGL2)> vg(
      nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES),nvgDeleteGL2);
  if (!vg) throw std::runtime_error("Registration NanoVG context failed");
  const typography::FontPair fonts(ProjectUtils::GetResourceRoot());
  for (bool bold : {false,true}) {
    const char *family=bold?"perastage-sans-bold":"perastage-sans";
    const auto &font=bold?fonts.bold:fonts.regular;
    const int id=nvgCreateFont(vg.get(),family,typography::BundledFontPath(fonts.resourceRoot,bold).string().c_str());
    if (id<0) throw std::runtime_error("Registration font failed");
    for (double dpi : {1.,1.5,2.}) for (double em : {8.,24.,48.})
    for (double fraction : {.1,.49,.9}) for (const char *glyph : {"M","3"}) {
      TextCommand text; text.text=glyph;
      text.style.fontFamily=family; text.style.fontSize=.12;
      text.style.color={1,1,1,1}; text.style.outlineColor={0,0,0,1};
      text.style.outlineWidth=typography::kFixtureLabelOutlineReferenceMetres;
      text.style.glyphOutline=typography::ReadGlyphOutline(font,static_cast<unsigned char>(glyph[0]));
      std::vector<unsigned char> reference;
      for (bool adapter : {false,true}) {
        target.BindForRendering(); glViewport(0,0,256,160); glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST); glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(vg.get(),256/dpi,160/dpi,dpi);
        const float x=(40+fraction)/dpi, y=(120+fraction)/dpi;
        if (adapter) {
          for (auto pass : {typography::TextInkPass::Outline,typography::TextInkPass::Fill})
            viewer2d::DrawCanvasText(vg.get(),text,x,y,em/(dpi*text.style.fontSize),pass,dpi);
        } else {
          nvgFontFaceId(vg.get(),id); nvgFontSize(vg.get(),em/dpi);
          nvgTextAlign(vg.get(),NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
          nvgFillColor(vg.get(),nvgRGB(0,0,0));
          for (const auto &offset : typography::OutlineOffsets(1/dpi))
            nvgText(vg.get(),x+offset[0],y+offset[1],glyph,nullptr);
          nvgFillColor(vg.get(),nvgRGB(255,255,255)); nvgText(vg.get(),x,y,glyph,nullptr);
        }
        nvgEndFrame(vg.get()); target.BindForReading();
        std::vector<unsigned char> pixels(256*160*4);
        glReadPixels(0,0,256,160,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        if (!adapter) reference=std::move(pixels);
        else for (size_t i=0;i<pixels.size();++i)
          if (std::abs(int(pixels[i])-int(reference[i]))>1)
            throw std::runtime_error("Fixture label contour/fill registration differs from scene raster halo");
      }
    }
  }
  glClearColor(clear[0],clear[1],clear[2],clear[3]);
  std::cout << "Label raster registration passed: regular/bold, 3 sizes, 3 pixel ratios, subpixel origins" << std::endl;
}

inline void CheckFixtureLabelRasterMetrics() {
  viewer3d::render::OpenGLStateGuard state;
  std::unique_ptr<NVGcontext,decltype(&nvgDeleteGL2)> vg(
      nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES),nvgDeleteGL2);
  if (!vg) throw std::runtime_error("Metric probe context failed");
  const typography::FontPair fonts(ProjectUtils::GetResourceRoot());
  nvgBeginFrame(vg.get(),1024,256,1);
  for (bool bold : {false,true}) {
    const char *family=bold?"perastage-sans-bold":"perastage-sans";
    const auto &font=bold?fonts.bold:fonts.regular;
    const int id=nvgCreateFont(vg.get(),family,typography::BundledFontPath(fonts.resourceRoot,bold).string().c_str());
    if (id<0) throw std::runtime_error("Metric probe font failed");
    const double factor=viewer2d::ResolveRasterFontEmScale(vg.get(),family,font);
    for (double em : {12.,32.,64.}) {
      nvgFontFaceId(vg.get(),id); nvgFontSize(vg.get(),em*factor);
      nvgTextAlign(vg.get(),NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE); nvgTextLetterSpacing(vg.get(),0);
      const double measured=nvgTextBounds(vg.get(),0,0,"M",nullptr,nullptr);
      const double expected=font.advanceWidths['M']*em/font.unitsPerEm;
      if (std::abs(measured-expected)>1.05)
        throw std::runtime_error("Raster glyph advance disagrees with canonical TTF composition");
    }
  }
  nvgEndFrame(vg.get());
  std::cout << "Canonical screen font metrics passed: regular/bold, 3 sizes" << std::endl;
}

// Invoked with a current GL context by the existing native viewport smoke.
inline void CheckFixtureLabelInkBalance() {
  viewer3d::render::OpenGLStateGuard state;
  GLfloat clear[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
  glcapture::FramebufferCaptureTarget target;
  if (!target.Initialize(1024,256)) throw std::runtime_error("Glyph probe framebuffer failed");
  std::unique_ptr<NVGcontext,decltype(&nvgDeleteGL2)> vg(
      nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES),nvgDeleteGL2);
  if (!vg) throw std::runtime_error("Glyph probe NanoVG context failed");
  const typography::FontPair fonts(ProjectUtils::GetResourceRoot());
  const auto path = typography::BundledFontPath(fonts.resourceRoot,false).string();
  const int font = nvgCreateFont(vg.get(),"perastage-sans",path.c_str());
  if (font < 0) throw std::runtime_error("Glyph probe bundled font failed");
  typography::TextStyle style; style.size = .48; style.alignment = 0;
  const auto layout = typography::Compose(viewer2d::WrapFixtureLabelName("LITE BEYE 10R4"),style,fonts.regular);
  std::vector<TextCommand> glyphs;
  for (const auto &g : layout.glyphs) {
    TextCommand t; t.text=g.text; t.x=g.x; t.y=g.baseline;
    t.style.fontFamily="perastage-sans"; t.style.fontSize=style.size;
    t.style.glyphOutline=typography::ReadGlyphOutline(fonts.regular,
        static_cast<unsigned char>(typography::EncodeWinAnsi(g.text)[0]));
    t.style.color={1,1,1,1}; t.style.outlineColor={0,0,0,1};
    t.style.outlineWidth=typography::kFixtureLabelOutlineReferenceMetres;
    glyphs.push_back(t);
  }
  for (double zoom : {.25,.5,1.,2.,4.}) for (bool light : {false,true}) {
    for (auto &t : glyphs) {
      t.style.color=light?CanvasColor{0,0,0,1}:CanvasColor{1,1,1,1};
      t.style.outlineColor=light?CanvasColor{1,1,1,1}:CanvasColor{0,0,0,1};
    }
    std::array<double,2> reference{};
    for (double dpi : {1.,1.5,2.}) for (bool modern : {false,true}) {
      // Legacy uses framebuffer coordinates; preview uses logical coordinates.
      const double ratio=modern?dpi:1, scale=25*zoom/ratio;
      target.BindForRendering(); glViewport(0,0,1024,256); glDisable(GL_SCISSOR_TEST);
      glClearColor(0,0,0,0);
      glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      nvgBeginFrame(vg.get(),1024/ratio,256/ratio,ratio);
      if (modern) {
        for (auto pass : {typography::TextInkPass::Outline,typography::TextInkPass::Fill})
          for (const auto &t : glyphs)
            viewer2d::DrawCanvasText(vg.get(),t,20/ratio+t.x*scale,100/ratio-t.y*scale,scale,pass,ratio);
      } else {
        nvgFontFaceId(vg.get(),font); nvgFontSize(vg.get(),12*zoom);
        nvgTextAlign(vg.get(),NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
        for (size_t i=0; i<layout.lines.size(); ++i) {
          const char *line=i==0?"LITE BEYE":"10R4";
          const double y=100-layout.lines[i].baseline*scale;
          nvgFillColor(vg.get(),light?nvgRGB(255,255,255):nvgRGB(0,0,0));
          for (const auto &o : typography::OutlineOffsets(1))
            nvgText(vg.get(),20+o[0],y+o[1],line,nullptr);
          nvgFillColor(vg.get(),light?nvgRGB(0,0,0):nvgRGB(255,255,255)); nvgText(vg.get(),20,y,line,nullptr);
        }
      }
      nvgEndFrame(vg.get()); target.BindForReading();
      std::vector<unsigned char> pixels(1024*256*4);
      glReadPixels(0,0,1024,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
      double fill=0, coverage=0;
      for (size_t i=0; i<pixels.size(); i+=4) {
        fill+=(light?static_cast<int>(pixels[i+3])-pixels[i]:pixels[i])/255.;
        coverage+=pixels[i+3]/255.;
      }
      if (!modern) reference={fill,coverage};
      else if (coverage<=0 || std::abs(fill/reference[0]-1)>.06 ||
               std::abs(fill/coverage-reference[0]/reference[1])>.08)
        throw std::runtime_error("Edited label fill/halo balance differs from legacy");
    }
  }
  glClearColor(clear[0],clear[1],clear[2],clear[3]);
  std::cout << "Legacy/edited glyph balance passed: 5 zooms, 3 pixel ratios, light/dark, wrapped name" << std::endl;
}
