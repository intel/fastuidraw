/*!
 * \file freetype_font.cpp
 * \brief file freetype_font.cpp
 *
 * Copyright 2016 by Intel.
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
 *
 */



#include <boost/thread.hpp>

#include <fastuidraw/text/freetype_font.hpp>
#include <fastuidraw/text/glyph_layout.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/text/glyph_render_data_curve_pair.hpp>
#include <fastuidraw/text/glyph_render_data_distance_field.hpp>
#include <fastuidraw/text/glyph_render_data_coverage.hpp>

#include "private/freetype_util.hpp"
#include "private/freetype_curvepair_util.hpp"
#include "../private/util_private.hpp"

namespace
{
  float
  to_pixel_sizes(FT_Pos p)
  {
    return static_cast<float>(p) / static_cast<float>(1<<6);
  }

  inline
  uint8_t
  pixel_value_from_distance(float dist, bool outside)
  {
    uint8_t v;

    if(outside)
      {
        dist = -dist;
      }

    dist = (dist + 1.0f) * 0.5f;
    v = static_cast<uint8_t>(255.0f * dist);
    return v;
  }

  class RenderParamsPrivate
  {
  public:
    RenderParamsPrivate(void):
      m_distance_field_pixel_size(48),
      m_distance_field_max_distance(96.0f),
      m_curve_pair_pixel_size(32)
    {}

    unsigned int m_distance_field_pixel_size;
    float m_distance_field_max_distance;
    unsigned int m_curve_pair_pixel_size;
  };

  class FontFreeTypePrivate
  {
  public:
    FontFreeTypePrivate(fastuidraw::FontFreeType *p, FT_Face pface,
                        const fastuidraw::FontFreeType::RenderParams &render_params);

    FontFreeTypePrivate(fastuidraw::FontFreeType *p, FT_Face pface, fastuidraw::FreetypeLib::handle lib,
                        const fastuidraw::FontFreeType::RenderParams &render_params);

    ~FontFreeTypePrivate();

    void
    common_init(void);

    void
    common_compute_rendering_data(int pixel_size, FT_Int32 load_flags,
                                  fastuidraw::GlyphLayoutData &layout,
                                  uint32_t glyph_code);

    void
    compute_rendering_data(int pixel_size, uint32_t glyph_code,
                           fastuidraw::GlyphLayoutData &layout,
                           fastuidraw::GlyphRenderDataCoverage &output);

    void
    compute_rendering_data(uint32_t glyph_code,
                           fastuidraw::GlyphLayoutData &layout,
                           fastuidraw::GlyphRenderDataDistanceField &output);

    void
    compute_rendering_data(uint32_t glyph_code,
                           fastuidraw::GlyphLayoutData &layout,
                           fastuidraw::GlyphRenderDataCurvePair &output);

    boost::mutex m_mutex;
    FT_Face m_face;
    fastuidraw::FontFreeType::RenderParams m_render_params;
    fastuidraw::FreetypeLib::handle m_lib;
    fastuidraw::FontFreeType *m_p;
  };
}

//////////////////////////////////////////////////
// FontFreeTypePrivate methods
FontFreeTypePrivate::
FontFreeTypePrivate(fastuidraw::FontFreeType *p, FT_Face pface,
                    const fastuidraw::FontFreeType::RenderParams &render_params):
  m_face(pface),
  m_render_params(render_params),
  m_p(p)
{
  common_init();
}

FontFreeTypePrivate::
FontFreeTypePrivate(fastuidraw::FontFreeType *p, FT_Face pface, fastuidraw::FreetypeLib::handle lib,
                    const fastuidraw::FontFreeType::RenderParams &render_params):
  m_face(pface),
  m_render_params(render_params),
  m_lib(lib),
  m_p(p)
{
  common_init();
}

FontFreeTypePrivate::
~FontFreeTypePrivate()
{
  if(m_lib)
    {
      FT_Done_Face(m_face);
    }
}

