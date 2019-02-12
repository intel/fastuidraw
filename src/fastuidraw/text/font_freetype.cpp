/*!
 * \file font_freetype.cpp
 * \brief file font_freetype.cpp
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

#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/text/glyph_render_data_texels.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include <fastuidraw/text/glyph_render_data_banded_rays.hpp>

#include "../private/array2d.hpp"
#include "../private/util_private.hpp"
#include "../private/int_path.hpp"

#include <ft2build.h>
#include FT_OUTLINE_H

namespace
{
  /* covert TO FontCoordinates from Freetype values that
   * are stored in scaled pixel size values.
   */
  class font_coordinate_converter
  {
  public:
    font_coordinate_converter(FT_Face face, unsigned int pixel_size)
    {
      m_factor = static_cast<float>(face->units_per_EM);
      m_factor /= 64.0f * static_cast<float>(pixel_size);
    }

    font_coordinate_converter(void):
      m_factor(1.0f)
    {}

    float
    factor(void) const
    {
      return m_factor;
    }

    float
    operator()(FT_Pos p) const
    {
      return static_cast<float>(p) * m_factor;
    }

  private:
    float m_factor;
  };

  class GenerateParams
  {
  public:
    GenerateParams(void):
      m_distance_field_pixel_size(fastuidraw::GlyphGenerateParams::distance_field_pixel_size()),
      m_distance_field_max_distance(fastuidraw::GlyphGenerateParams::distance_field_max_distance())
    {}

    unsigned int m_distance_field_pixel_size;
    float m_distance_field_max_distance;
  };

  class ComputeOutlineDegree
  {
  public:
    static
    int
    compute(FT_Outline *outline)
    {
      int return_value(0);
      FT_Outline_Funcs funcs;

      funcs.move_to = &ft_outline_move_to;
      funcs.line_to = &ft_outline_line_to;
      funcs.conic_to = &ft_outline_conic_to;
      funcs.cubic_to = &ft_outline_cubic_to;
      funcs.shift = 0;
      funcs.delta = 0;
      FT_Outline_Decompose(outline, &funcs, &return_value);
      return return_value;
    }

  private:
    static
    int
    ft_outline_move_to(const FT_Vector*, void*)
    {
      return 0;
    }

    static
    int
    ft_outline_line_to(const FT_Vector*, void *user)
    {
      int *p;
      p = static_cast<int*>(user);
      *p = fastuidraw::t_max(1, *p);
      return 0;
    }

    static
    int
    ft_outline_conic_to(const FT_Vector*,
                        const FT_Vector*, void *user)
    {
      int *p;
      p = static_cast<int*>(user);
      *p = fastuidraw::t_max(2, *p);
      return 0;
    }

    static
    int
    ft_outline_cubic_to(const FT_Vector*,
                        const FT_Vector*,
                        const FT_Vector*, void *user)
    {
      int *p;
      p = static_cast<int*>(user);
      *p = fastuidraw::t_max(3, *p);
      return 0;
    }
  };

  class IntPathCreator
  {
  public:
    static
    void
    decompose_to_path(FT_Outline *outline, fastuidraw::detail::IntPath &p, int factor)
    {
      IntPathCreator datum(p, factor);
      FT_Outline_Funcs funcs;

      funcs.move_to = &ft_outline_move_to;
      funcs.line_to = &ft_outline_line_to;
      funcs.conic_to = &ft_outline_conic_to;
      funcs.cubic_to = &ft_outline_cubic_to;
      funcs.shift = 0;
      funcs.delta = 0;
      FT_Outline_Decompose(outline, &funcs, &datum);
    }

    static
    void
    decompose_to_path(FT_Outline *outline, fastuidraw::Path &p,
                      font_coordinate_converter C)
    {
      fastuidraw::detail::IntPath ip;
      fastuidraw::detail::IntBezierCurve::transformation<float> tr(C.factor());

      decompose_to_path(outline, ip, 1);
      ip.add_to_path(tr, &p);
    }

  private:
    explicit
    IntPathCreator(fastuidraw::detail::IntPath &P, int factor):
      m_path(P),
      m_factor(factor)
    {}

    static
    int
    ft_outline_move_to(const FT_Vector *pt, void *user)
    {
      IntPathCreator *p;
      p = static_cast<IntPathCreator*>(user);
      p->m_path.move_to(p->m_factor * fastuidraw::ivec2(pt->x, pt->y));
      return 0;
    }

    static
    int
    ft_outline_line_to(const FT_Vector *pt, void *user)
    {
      IntPathCreator *p;
      p = static_cast<IntPathCreator*>(user);
      p->m_path.line_to(p->m_factor * fastuidraw::ivec2(pt->x, pt->y));
      return 0;
    }

    static
    int
    ft_outline_conic_to(const FT_Vector *control_pt,
                        const FT_Vector *pt, void *user)
    {
      IntPathCreator *p;
      p = static_cast<IntPathCreator*>(user);
      p->m_path.conic_to(p->m_factor * fastuidraw::ivec2(control_pt->x, control_pt->y),
                         p->m_factor * fastuidraw::ivec2(pt->x, pt->y));
      return 0;
    }

    static
    int
    ft_outline_cubic_to(const FT_Vector *control_pt0,
                        const FT_Vector *control_pt1,
                        const FT_Vector *pt, void *user)
    {
      IntPathCreator *p;
      p = static_cast<IntPathCreator*>(user);
      p->m_path.cubic_to(p->m_factor * fastuidraw::ivec2(control_pt0->x, control_pt0->y),
                         p->m_factor * fastuidraw::ivec2(control_pt1->x, control_pt1->y),
                         p->m_factor * fastuidraw::ivec2(pt->x, pt->y));
      return 0;
    }

    fastuidraw::detail::IntPath &m_path;
    int m_factor;
  };

  class FaceAndEncoding
  {
  public:
    FaceAndEncoding(void):
      m_current_encoding(static_cast<enum fastuidraw::CharacterEncoding::encoding_value_t>(0))
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace> m_face;
    enum fastuidraw::CharacterEncoding::encoding_value_t m_current_encoding;
  };

  class FontFreeTypePrivate
  {
  public:

    class FaceGrabber
    {
    public:
      FaceGrabber(FontFreeTypePrivate *q);
      ~FaceGrabber();

      fastuidraw::FreeTypeFace *m_p;
      enum fastuidraw::CharacterEncoding::encoding_value_t *m_current_encoding;
    };

    FontFreeTypePrivate(fastuidraw::FontFreeType *p,
                        const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase> &pface_generator,
                        fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                        unsigned int num_faces);

    ~FontFreeTypePrivate();

    static
    void
    load_glyph(FT_Face face, uint32_t glyph_code);

    void
    compute_rendering_data_coverage(int pixel_size,
                                    fastuidraw::GlyphMetrics glyph_metrics,
                                    fastuidraw::GlyphRenderDataTexels &output,
                                    fastuidraw::Path &path,
                                    fastuidraw::vec2 &render_size);

    void
    compute_rendering_data_distance_field(fastuidraw::GlyphMetrics glyph_metrics,
                                          fastuidraw::GlyphRenderDataTexels &output,
                                          fastuidraw::Path &path,
                                          fastuidraw::vec2 &render_size);

    template<typename T, typename P>
    void
    compute_rendering_data_rays(fastuidraw::GlyphMetrics glyph_metrics,
                                T &output,
                                fastuidraw::Path &path,
                                fastuidraw::vec2 &render_size);

    void
    compute_rendering_data_rays_finalize(fastuidraw::GlyphMetrics glyph_metrics,
                                         fastuidraw::GlyphRenderDataRestrictedRays &output,
                                         const fastuidraw::RectT<int> &rect,
                                         int scale_factor,
                                         enum fastuidraw::PainterEnums::fill_rule_t fill_rule)
    {
      output.finalize(fill_rule, rect, static_cast<float>(scale_factor) * glyph_metrics.units_per_EM());
    }

    void
    compute_rendering_data_rays_finalize(fastuidraw::GlyphMetrics /*glyph_metrics*/,
                                         fastuidraw::GlyphRenderDataBandedRays &output,
                                         const fastuidraw::RectT<int> &rectI,
                                         int /*scale_factor*/,
                                         enum fastuidraw::PainterEnums::fill_rule_t fill_rule)
    {
      fastuidraw::Rect rect(rectI);
      output.finalize(fill_rule, rect);
    }

    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase> m_generator;
    GenerateParams m_generate_params;
    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> m_lib;
    fastuidraw::FontFreeType *m_p;

    /* for now we have a static number of m_faces we use for parallel
     * glyph generation
     */
    std::vector<FaceAndEncoding> m_faces;
    bool m_all_faces_null;
    unsigned int m_number_glyphs;
  };
}

