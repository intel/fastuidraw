#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <SDL_thread.h>
#include <SDL_atomic.h>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/text/font_database.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/painter/glyph_sequence.hpp>

#include "cast_c_array.hpp"

inline
std::ostream&
operator<<(std::ostream &str, const fastuidraw::FontProperties &obj)
{
  str << obj.source_label() << "(foundry = " << obj.foundry()
      << ", family = " << obj.family() << ", style = " << obj.style()
      << ", bold = " << obj.bold() << ", italic = " << obj.italic()
      << ")";
  return str;
}

class GlyphSetGenerator
{
public:
  static
  void
  generate(unsigned int num_threads,
           fastuidraw::GlyphRenderer r,
           fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> f,
           std::vector<fastuidraw::Glyph> &dst,
           fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> glyph_cache,
           std::vector<int> &cnts);

private:
  GlyphSetGenerator(fastuidraw::GlyphRenderer r,
                    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> f,
                    std::vector<fastuidraw::Glyph> &dst);

  static
  int
  execute(void *ptr);

  fastuidraw::GlyphRenderer m_render;
  fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> m_font;
  fastuidraw::c_array<fastuidraw::Glyph> m_dst;
  SDL_atomic_t m_counter;
};

/*
 * \param[out] out_sequence sequence to which to add glyphs
 * \param glyph_codes sequence of glyph codes (not characater codes!)
 * \param font font of the glyphs
 * \param shift_by amount by which to shit all glyphs
 */
void
create_formatted_text(fastuidraw::GlyphSequence &out_sequence,
                      const std::vector<uint32_t> &glyph_codes,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      const fastuidraw::vec2 &shift_by = fastuidraw::vec2(0.0f, 0.0f));


/*
 * \param[out] out_sequence sequence to which to add glyphs
 * \param stream input stream from which to grab lines of text
 * \param font font of the glyphs
 * \param font_database used to select glyphs from font
 * \param shift_by amount by which to shit all glyphs
 */
void
create_formatted_text(fastuidraw::GlyphSequence &out_sequence,
                      std::istream &stream,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database,
                      const fastuidraw::vec2 &shift_by = fastuidraw::vec2(0.0f, 0.0f));

/*
 * \param[out] out_run run to which to add glyphs
 * \param stream input stream from which to grab lines of text
 * \param font font of the glyphs
 * \param font_database used to select glyphs from font
 * \param shift_by amount by which to shit all glyphs
 */
void
create_formatted_text(fastuidraw::GlyphRun &out_run,
                      std::istream &stream,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database,
                      const fastuidraw::vec2 &shift_by = fastuidraw::vec2(0.0f, 0.0f));

void
add_fonts_from_path(const std::string &path,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                    fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database);

fastuidraw::c_string
default_font(void);

fastuidraw::c_string
default_font_path(void);