void
FontFreeTypePrivate::
common_init(void)
{
  assert(m_face != NULL);
  assert(m_face->face_flags & FT_FACE_FLAG_SCALABLE);
  FT_Set_Transform(m_face, NULL, NULL);
}

void
FontFreeTypePrivate::
common_compute_rendering_data(int pixel_size, FT_Int32 load_flags,
                              fastuidraw::GlyphLayoutData &output,
                              uint32_t glyph_code)
{
  fastuidraw::ivec2 bitmap_sz, bitmap_offset, iadvance;

  FT_Set_Pixel_Sizes(m_face, pixel_size, pixel_size);
  FT_Load_Glyph(m_face, glyph_code, load_flags);

  output.m_size.x() = to_pixel_sizes(m_face->glyph->metrics.width);
  output.m_size.y() = to_pixel_sizes(m_face->glyph->metrics.height);
  output.m_horizontal_layout_origin.x() = to_pixel_sizes(m_face->glyph->metrics.horiBearingX);
  output.m_horizontal_layout_origin.y() = to_pixel_sizes(m_face->glyph->metrics.horiBearingY) - output.m_size.y();
  output.m_vertical_layout_origin.x() = to_pixel_sizes(m_face->glyph->metrics.vertBearingX);
  output.m_vertical_layout_origin.y() = to_pixel_sizes(m_face->glyph->metrics.vertBearingY) - output.m_size.y();
  output.m_advance.x() = to_pixel_sizes(m_face->glyph->metrics.horiAdvance);
  output.m_advance.y() = to_pixel_sizes(m_face->glyph->metrics.vertAdvance);
  output.m_glyph_code = glyph_code;
  output.m_font = m_p;
}

void
FontFreeTypePrivate::
compute_rendering_data(int pixel_size, uint32_t glyph_code,
                       fastuidraw::GlyphLayoutData &layout,
                       fastuidraw::GlyphRenderDataCoverage &output)
{
  fastuidraw::ivec2 bitmap_sz;
  fastuidraw::autolock_mutex m(m_mutex);

  common_compute_rendering_data(pixel_size, FT_LOAD_DEFAULT, layout, glyph_code);
  FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);

  bitmap_sz.x() = m_face->glyph->bitmap.width;
  bitmap_sz.y() = m_face->glyph->bitmap.rows;
  layout.m_texel_size = fastuidraw::vec2(bitmap_sz);
  layout.m_pixel_size = pixel_size;

  /* add one pixel slack on glyph
   */
  if(bitmap_sz.x() != 0 && bitmap_sz.y() != 0)
    {
      int pitch;

      pitch = m_face->glyph->bitmap.pitch;
      output.resize(bitmap_sz + fastuidraw::ivec2(1, 1));
      std::fill(output.coverage_values().begin(), output.coverage_values().end(), 0);
      for(int y = 0; y < bitmap_sz.y(); ++y)
        {
          for(int x = 0; x < bitmap_sz.x(); ++x)
            {
              int write_location, read_location;

              write_location = x + y * output.resolution().x();
              read_location = x + (bitmap_sz.y() - 1 - y) * pitch;
              output.coverage_values()[write_location] = m_face->glyph->bitmap.buffer[read_location];
            }
        }
    }
  else
    {
      output.resize(fastuidraw::ivec2(0, 0));
    }
}