///////////////////////////////////////////
// FontFreeTypePrivate::FaceGrabber methods
FontFreeTypePrivate::FaceGrabber::
FaceGrabber(FontFreeTypePrivate *q):
  m_p(nullptr)
{
  while(!m_p && !q->m_all_faces_null)
    {
      for(unsigned int i = 0, endi = q->m_faces.size(); i < endi && !m_p; ++i)
        {
          const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace> &face(q->m_faces[i].m_face);
          if (face && face->try_lock())
            {
              m_p = face.get();
              m_current_encoding = &q->m_faces[i].m_current_encoding;
            }
        }
    }
}

FontFreeTypePrivate::FaceGrabber::
~FaceGrabber()
{
  if (m_p)
    {
      m_p->unlock();
    }
}

//////////////////////////////////////////////////
// FontFreeTypePrivate methods
FontFreeTypePrivate::
FontFreeTypePrivate(fastuidraw::FontFreeType *p,
                    const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase> &generator,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                    unsigned int num_faces):
  m_generator(generator),
  m_lib(lib),
  m_p(p),
  m_faces(num_faces),
  m_all_faces_null(true),
  m_number_glyphs(0)
{
  if (!m_lib)
    {
      m_lib = FASTUIDRAWnew fastuidraw::FreeTypeLib();
    }

  for(unsigned int i = 0; i < m_faces.size(); ++i)
    {
      m_faces[i].m_face = m_generator->create_face(m_lib);
      if (m_faces[i].m_face && m_faces[i].m_face->face())
        {
          m_all_faces_null = false;
          m_number_glyphs = m_faces[i].m_face->face()->num_glyphs;
          FT_Set_Transform(m_faces[i].m_face->face(), nullptr, nullptr);
          FASTUIDRAWwarn_assert(m_faces[i].m_face->face()->face_flags & FT_FACE_FLAG_SCALABLE);
        }
    }
}

