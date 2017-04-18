#include <string>
#include <algorithm>
#include <dirent.h>
#include "text_helper.hpp"

namespace
{
  void
  preprocess_text(std::string &text)
  {
    /* we want to change '\t' into 4 spaces
     */
    std::string v;
    v.reserve(text.size() + 4 * std::count(text.begin(), text.end(), '\t'));
    for(std::string::const_iterator iter = text.begin(); iter != text.end(); ++iter)
      {
        if(*iter != '\t')
          {
            v.push_back(*iter);
          }
        else
          {
            v.push_back(' ');
          }
      }
    text.swap(v);
  }

  void
  add_fonts_from_file(const std::string &filename,
                      fastuidraw::reference_counted_ptr<fastuidraw::FreetypeLib> lib,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      fastuidraw::FontFreeType::RenderParams render_params)
  {
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::FontFreeType> > h;
    unsigned int cnt;

    cnt = fastuidraw::FontFreeType::create(cast_c_array(h), filename.c_str(), lib, render_params);
    h.resize(cnt);
    cnt = fastuidraw::FontFreeType::create(cast_c_array(h), filename.c_str(), lib, render_params);

    FASTUIDRAWassert(cnt == h.size());

    for(unsigned int i = 0, endi = h.size(); i < endi; ++i)
      {
        glyph_selector->add_font(h[i]);
      }
  }

}

void
create_formatted_text(std::istream &istr, fastuidraw::GlyphRender renderer,
                      float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      std::vector<fastuidraw::Glyph> &glyphs,
                      std::vector<fastuidraw::vec2> &positions,
                      std::vector<uint32_t> &character_codes)
{
  std::streampos current_position, end_position;
  unsigned int loc(0);
  fastuidraw::vec2 pen(0.0f, 0.0f);
  std::string line, original_line;
  float last_negative_tallest(0.0f);

  current_position = istr.tellg();
  istr.seekg(0, std::ios::end);
  end_position = istr.tellg();
  istr.seekg(current_position, std::ios::beg);

  glyphs.resize(end_position - current_position);
  positions.resize(end_position - current_position);
  character_codes.resize(end_position - current_position);

  fastuidraw::c_array<fastuidraw::Glyph> glyphs_ptr(cast_c_array(glyphs));
  fastuidraw::c_array<fastuidraw::vec2> pos_ptr(cast_c_array(positions));
  fastuidraw::c_array<uint32_t> char_codes_ptr(cast_c_array(character_codes));

  while(getline(istr, line))
    {
      fastuidraw::c_array<fastuidraw::Glyph> sub_g;
      fastuidraw::c_array<fastuidraw::vec2> sub_p;
      fastuidraw::c_array<uint32_t> sub_ch;
      float tallest, negative_tallest, offset;
      bool empty_line;

      empty_line = true;
      tallest = 0.0f;
      negative_tallest = 0.0f;

      original_line = line;
      preprocess_text(line);

      sub_g = glyphs_ptr.sub_array(loc, line.length());
      sub_p = pos_ptr.sub_array(loc, line.length());
      sub_ch = char_codes_ptr.sub_array(loc, line.length());

      glyph_selector->create_glyph_sequence(renderer, font, line.begin(), line.end(), sub_g.begin());
      for(unsigned int i = 0, endi = sub_g.size(); i < endi; ++i)
        {
          fastuidraw::Glyph g;

          g = sub_g[i];
          sub_p[i] = pen;
          sub_ch[i] = static_cast<uint32_t>(line[i]);
          if(g.valid())
            {
              float ratio;
              ratio = pixel_size / static_cast<float>(g.layout().m_pixel_size);

              empty_line = false;
              pen.x() += ratio * g.layout().m_advance.x();

              tallest = std::max(tallest, ratio * (g.layout().m_horizontal_layout_offset.y() + g.layout().m_size.y()));
              negative_tallest = std::min(negative_tallest, ratio * g.layout().m_horizontal_layout_offset.y());
            }
        }

      if(empty_line)
        {
          offset = pixel_size + 1.0f;
        }
      else
        {
          offset = (tallest - last_negative_tallest);
        }
      for(unsigned int i = 0; i < sub_p.size(); ++i)
        {
          sub_p[i].y() += offset;
        }
      pen.x() = 0.0f;
      pen.y() += offset + 1.0f;
      loc += line.length();
      last_negative_tallest = negative_tallest;
    }

  glyphs.resize(loc);
  positions.resize(loc);
  character_codes.resize(loc);
}

void
add_fonts_from_path(const std::string &filename,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreetypeLib> lib,
                    fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                    fastuidraw::FontFreeType::RenderParams render_params)
{
  DIR *dir;
  struct dirent *entry;

  dir = opendir(filename.c_str());
  if(!dir)
    {
      add_fonts_from_file(filename, lib, glyph_selector, render_params);
      return;
    }

  for(entry = readdir(dir); entry != nullptr; entry = readdir(dir))
    {
      std::string file;
      file = entry->d_name;
      if(file != ".." && file != ".")
        {
          add_fonts_from_path(filename + "/" + file, lib, glyph_selector, render_params);
        }
    }
  closedir(dir);
}