void
FontFreeTypePrivate::
compute_rendering_data(uint32_t glyph_code,
                       fastuidraw::GlyphLayoutData &layout,
                       fastuidraw::GlyphRenderDataDistanceField &output)
{
  int pixel_size(m_render_params.distance_field_pixel_size());
  float max_distance(m_render_params.distance_field_max_distance());
  fastuidraw::ivec2 bitmap_sz, bitmap_offset;

  std::vector<fastuidraw::detail::point_type> pts;
  std::ostream *stream_ptr(NULL);
  fastuidraw::detail::geometry_data dbg(stream_ptr, pts);

  m_mutex.lock();

    common_compute_rendering_data(pixel_size, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING, layout, glyph_code);
    FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);

    bitmap_sz.x() = m_face->glyph->bitmap.width;
    bitmap_sz.y() = m_face->glyph->bitmap.rows;
    bitmap_offset.x() = m_face->glyph->bitmap_left;
    bitmap_offset.y() = m_face->glyph->bitmap_top - m_face->glyph->bitmap.rows;
    layout.m_texel_size = fastuidraw::vec2(bitmap_sz);
    layout.m_pixel_size = pixel_size;

    fastuidraw::detail::OutlineData outline_data(m_face->glyph->outline, bitmap_sz, bitmap_offset, dbg);

  m_mutex.unlock();
  if(bitmap_sz.x() != 0 && bitmap_sz.y() != 0)
    {
      /* add one pixel slack on glyph
       */
      output.resize(bitmap_sz + fastuidraw::ivec2(1, 1));
      std::fill(output.distance_values().begin(), output.distance_values().end(), 0);
      boost::multi_array<fastuidraw::detail::distance_return_type, 2> distance_values(boost::extents[bitmap_sz.x()][bitmap_sz.y()]);

      outline_data.compute_distance_values(distance_values, max_distance, true);
      for(int y = 0; y < bitmap_sz.y(); ++y)
        {
          for(int x = 0; x < bitmap_sz.x(); ++x)
            {
              int location;
              bool outside;
              float v0;

              location = x + y * output.resolution().x();
              outside = (distance_values[x][y].m_solution_count.winding_number() == 0);

              v0 = distance_values[x][y].m_distance.value();
              v0 = std::min(v0 / max_distance, 1.0f);

              output.distance_values()[location] = pixel_value_from_distance(v0, outside);
            }
        }
    }
  else
    {
      output.resize(fastuidraw::ivec2(0, 0));
    }
}

void
FontFreeTypePrivate::
compute_rendering_data(uint32_t glyph_code,
                       fastuidraw::GlyphLayoutData &layout,
                       fastuidraw::GlyphRenderDataCurvePair &output)
{
  int pixel_size(m_render_params.curve_pair_pixel_size());
  fastuidraw::ivec2 bitmap_offset, bitmap_sz;

  m_mutex.lock();
    common_compute_rendering_data(pixel_size, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING, layout, glyph_code);
    FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);
    bitmap_sz.x() = m_face->glyph->bitmap.width;
    bitmap_sz.y() = m_face->glyph->bitmap.rows;
    bitmap_offset.x() = m_face->glyph->bitmap_left;
    bitmap_offset.y() = m_face->glyph->bitmap_top - m_face->glyph->bitmap.rows;
    layout.m_texel_size = fastuidraw::vec2(bitmap_sz);
    layout.m_pixel_size = pixel_size;
    fastuidraw::detail::CurvePairGenerator gen(m_face->glyph->outline, bitmap_sz, bitmap_offset, output);
  m_mutex.unlock();

  gen.extract_data(output);
}

/////////////////////////////////////////////
// fastuidraw::FontFreeType::RenderParams methods
fastuidraw::FontFreeType::RenderParams::
RenderParams(void)
{
  m_d = FASTUIDRAWnew RenderParamsPrivate();
}

fastuidraw::FontFreeType::RenderParams::
RenderParams(const RenderParams &obj)
{
  RenderParamsPrivate *obj_d;
  obj_d = reinterpret_cast<RenderParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew RenderParamsPrivate(*obj_d);
}

fastuidraw::FontFreeType::RenderParams::
~RenderParams(void)
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::FontFreeType::RenderParams&
fastuidraw::FontFreeType::RenderParams::
operator=(const RenderParams &rhs)
{
  RenderParamsPrivate *d, *rhs_d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  rhs_d = reinterpret_cast<RenderParamsPrivate*>(rhs.m_d);
  *d = *rhs_d;
  return *this;
}

