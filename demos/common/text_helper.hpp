#pragma once

#include <vector>
#include <string>
#include <iostream>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/text/freetype_font.hpp>

#include "cast_c_array.hpp"

inline
std::ostream&
operator<<(std::ostream &str, const fastuidraw::FontProperties &obj)
{
  str << obj.source_label() << ":(" << obj.foundry()
      << ", " << obj.family() << ", " << obj.style()
      << ", " << obj.italic() << ", " << obj.bold() << ")";
  return str;
}

void
create_formatted_text(std::istream &stream, fastuidraw::GlyphRender renderer,
                      float pixel_size,
                      fastuidraw::FontBase::const_handle font,
                      fastuidraw::GlyphSelector::handle glyph_selector,
                      std::vector<fastuidraw::Glyph> &glyphs,
                      std::vector<fastuidraw::vec2> &positions,
                      std::vector<uint32_t> &character_codes);

void
add_fonts_from_path(const std::string &path, fastuidraw::FreetypeLib::handle lib,
                    fastuidraw::GlyphSelector::handle glyph_selector,
                    fastuidraw::FontFreeType::RenderParams render_params);
