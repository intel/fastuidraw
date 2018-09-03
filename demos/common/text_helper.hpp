#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <SDL_thread.h>
#include <SDL_atomic.h>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

#include "cast_c_array.hpp"

inline
std::ostream&
operator<<(std::ostream &str, const fastuidraw::FontProperties &obj)
{
  str << obj.source_label() << "(" << obj.foundry()
      << ", " << obj.family() << ", " << obj.style()
      << ", " << obj.italic() << ", " << obj.bold() << ")";
  return str;
}

class LineData
{
public:
  /* range into glyphs/glyph_position where
     line is located
   */
  fastuidraw::range_type<unsigned int> m_range;

  /* line's vertical spread
   */
  fastuidraw::range_type<float> m_vertical_spread;

  /* line's horizontal spread
   */
  fastuidraw::range_type<float> m_horizontal_spread;
};

class GlyphSetGenerator
{
public:
  static
  void
  generate(unsigned int num_threads,
           fastuidraw::GlyphRender r,
           fastuidraw::reference_counted_ptr<const fastuidraw::FontFreeType> f,
           fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace> face,
           std::vector<fastuidraw::Glyph> &dst,
           fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> glyph_cache,
           std::vector<int> &cnts);

private:
  GlyphSetGenerator(fastuidraw::GlyphRender r,
                    fastuidraw::reference_counted_ptr<const fastuidraw::FontFreeType> f,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace> face,
                    std::vector<fastuidraw::Glyph> &dst);

  static
  int
  execute(void *ptr);

  fastuidraw::GlyphRender m_render;
  fastuidraw::reference_counted_ptr<const fastuidraw::FontFreeType> m_font;
  fastuidraw::c_array<fastuidraw::Glyph> m_dst;
  SDL_atomic_t m_counter;
};

void
create_formatted_text(const std::vector<uint32_t> &glyph_codes,
                      fastuidraw::GlyphRender renderer, float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> glyph_cache,
                      std::vector<fastuidraw::Glyph> &glyphs,
                      std::vector<fastuidraw::vec2> &positions,
                      std::vector<LineData> *line_data = nullptr,
                      std::vector<fastuidraw::range_type<float> > *glyph_extents = nullptr,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation
                      = fastuidraw::PainterEnums::y_increases_downwards);

void
create_formatted_text(std::istream &stream, fastuidraw::GlyphRender renderer,
                      float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      std::vector<fastuidraw::Glyph> &glyphs,
                      std::vector<fastuidraw::vec2> &positions,
                      std::vector<uint32_t> &character_codes,
                      std::vector<LineData> *line_data = nullptr,
                      std::vector<fastuidraw::range_type<float> > *glyph_extents = nullptr,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation
                      = fastuidraw::PainterEnums::y_increases_downwards);

void
add_fonts_from_path(const std::string &path,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                    fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                    fastuidraw::FontFreeType::RenderParams render_params);

fastuidraw::c_string
default_font(void);

fastuidraw::c_string
default_font_path(void);