fastuidraw::FontFreeType::RenderParams&
fastuidraw::FontFreeType::RenderParams::
distance_field_pixel_size(unsigned int v)
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  d->m_distance_field_pixel_size = v;
  return *this;
}

unsigned int
fastuidraw::FontFreeType::RenderParams::
distance_field_pixel_size(void) const
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  return d->m_distance_field_pixel_size;
}

fastuidraw::FontFreeType::RenderParams&
fastuidraw::FontFreeType::RenderParams::
distance_field_max_distance(float v)
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  d->m_distance_field_max_distance = v;
  return *this;
}

float
fastuidraw::FontFreeType::RenderParams::
distance_field_max_distance(void) const
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  return d->m_distance_field_max_distance;
}

fastuidraw::FontFreeType::RenderParams&
fastuidraw::FontFreeType::RenderParams::
curve_pair_pixel_size(unsigned int v)
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  d->m_curve_pair_pixel_size = v;
  return *this;
}

unsigned int
fastuidraw::FontFreeType::RenderParams::
curve_pair_pixel_size(void) const
{
  RenderParamsPrivate *d;
  d = reinterpret_cast<RenderParamsPrivate*>(m_d);
  return d->m_curve_pair_pixel_size;
}

///////////////////////////////////////////////////
// fastuidraw::FontFreeType methods
fastuidraw::FontFreeType::
FontFreeType(FT_Face pface, const FontProperties &props,
             const RenderParams &render_params):
  FontBase(props)
{
  m_d = FASTUIDRAWnew FontFreeTypePrivate(this, pface, render_params);
}

fastuidraw::FontFreeType::
FontFreeType(FT_Face pface, const RenderParams &render_params):
  FontBase(compute_font_propertes_from_face(pface))
{
  m_d = FASTUIDRAWnew FontFreeTypePrivate(this, pface, render_params);
}

fastuidraw::FontFreeType::
FontFreeType(FT_Face pface, FreetypeLib::handle lib, const FontProperties &props,
             const RenderParams &render_params):
  FontBase(props)
{
  m_d = FASTUIDRAWnew FontFreeTypePrivate(this, pface, lib, render_params);
}

fastuidraw::FontFreeType::
FontFreeType(FT_Face pface, FreetypeLib::handle lib, const RenderParams &render_params):
  FontBase(compute_font_propertes_from_face(pface))
{
  m_d = FASTUIDRAWnew FontFreeTypePrivate(this, pface, lib, render_params);
}

