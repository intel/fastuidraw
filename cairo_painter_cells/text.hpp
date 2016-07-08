#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <cairo.h>
#include <cairo-ft.h>
#include "vec2.hpp"

class glyph_data
{
public:
  uint32_t m_glyph_code;
  vec2 m_origin;
  vec2 m_size;
  vec2 m_texel_size;
  vec2 m_advance;
};

class ft_cairo_font
{
public:
  static
  ft_cairo_font*
  create_font(const std::string &filename, int pixel_size);

  ~ft_cairo_font(void);

  void
  layout_glyphs(std::istream &text, std::vector<cairo_glyph_t> &output);

private:
  class ft_data
  {
  public:
    ft_data(FT_Library lib, FT_Face face);
    ~ft_data();

    FT_Library m_lib;
    FT_Face m_face;
  };

  ft_cairo_font(FT_Library lib, FT_Face face, int pixel_size);

  static
  void
  cleanup(void*);

  uint32_t
  fetch_glyph(uint32_t character_code);

  int m_pixel_size;
  ft_data *m_ft_data;
  cairo_font_face_t *m_cairo_font;
  std::vector<glyph_data*> m_glyph_data;
};
