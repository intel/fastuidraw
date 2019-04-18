/*!
 * \file example_text.cpp
 * \brief file example_text.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */

//! [ExampleText]

#include <iostream>
#include <fstream>
#include <sstream>
#include <fastuidraw/painter/attribute_data/glyph_sequence.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include "initialization.hpp"

class example_text:public initialization
{
public:
  example_text(void):
    m_translate(0.0f, 0.0f),
    m_scale(1.0f)
  {}

  ~example_text();

protected:
  virtual
  void
  draw_frame(void) override;

  virtual
  void
  derived_init(int argc, char **argv) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  fastuidraw::reference_counted_ptr<fastuidraw::FontBase> m_font;
  fastuidraw::GlyphSequence *m_glyph_sequence;
  fastuidraw::vec2 m_translate;
  float m_scale;
};

void
example_text::
derived_init(int argc, char **argv)
{
  /* call the base-clas derived_init() to create the
   * fastuidraw::Painter, fastuidraw::PainterEngineGL
   * and fastuidraw::PainterSurface objects
   */
  initialization::derived_init(argc, argv);

  /* First we need a font object from which to grab glyphs.
   * The class fastuidraw::FreeTypeFace::GeneratorBase
   * provides an interface to read fonts from memory or
   * a file.
   */
  fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase> font_generator;

  /* Using a GeneratorMemory loads the contents of the file into memory which
   * is then passed to Freetype for the FastUIDraw font to extract data. There
   * is also the class fastuidraw::FreeTypeFace::GeneratorFile that has the
   * font directly extracted from the file at the cost that the file remains
   * open (indeed FontFreeType will have multiple files open so that it can
   * parallize glyph creation) for the lifetime of the font.
   */
  font_generator = FASTUIDRAWnew fastuidraw::FreeTypeFace::GeneratorMemory(argv[1], 0);

  /* Check that Freetype can extract a scalable font from the data,
   * and if not, abort.
   */
  if (font_generator->check_creation() == fastuidraw::routine_fail)
    {
      std::cerr << "Unable to extract scalable font from \""
                << argv[1] << "\"\n";
      end_demo(-1);
      return;
    }

  /* We can now generate the font with font_generator */
  m_font = FASTUIDRAWnew fastuidraw::FontFreeType(font_generator);

  std::istream *text_src(nullptr);
  std::istringstream default_text("Hello World");
  std::ifstream text_file;

  if (argc >= 3)
    {
      text_file.open(argv[2]);
      if (text_file)
        {
          text_src = &text_file;
        }
    }

  if (!text_src)
    {
      text_src = &default_text;
    }

  /* The ctor to fastuidraw::GlyphSequence requires knowing the
   * GlyphCache which we fetch from intialize::m_painter_engine_gl.
   * The class fastuidraw::GlyphSequence does not have a reference
   * count, so we need to create the object dynamically and use
   * a regular pointer to it. The value of format_size is the size
   * at which one will format the glyphs added to the
   * fastuidraw::GlyphSequence
   */
  int format_size(32.0f);
  m_glyph_sequence = FASTUIDRAWnew fastuidraw::GlyphSequence(format_size,
                                                             fastuidraw::PainterEnums::y_increases_downwards,
                                                             m_painter_engine_gl->glyph_cache());

  /* now add some glyphs to the fastuidraw::GlyphSequence.
   * A real application would use HarfBuzz or another library
   * to layout and shape the text nicely. Below we use simple
   * code to layout the text left-to-right and advancing the
   * pen vertically on line-breaks.
   *
   * The fastuidraw::GlyphCache can be used to fetch the
   * fastuidraw::GlyphMetrics of a glyph from a file which
   * we use to perform our rudimentary formatting. The values
   * of fastuidraw::GlyphMetrics is ALWAYS in font coordinates,
   * one can convert from font coordinates to formatting
   * coordinates using fastuidraw::GlyphMetrics::units_per_EM()
   */
  float ratio(m_glyph_sequence->format_size() / m_font->metrics().units_per_EM());
  fastuidraw::vec2 pen(0.0f, 0.0f);
  char ch;

  /* The pen represents the base-line to where to draw the text.
   * Because we chose the orientation fastuidraw::PainterEnums::y_increases_downwards,
   * a value of y = 0 means the top of the screen. Thus we increase the
   * y-coordinte by the height of the font.
   */
  pen.y() += ratio * m_font->metrics().height();


  /* The below is a terribly naive text formatter (but good enough for demos).
   * It assumes 8-bit text file and it fetches a single glyph at a time
   * from the glyph cache instead of fetch many glyphs per GlyphCache
   * call.
   */
  while (text_src->get(ch))
    {
      if (ch == '\n')
        {
          pen.y() += ratio * m_font->metrics().height();
          pen.x() = 0.0f;
        }
      else
        {
          fastuidraw::GlyphMetrics glyph_metrics;
          uint32_t glyph_code;

          /* Fetch the glyph_code from the font, a return value of 0
           * is the special glyph code to represent that the character
           * code is not present and to instead draw a glyph indicating
           * that.
           */
          glyph_code = m_font->glyph_code(ch);

          /* Fetch the GlyphMetrics value for the glyph that tells how
           * to format the glyph. Note, it is better to fetch multiple
           * metrics instead of one at a time because each call to
           * GlyphCache::fetch_glyph_metrics() locks and unlocks a mutex.
           * The function GlyphCache::fetch_glyph_metrics() has an override
           * taking array of glyph-code values.
           */
          glyph_metrics = m_glyph_sequence->glyph_cache()->fetch_glyph_metrics(m_font.get(), glyph_code);

          /* Check that the glyph_metrics value is valid, and if so, add the
           * glyph to m_glyph_sequence and advance the pen
           */
          if (glyph_metrics.valid())
            {
              m_glyph_sequence->add_glyph(fastuidraw::GlyphSource(m_font.get(), glyph_code), pen);
              pen.x() += ratio * glyph_metrics.advance().x();
            }
        }
    }
}