fastuidraw::FontFreeType::
~FontFreeType()
{
  FontFreeTypePrivate *d;
  d = reinterpret_cast<FontFreeTypePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

uint32_t
fastuidraw::FontFreeType::
glyph_code(uint32_t pcharacter_code) const
{
  FontFreeTypePrivate *d;
  d = reinterpret_cast<FontFreeTypePrivate*>(m_d);

  FT_UInt glyphcode;
  glyphcode = FT_Get_Char_Index(d->m_face, FT_ULong(pcharacter_code));
  return glyphcode;
}

bool
fastuidraw::FontFreeType::
can_create_rendering_data(enum glyph_type tp) const
{
  return tp == coverage_glyph
    || tp == distance_field_glyph
    || tp == curve_pair_glyph;
}

fastuidraw::GlyphRenderData*
fastuidraw::FontFreeType::
compute_rendering_data(GlyphRender render, uint32_t glyph_code, GlyphLayoutData &layout) const
{
  FontFreeTypePrivate *d;
  d = reinterpret_cast<FontFreeTypePrivate*>(m_d);

  switch(render.m_type)
    {
    case coverage_glyph:
      {
        GlyphRenderDataCoverage *data;
        data = FASTUIDRAWnew GlyphRenderDataCoverage();
        d->compute_rendering_data(render.m_pixel_size, glyph_code, layout, *data);
        return data;
      }
      break;

    case distance_field_glyph:
      {
        GlyphRenderDataDistanceField *data;
        data = FASTUIDRAWnew GlyphRenderDataDistanceField();
        d->compute_rendering_data(glyph_code, layout, *data);
        return data;
      }
      break;

    case curve_pair_glyph:
      {
        GlyphRenderDataCurvePair *data;
        data = FASTUIDRAWnew GlyphRenderDataCurvePair();
        d->compute_rendering_data(glyph_code, layout, *data);
        return data;
      }
      break;

    default:
      assert(!"Invalid glyph type");
      return NULL;
    }
}


const fastuidraw::FontFreeType::RenderParams&
fastuidraw::FontFreeType::
render_params(void) const
{
  FontFreeTypePrivate *d;
  d = reinterpret_cast<FontFreeTypePrivate*>(m_d);
  return d->m_render_params;
}

FT_Face
fastuidraw::FontFreeType::
face(void) const
{
  FontFreeTypePrivate *d;
  d = reinterpret_cast<FontFreeTypePrivate*>(m_d);
  return d->m_face;
}

int
fastuidraw::FontFreeType::
create(c_array<handle> fonts, const char *filename,
       FreetypeLib::handle lib, const RenderParams &render_params)
{
  if(!lib || !lib->valid())
    {
      return 0;
    }

  FT_Face face(NULL);
  int error_code;
  unsigned int num(0);

  error_code = FT_New_Face(lib->lib(), filename, -1, &face);
  if(error_code == 0 && face != NULL && (face->face_flags & FT_FACE_FLAG_SCALABLE) == 0)
    {
      fastuidraw::FontFreeType::handle f;

      num = face->num_faces;
      for(unsigned int i = 0, c = 0; i < num && c < fonts.size(); ++i, ++c)
        {
          fonts[c] = create(filename, lib, render_params, i);
        }
    }

  if(face != NULL)
    {
      FT_Done_Face(face);
    }

  return num;
}

fastuidraw::FontFreeType::handle
fastuidraw::FontFreeType::
create(const char *filename, FreetypeLib::handle lib,
       const RenderParams &render_params, int face_index)
{
  if(!lib || !lib->valid())
    {
      return handle();
    }

  int error_code;
  FT_Face face(NULL);
  error_code = FT_New_Face(lib->lib(), filename, face_index, &face);
  if(error_code != 0 || face == NULL || (face->face_flags & FT_FACE_FLAG_SCALABLE) == 0)
    {
      if(face != NULL)
        {
          FT_Done_Face(face);
        }
      return handle();
    }

  FontProperties p;
  std::ostringstream str;

  str << filename << ":" << face_index;
  compute_font_propertes_from_face(face, p);
  p.source_label(str.str().c_str());

  return FASTUIDRAWnew FontFreeType(face, lib, p, render_params);
}

fastuidraw::FontFreeType::handle
fastuidraw::FontFreeType::
create(const char *filename, const RenderParams &render_params, int face_index)
{
  FreetypeLib::handle lib;
  lib = FASTUIDRAWnew FreetypeLib();
  return create(filename, lib, render_params, face_index);
}

fastuidraw::FontProperties
fastuidraw::FontFreeType::
compute_font_propertes_from_face(FT_Face in_face)
{
  FontProperties return_value;
  compute_font_propertes_from_face(in_face, return_value);
  return return_value;
}

void
fastuidraw::FontFreeType::
compute_font_propertes_from_face(FT_Face in_face, FontProperties &out_properties)
{
  out_properties.family(in_face->family_name);
  out_properties.style(in_face->style_name);
  out_properties.bold(in_face->style_flags & FT_STYLE_FLAG_BOLD);
  out_properties.italic(in_face->style_flags & FT_STYLE_FLAG_ITALIC);
}
