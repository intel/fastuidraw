#include "sdl_skia_demo.hpp"
#include "GrBackendSurface.h"
#include "GrContext.h"

#include "gl/GrGLInterface.h"
#include <GL/glcorearb.h>

sdl_skia_demo::
sdl_skia_demo(const std::string &about_text):
  sdl_demo(about_text),
  m_demo_options("Demo Options", *this)
{}

sdl_skia_demo::
~sdl_skia_demo()
{
  m_skia_surface.reset();
  m_skia_context.reset();
}

void
sdl_skia_demo::
init_gl(int w, int h)
{
  init_skia(w, h);
  derived_init(w, h);
}

void
sdl_skia_demo::
init_skia(int w, int h)
{
  assert(m_skia_context == NULL);
  assert(m_skia_surface.get() == NULL);

  auto interface = GrGLMakeNativeInterface();
  GrGLint buffer;
  GrGLFramebufferInfo info;
  SkColorType colorType;
  int MsaaSampleCount;

  m_skia_context = GrContext::MakeGL(interface);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
  info.fFBOID = (GrGLuint) buffer;
  info.fFormat = GL_RGBA8;
  colorType = kRGBA_8888_SkColorType;

  MsaaSampleCount = (m_use_msaa.m_value) ?
    m_msaa.m_value : 0;

  GrBackendRenderTarget target(w, h, MsaaSampleCount, m_stencil_bits.m_value, info);
  SkSurfaceProps props(SkSurfaceProps::kLegacyFontHost_InitType);

  m_skia_surface = SkSurface::MakeFromBackendRenderTarget(m_skia_context.get(), target,
                                                          kBottomLeft_GrSurfaceOrigin,
                                                          colorType, NULL, &props);
}

void
sdl_skia_demo::
on_resize(int w, int h)
{
  m_skia_surface.reset();
  m_skia_context.reset();
  init_skia(w, h);
}

SkCanvas*
sdl_skia_demo::
skia_canvas(void)
{
  assert(m_skia_surface != NULL);
  return m_skia_surface->getCanvas();
}