FontFreeTypePrivate::
~FontFreeTypePrivate()
{
}

void
FontFreeTypePrivate::
load_glyph(FT_Face face, uint32_t glyph_code)
{
  using namespace fastuidraw;

  FT_Int32 load_flags;

  load_flags = FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING
    | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM
    | FT_LOAD_LINEAR_DESIGN;

  FT_Load_Glyph(face, glyph_code, load_flags);
}

void
FontFreeTypePrivate::
compute_rendering_data_coverage(int pixel_size,
                                fastuidraw::GlyphMetrics glyph_metrics,
                                fastuidraw::GlyphRenderDataTexels &output,
                                fastuidraw::Path &path,
                                fastuidraw::vec2 &render_size)
{
  FaceGrabber p(this);

  if (!p.m_p || !p.m_p->face())
    {
      return;
    }

  FT_Face face(p.m_p->face());
  fastuidraw::ivec2 bitmap_sz;
  font_coordinate_converter C(face, pixel_size);
  uint32_t glyph_code(glyph_metrics.glyph_code());

  FT_Set_Pixel_Sizes(face, pixel_size, pixel_size);
  FT_Load_Glyph(face, glyph_code, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP);
  IntPathCreator::decompose_to_path(&face->glyph->outline, path, C);
  bitmap_sz.x() = face->glyph->bitmap.width;
  bitmap_sz.y() = face->glyph->bitmap.rows;
  render_size = fastuidraw::vec2(bitmap_sz) * float(face->units_per_EM) / float(pixel_size);

  /* add one pixel slack on glyph */
  if (bitmap_sz.x() != 0 && bitmap_sz.y() != 0)
    {
      int pitch;
      fastuidraw::c_array<uint8_t> texel_data;

      pitch = fastuidraw::t_abs(face->glyph->bitmap.pitch);
      output.resize(bitmap_sz);
      texel_data = output.texel_data();

      std::fill(texel_data.begin(), texel_data.end(), 0);
      for(int y = 0; y < bitmap_sz.y(); ++y)
        {
          for(int x = 0; x < bitmap_sz.x(); ++x)
            {
              int write_location, read_location;

              write_location = x + y * output.resolution().x();
              read_location = x + (bitmap_sz.y() - 1 - y) * pitch;
              texel_data[write_location] = face->glyph->bitmap.buffer[read_location];
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
compute_rendering_data_distance_field(fastuidraw::GlyphMetrics glyph_metrics,
                                      fastuidraw::GlyphRenderDataTexels &output,
                                      fastuidraw::Path &path,
                                      fastuidraw::vec2 &render_size)
{
  fastuidraw::detail::IntPath int_path_ecm;
  int units_per_EM;
  fastuidraw::ivec2 layout_offset;
  fastuidraw::vec2 layout_size;
  int outline_flags;
  uint32_t glyph_code(glyph_metrics.glyph_code());

  {
    FaceGrabber p(this);
    if (!p.m_p || !p.m_p->face())
      {
        return;
      }

    FT_Face face(p.m_p->face());

    load_glyph(face, glyph_code);
    units_per_EM = face->units_per_EM;
    outline_flags = face->glyph->outline.flags;
    layout_offset = fastuidraw::ivec2(face->glyph->metrics.horiBearingX,
                                      face->glyph->metrics.horiBearingY);
    layout_offset.y() -= face->glyph->metrics.height;
    layout_size = fastuidraw::vec2(face->glyph->metrics.width,
                                   face->glyph->metrics.height);
    IntPathCreator::decompose_to_path(&face->glyph->outline, int_path_ecm, 1);
  }

  /* TODO: adjust for the discretization to pixels */
  render_size = glyph_metrics.size();

  if (int_path_ecm.empty())
    {
      return;
    }

  fastuidraw::detail::IntBezierCurve::transformation<float> identity_tr;
  int_path_ecm.add_to_path(identity_tr, &path);

  int_path_ecm.replace_cubics_with_quadratics();

  /* choose the correct fill rule as according to outline_flags */
  enum fastuidraw::PainterEnums::fill_rule_t fill_rule;
  fill_rule = (outline_flags & FT_OUTLINE_EVEN_ODD_FILL) ?
    fastuidraw::PainterEnums::odd_even_fill_rule:
    fastuidraw::PainterEnums::nonzero_fill_rule;

  /* compute the step value needed to create the distance field value*/
  int pixel_size(m_generate_params.m_distance_field_pixel_size);
  float scale_factor(static_cast<float>(pixel_size) / static_cast<float>(units_per_EM));

  /* compute how many pixels we need to store the glyph. */
  fastuidraw::vec2 image_sz_f(layout_size * scale_factor);
  fastuidraw::ivec2 image_sz(ceilf(image_sz_f.x()), ceilf(image_sz_f.y()));

  if (image_sz.x() == 0 || image_sz.y() == 0)
    {
      output.resize(fastuidraw::ivec2(0, 0));
      return;
    }

  /* we translate by -layout_offset to make the points of
   * the IntPath match correctly with that of the texel
   * data (this also gaurantees that the box of the glyph
   * is (0, 0)). In addition, we scale by 2 * pixel_size;
   * By doing this, the distance between texel-centers
   * is then just 2 * units_per_EM.
   */
  int tr_scale(2 * pixel_size);
  fastuidraw::ivec2 tr_translate(-2 * pixel_size * layout_offset);
  fastuidraw::detail::IntBezierCurve::transformation<int> tr(tr_scale, tr_translate);
  fastuidraw::ivec2 texel_distance(2 * units_per_EM);
  float max_distance = (m_generate_params.m_distance_field_max_distance)
    * static_cast<float>(2 * units_per_EM);

  int_path_ecm.extract_render_data(texel_distance, image_sz, max_distance, tr,
                                   fastuidraw::CustomFillRuleFunction(fill_rule),
                                   &output);
}

template<typename T, typename P>
void
FontFreeTypePrivate::
compute_rendering_data_rays(fastuidraw::GlyphMetrics glyph_metrics,
                            T &output,
                            fastuidraw::Path &path,
                            fastuidraw::vec2 &render_size)
{
  fastuidraw::detail::IntPath int_path_ecm;
  fastuidraw::ivec2 layout_offset, layout_size;
  int outline_flags;
  uint32_t glyph_code(glyph_metrics.glyph_code());
  int scale_factor;

  {
    FaceGrabber p(this);
    if (!p.m_p || !p.m_p->face())
      {
        return;
      }

    FT_Face face(p.m_p->face());
    load_glyph(face, glyph_code);
    outline_flags = face->glyph->outline.flags;
    layout_offset = fastuidraw::ivec2(face->glyph->metrics.horiBearingX,
                                      face->glyph->metrics.horiBearingY);
    layout_offset.y() -= face->glyph->metrics.height;
    layout_size = fastuidraw::ivec2(face->glyph->metrics.width,
                                    face->glyph->metrics.height);

    if (ComputeOutlineDegree::compute(&face->glyph->outline) > 2)
      {
        /* When breaking cubics into quadratics, the first step is
         * to break each cubic into 4 (via De Casteljau's algorithm)
         * and then approximate each cubic with a single quadratic.
         * Each run of De Casteljau's algorithm on a cubic invokes
         * a dived by 8, running it twice gives a divide by 64; thus
         * to avoid losing any information from that, we scale up by
         * 64.
         */
        scale_factor = 64;
      }
    else
      {
        scale_factor = 1;
      }

    layout_offset *= scale_factor;
    layout_size *= scale_factor;
    IntPathCreator::decompose_to_path(&face->glyph->outline, int_path_ecm, scale_factor);
  }

  /* render size is identical to metric's size because there is
   * no discretization from the glyph'sp outline data.
   */
  render_size = glyph_metrics.size();

  /* extract to a Path */
  fastuidraw::detail::IntBezierCurve::transformation<float> tr(1.0f / float(scale_factor));
  int_path_ecm.add_to_path(tr, &path);

  int_path_ecm.replace_cubics_with_quadratics();
  for (const auto &contour : int_path_ecm.contours())
    {
      if (!contour.curves().empty())
        {
          output.move_to(P(contour.curves().front().control_pts().front()));
          for (const auto &curve: contour.curves())
            {
              if (curve.degree() == 2)
                {
                  output.quadratic_to(P(curve.control_pts()[1]),
                                      P(curve.control_pts()[2]));
                }
              else
                {
                  FASTUIDRAWassert(curve.degree() == 1);
                  output.line_to(P(curve.control_pts()[1]));
                }
            }
        }
    }

  enum fastuidraw::PainterEnums::fill_rule_t fill_rule;
  fill_rule = (outline_flags & FT_OUTLINE_EVEN_ODD_FILL) ?
    fastuidraw::PainterEnums::odd_even_fill_rule:
    fastuidraw::PainterEnums::nonzero_fill_rule;

  compute_rendering_data_rays_finalize(glyph_metrics, output,
                                       fastuidraw::RectT<int>()
                                       .min_point(layout_offset)
                                       .max_point(layout_offset + layout_size),
                                       scale_factor, fill_rule);
}

///////////////////////////////////////////////////
// fastuidraw::FontFreeType methods
fastuidraw::FontFreeType::
FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
             const FontProperties &props,
             const reference_counted_ptr<FreeTypeLib> &plib,
             unsigned int num_faces):
  FontBase(props)
{
  m_d = FASTUIDRAWnew FontFreeTypePrivate(this, pface_generator, plib, num_faces);
}

fastuidraw::FontFreeType::
FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
             const reference_counted_ptr<FreeTypeLib> &plib,
             unsigned int num_faces):
  FontBase(compute_font_properties_from_face(pface_generator->create_face(plib)))
{
  m_d = FASTUIDRAWnew FontFreeTypePrivate(this, pface_generator, plib, num_faces);
}

fastuidraw::FontFreeType::
~FontFreeType()
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::FontFreeType::
glyph_codes(enum CharacterEncoding::encoding_value_t encoding,
            c_array<const uint32_t> in_character_codes,
            c_array<uint32_t> out_glyph_codes) const
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);

  FontFreeTypePrivate::FaceGrabber p(d);
  uint32_t sz;

  sz = t_min(in_character_codes.size(),
             out_glyph_codes.size());

  if (p.m_p && p.m_p->face())
    {
      if (encoding != *p.m_current_encoding)
        {
          FT_Error error;

          error = FT_Select_Charmap(p.m_p->face(), FT_Encoding(encoding));
          if (error)
            {
              p.m_p = nullptr;
            }
          else
            {
              *p.m_current_encoding = encoding;
            }
        }
    }

  if (p.m_p && p.m_p->face())
    {
      for (unsigned int i = 0; i < sz; ++i)
        {
          out_glyph_codes[i] = FT_Get_Char_Index(p.m_p->face(), FT_ULong(in_character_codes[i]));
        }
    }
  else
    {
      for (unsigned int i = 0; i < sz; ++i)
        {
          out_glyph_codes[i] = 0u;
        }
    }
}

