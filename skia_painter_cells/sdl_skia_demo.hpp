
#include "GrContext.h"
#include "SkCanvas.h"
#include "SkImage.h"
#include "SkRSXform.h"
#include "SkSurface.h"
#include "SkPaint.h"

#include "sdl_demo.hpp"

class sdl_skia_demo:public sdl_demo
{
public:
  explicit
  sdl_skia_demo(const std::string &about_text=std::string());

  ~sdl_skia_demo();

protected:
  void
  init_gl(int, int);

  virtual
  void
  derived_init(int, int)
  {}

  void
  on_resize(int, int);

  SkCanvas*
  skia_canvas(void);

private:
  command_separator m_demo_options;

  void
  init_skia(int w, int h);

  sk_sp<GrContext> m_skia_context;
  sk_sp<SkSurface> m_skia_surface;
};
