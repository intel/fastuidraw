#include "sdl_skia_demo.hpp"

sdl_skia_demo::
sdl_skia_demo(const std::string &about_text):
  sdl_demo(about_text),
  m_demo_options("Demo Options", *this),
  m_skia_context(NULL)
{}

sdl_skia_demo::
~sdl_skia_demo()
{
  m_skia_surface.reset();
  delete m_skia_context;
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

  m_skia_context = GrContext::Create(kOpenGL_GrBackend, 0);

  GrBackendRenderTargetDesc desc;
  desc.fWidth = w;
  desc.fHeight = h;
  desc.fConfig = kSkia8888_GrPixelConfig;
  desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
  desc.fSampleCnt = sample_count();
  desc.fStencilBits = stencil_bits();
  desc.fRenderTargetHandle = 0;  // assume default framebuffer
  m_skia_surface = SkSurface::MakeFromBackendRenderTarget(m_skia_context, desc, NULL);
}

void
sdl_skia_demo::
on_resize(int w, int h)
{
  m_skia_surface.reset();
  delete m_skia_context;
  m_skia_context = NULL;
  init_skia(w, h);
}

SkCanvas*
sdl_skia_demo::
skia_canvas(void)
{
  assert(m_skia_surface != NULL);
  return m_skia_surface->getCanvas();
}