bool
fastuidraw::FontFreeType::
can_create_rendering_data(enum glyph_type tp) const
{
  return tp == coverage_glyph
    || tp == distance_field_glyph
    || tp == restricted_rays_glyph
    || tp == banded_rays_glyph;
}

unsigned int
fastuidraw::FontFreeType::
number_glyphs(void) const
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);
  return d->m_number_glyphs;
}

void
fastuidraw::FontFreeType::
compute_metrics(uint32_t glyph_code, GlyphMetricsValue &metrics) const
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);

  FontFreeTypePrivate::FaceGrabber p(d);

  if (!p.m_p || !p.m_p->face())
    {
      return;
    }
  FontFreeTypePrivate::load_glyph(p.m_p->face(), glyph_code);
  FT_Face face(p.m_p->face());

  metrics
    .size(vec2(face->glyph->metrics.width, face->glyph->metrics.height))
    .horizontal_layout_offset(vec2(face->glyph->metrics.horiBearingX,
                                   face->glyph->metrics.horiBearingY - face->glyph->metrics.height))
    .vertical_layout_offset(vec2(face->glyph->metrics.vertBearingX,
                                 face->glyph->metrics.vertBearingY - face->glyph->metrics.height))
    .advance(vec2(face->glyph->linearHoriAdvance, face->glyph->linearVertAdvance))
    .units_per_EM(face->units_per_EM);
}

