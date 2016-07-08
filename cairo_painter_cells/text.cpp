#include <assert.h>
#include <algorithm>
#include <istream>

#include "text.hpp"

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
}


ft_cairo_font::ft_data::
ft_data(FT_Library lib, FT_Face face):
  m_lib(lib),
  m_face(face)
{
  assert(m_face);
  assert(m_lib);
}

ft_cairo_font::ft_data::
~ft_data()
{
  FT_Done_Face(m_face);
  FT_Done_FreeType(m_lib);
}

ft_cairo_font::
ft_cairo_font(FT_Library lib, FT_Face face, int pixel_size):
  m_pixel_size(pixel_size)
{
  static cairo_user_data_key_t key = { 0 };
  m_ft_data = new ft_data(lib, face);
  m_cairo_font = cairo_ft_font_face_create_for_ft_face(face, 0);
  cairo_font_face_set_user_data(m_cairo_font, &key, m_ft_data, &cleanup);
}

ft_cairo_font::
~ft_cairo_font()
{
  cairo_font_face_destroy(m_cairo_font);
}

void
ft_cairo_font::
cleanup(void *p)
{
  ft_data *d;
  d = reinterpret_cast<ft_data*>(p);
  delete d;
}

void
ft_cairo_font::
layout_glyphs(std::istream &istr, std::vector<cairo_glyph_t> &output)
{
  std::streampos current_position, end_position;
  unsigned int loc(0);
  vec2 pen(0.0f, 0.0f);
  std::string line, original_line;
  double last_negative_tallest(0.0);

  current_position = istr.tellg();
  istr.seekg(0, std::ios::end);
  end_position = istr.tellg();
  istr.seekg(current_position, std::ios::beg);

  output.resize(end_position - current_position);
  while(getline(istr, line))
    {
      double tallest, negative_tallest, offset;
      bool empty_line;
      cairo_glyph_t *sub_g;

      empty_line = true;
      tallest = 0.0;
      negative_tallest = 0.0;

      original_line = line;
      preprocess_text(line);

      sub_g = &output[loc];
      for(unsigned int i = 0, endi = line.length(); i < endi; ++i)
        {
          sub_g[i].index = fetch_glyph(line[i]);
        }

      for(unsigned int i = 0, endi = line.length(); i < endi; ++i)
        {
          sub_g[i].x = pen.x();
          sub_g[i].y = pen.y();

          const glyph_data &g(*m_glyph_data[sub_g[i].index]);
          empty_line = false;
          pen.x() += g.m_advance.x();
          tallest = std::max(tallest, (g.m_origin.y() + g.m_size.y()));
          negative_tallest = std::min(negative_tallest, g.m_origin.y());
        }

      if(empty_line)
        {
          offset = m_pixel_size + 1.0;
        }
      else
        {
          offset = (tallest - last_negative_tallest);
        }

      for(unsigned int i = 0, endi = line.length(); i < endi; ++i)
        {
          sub_g[i].y += offset;
        }

      pen.x() = 0.0;
      pen.y() += offset + 1.0;
      loc += line.length();
      last_negative_tallest = negative_tallest;
    }
  output.resize(loc);
}

uint32_t
ft_cairo_font::
fetch_glyph(uint32_t character_code)
{
  uint32_t return_value;

  return_value = FT_Get_Char_Index(m_ft_data->m_face, FT_ULong(character_code));
  if(m_glyph_data.size() <= return_value)
    {
      m_glyph_data.resize(return_value + 1, NULL);
    }

  if(!m_glyph_data[return_value])
    {
      m_glyph_data[return_value] = new glyph_data();

      ivec2 bitmap_sz, bitmap_offset, iadvance;
      glyph_data &output(*m_glyph_data[return_value]);

      FT_Set_Pixel_Sizes(m_ft_data->m_face, m_pixel_size, m_pixel_size);
      FT_Set_Transform(m_ft_data->m_face, NULL, NULL);
      FT_Load_Glyph(m_ft_data->m_face, return_value, FT_LOAD_DEFAULT);
      FT_Render_Glyph(m_ft_data->m_face->glyph, FT_RENDER_MODE_NORMAL);

      bitmap_sz.x() = m_ft_data->m_face->glyph->bitmap.width;
      bitmap_sz.y() = m_ft_data->m_face->glyph->bitmap.rows;

      bitmap_offset.x() = m_ft_data->m_face->glyph->bitmap_left;
      bitmap_offset.y() = m_ft_data->m_face->glyph->bitmap_top - m_ft_data->m_face->glyph->bitmap.rows;

      iadvance.x() = m_ft_data->m_face->glyph->advance.x;
      iadvance.y() = m_ft_data->m_face->glyph->advance.y;

      output.m_glyph_code = return_value;
      output.m_origin = vec2(bitmap_offset);
      output.m_advance = vec2(iadvance) / 64.0f;
      output.m_size = vec2(bitmap_sz);
      output.m_texel_size = vec2(bitmap_sz);
    }

  return return_value;
}

ft_cairo_font*
ft_cairo_font::
create_font(const std::string &filename, int pixel_size)
{
  int error_code;
  FT_Face face;
  FT_Library lib;

  error_code = FT_Init_FreeType(&lib);
  if(error_code != 0)
    {
      if(lib)
        {
          FT_Done_FreeType(lib);
        }
      return NULL;
    }

  error_code = FT_New_Face(lib, filename.c_str(), 0, &face);
  if(error_code != 0)
    {
      if(face != NULL)
        {
          FT_Done_Face(face);
        }
      FT_Done_FreeType(lib);
      return NULL;
    }
  return new ft_cairo_font(lib, face, pixel_size);
}
