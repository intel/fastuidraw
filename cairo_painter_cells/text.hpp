#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <istream>
#include <cairo.h>
#include <cairo-ft.h>
#include "vec2.hpp"

class glyph_data
{
public:
  uint32_t m_glyph_code;
  vec2 m_origin;
  vec2 m_size;
  vec2 m_advance;
};

class text_formatter
{
public:
  static
  text_formatter*
  create(const std::string &filename, int pixel_size);

  ~text_formatter(void);

  cairo_font_face_t*
  cairo_font(void)
  {
    return m_cairo_font;
  }

  int
  pixel_size(void)
  {
    return m_pixel_size;
  }

  void
  layout_glyphs(std::istream &text, double scale_factor, std::vector<cairo_glyph_t> &output);

  void
  layout_glyphs(const std::string &text, double scale_factor, std::vector<cairo_glyph_t> &output);

private:
  class ft_data
  {
  public:
    ft_data(FT_Library lib, FT_Face face);
    ~ft_data();

    FT_Library m_lib;
    FT_Face m_face;
  };

  text_formatter(FT_Library lib, FT_Face face, int pixel_size);

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