fastuidraw::GlyphRenderData*
fastuidraw::FontFreeType::
compute_rendering_data(GlyphRenderer render, GlyphMetrics glyph_metrics,
		       Path &path, vec2 &render_size) const
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);

  switch(render.m_type)
    {
    case coverage_glyph:
      {
        GlyphRenderDataTexels *data;
        data = FASTUIDRAWnew GlyphRenderDataTexels();
        d->compute_rendering_data_coverage(render.m_pixel_size, glyph_metrics, *data,
                                           path, render_size);
        return data;
      }
      break;

    case distance_field_glyph:
      {
        GlyphRenderDataTexels *data;
        data = FASTUIDRAWnew GlyphRenderDataTexels();
        d->compute_rendering_data_distance_field(glyph_metrics, *data, path, render_size);
        return data;
      }
      break;

    case restricted_rays_glyph:
      {
        GlyphRenderDataRestrictedRays *data;
        data = FASTUIDRAWnew GlyphRenderDataRestrictedRays();
        d->compute_rendering_data_rays<GlyphRenderDataRestrictedRays, ivec2>(glyph_metrics, *data, path, render_size);
        return data;
      }
      break;

    case banded_rays_glyph:
      {
        GlyphRenderDataBandedRays *data;
        data = FASTUIDRAWnew GlyphRenderDataBandedRays();
        d->compute_rendering_data_rays<GlyphRenderDataBandedRays, vec2>(glyph_metrics, *data, path, render_size);
        return data;
      }
      break;

    default:
      FASTUIDRAWassert(!"Invalid glyph type");
      return nullptr;
    }
}