void
example_text::
draw_frame(void)
{
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());

  m_surface_gl->viewport(vwp);
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  /* translate and scale as according to m_translate and m_scale */
  m_painter->translate(m_translate);
  m_painter->scale(m_scale);

  /* Draw the text of m_glyph_sequence; fastuidraw::Painter will
   * auto-select how to render the glyph based off of the size
   * at which it will appear on the screen.
   */
  fastuidraw::PainterBrush brush;
  brush.color(1.0f, 1.0f, 1.0f, 1.0f);
  m_painter->draw_glyphs(&brush, *m_glyph_sequence);

  m_painter->end();

  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface_gl->blit_surface(GL_NEAREST);
}

void
example_text::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      switch (ev.key.keysym.sym)
        {
        case SDLK_UP:
          m_translate.y() += 16.0f;
          break;

        case SDLK_DOWN:
          m_translate.y() -= 16.0f;
          break;

        case SDLK_LEFT:
          m_translate.x() += 16.0f;
          break;

        case SDLK_RIGHT:
          m_translate.x() -= 16.0f;
          break;

        case SDLK_PAGEUP:
          m_scale += 0.2f;
          break;

        case SDLK_PAGEDOWN:
          m_scale -= 0.2f;
          break;

        case SDLK_SPACE:
          m_scale = 1.0f;
          m_translate.x() = 0.0f;
          m_translate.y() = 0.0f;
          break;
        }
      break;
    }
  initialization::handle_event(ev);
}

example_text::
~example_text()
{
  /* Because we created m_glyph_sequence is NOT a reference counted
   * object that we created with FASTUIDRAWnew, we need to delete it
   * ourselves with FASTUIDRAWdelete().
   */
  FASTUIDRAWdelete(m_glyph_sequence);
}

int
main(int argc, char **argv)
{
  if (argc < 2)
    {
      std::cout << "Usage: " << argv[0] << " font_file [text_file]\n";
      return -1;
    }

  example_text demo;
  return demo.main(argc, argv);
}

//! [ExampleText]
