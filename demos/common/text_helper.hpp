#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <SDL_thread.h>
#include <SDL_atomic.h>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/text/glyph_sequence.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
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

/*
 * \param glyph_codes sequence of glyph codes (not characater codes!)
 * \param pixel_size pixel size to show glyphs at
 * \param font font of the glyphs
 * \param[out] out_sequence sequence to which to add glyphs
 * \param[out] line_data bounding boxes of lines
 * \param[out] glyph_extents for each glyph, its extent on the x-axis
 * \param orientation y-coordinate convention for drawing glyphs
 * \param adjust_starting_baseline if true, 0.0 is the baseline of the -previous-
 *                                 line, i.e. the text starts on the line after 0.0;
 *                                 if false the baseline of the test is a y = 0.0.
 */
void
create_formatted_text(const std::vector<uint32_t> &glyph_codes,
                      float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::GlyphSequence &out_sequence,
                      std::vector<LineData> *line_data = nullptr,
                      std::vector<fastuidraw::range_type<float> > *glyph_extents = nullptr,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation
                      = fastuidraw::PainterEnums::y_increases_downwards,
                      bool adjust_starting_baseline = true);


/*
 * \param stream input stream from which to grab lines of text
 * \param pixel_size pixel size to show glyphs at
 * \param font font of the glyphs
 * \param[out] out_sequence sequence to which to add glyphs
 * \param[out] character_codes character codes of input stream
 * \param[out] line_data bounding boxes of lines
 * \param[out] glyph_extents for each glyph, its extent on the x-axis
 * \param orientation y-coordinate convention for drawing glyphs
 * \param adjust_starting_baseline if true, 0.0 is the baseline of the -previous-
 *                                 line, i.e. the text starts on the line after 0.0;
 *                                 if false the baseline of the test is a y = 0.0.
 */
void
create_formatted_text(std::istream &stream, float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      fastuidraw::GlyphSequence &out_sequence,
                      std::vector<uint32_t> *character_codes = nullptr,
                      std::vector<LineData> *line_data = nullptr,
                      std::vector<fastuidraw::range_type<float> > *glyph_extents = nullptr,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation
                      = fastuidraw::PainterEnums::y_increases_downwards,
                      bool adjust_starting_baseline = true,
                      const fastuidraw::vec2 &starting_place = fastuidraw::vec2(0.0f, 0.0f));


/*
 * \param glyph_codes sequence of glyph codes (not characater codes!)
 * \param renderer how to render glyphs
 * \param pixel_size pixel size to show glyphs at
 * \param font font of the glyphs
 * \param glyph_cache cache of glyphs from which to fetch glyphs
 * \param[out] glyphs glyphs of glyph codes
 * \param[out] positions positions of glyphs
 * \param[out] character_codes character codes of input stream
 * \param[out] line_data bounding boxes of lines
 * \param[out] glyph_extents for each glyph, its extent on the x-axis
 * \param orientation y-coordinate convention for drawing glyphs
 * \param adjust_starting_baseline if true, 0.0 is the baseline of the -previous-
 *                                 line, i.e. the text starts on the line after 0.0;
 *                                 if false the baseline of the test is a y = 0.0.
 */
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
                      = fastuidraw::PainterEnums::y_increases_downwards,
                      bool adjust_starting_baseline = true);


/*
 * \param stream input stream from which to grab lines of text
 * \param renderer how to render glyphs
 * \param pixel_size pixel size to show glyphs at
 * \param font font of the glyphs
 * \param glyph_cache cache of glyphs from which to fetch glyphs
 * \param[out] glyphs glyphs of glyph codes
 * \param[out] positions positions of glyphs
 * \param[out] line_data bounding boxes of lines
 * \param[out] glyph_extents for each glyph, its extent on the x-axis
 * \param orientation y-coordinate convention for drawing glyphs
 * \param adjust_starting_baseline if true, 0.0 is the baseline of the -previous-
 *                                 line, i.e. the text starts on the line after 0.0;
 *                                 if false the baseline of the test is a y = 0.0.
 */
void
create_formatted_text(std::istream &stream, fastuidraw::GlyphRender renderer,
                      float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> glyph_cache,
                      std::vector<fastuidraw::Glyph> &glyphs,
                      std::vector<fastuidraw::vec2> &positions,
                      std::vector<uint32_t> &character_codes,
                      std::vector<LineData> *line_data = nullptr,
                      std::vector<fastuidraw::range_type<float> > *glyph_extents = nullptr,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation
                      = fastuidraw::PainterEnums::y_increases_downwards,
                      bool adjust_starting_baseline = true);

void
add_fonts_from_path(const std::string &path,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                    fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                    fastuidraw::FontFreeType::RenderParams render_params);

fastuidraw::c_string
default_font(void);

fastuidraw::c_string
default_font_path(void);