const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase>&
fastuidraw::FontFreeType::
face_generator(void) const
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);
  return d->m_generator;
}

const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib>&
fastuidraw::FontFreeType::
lib(void) const
{
  FontFreeTypePrivate *d;
  d = static_cast<FontFreeTypePrivate*>(m_d);
  return d->m_lib;
}

fastuidraw::FontProperties
fastuidraw::FontFreeType::
compute_font_properties_from_face(FT_Face in_face)
{
  FontProperties return_value;
  compute_font_properties_from_face(in_face, return_value);
  return return_value;
}

fastuidraw::FontProperties
fastuidraw::FontFreeType::
compute_font_properties_from_face(const reference_counted_ptr<FreeTypeFace> &in_face)
{
  FontProperties return_value;
  if (in_face && in_face->face())
    {
      compute_font_properties_from_face(in_face->face(), return_value);
    }
  return return_value;
}

void
fastuidraw::FontFreeType::
compute_font_properties_from_face(FT_Face in_face, FontProperties &out_properties)
{
  if (in_face)
    {
      out_properties.family(in_face->family_name);
      out_properties.style(in_face->style_name);
      out_properties.bold(in_face->style_flags & FT_STYLE_FLAG_BOLD);
      out_properties.italic(in_face->style_flags & FT_STYLE_FLAG_ITALIC);
    }
}
