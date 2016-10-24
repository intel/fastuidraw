/*!
 * \file painter_attribute_data_filler_glyphs.cpp
 * \brief file painter_attribute_data_filler_glyphs.cpp
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

#include <algorithm>
#include <vector>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_glyphs.hpp>
#include "../private/util_private.hpp"

namespace
{
  inline
  uint32_t
  filter_atlas_layer(int layer)
  {
    assert(layer >= -1);
    return (layer != -1) ? static_cast<uint32_t>(layer) : ~0u;
  }

  inline
  void
  pack_glyph_indices(fastuidraw::c_array<fastuidraw::PainterIndex> dst, unsigned int aa)
  {
    assert(dst.size() == 6);
    dst[0] = aa;
    dst[1] = aa + 1;
    dst[2] = aa + 2;
    dst[3] = aa;
    dst[4] = aa + 2;
    dst[5] = aa + 3;
  }

  inline
  void
  pack_glyph_attributes(enum fastuidraw::PainterEnums::glyph_orientation orientation,
                        fastuidraw::vec2 p, fastuidraw::Glyph glyph, float SCALE,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> dst)
  {
    assert(glyph.valid());

    fastuidraw::GlyphLocation atlas(glyph.atlas_location());
    fastuidraw::GlyphLocation secondary_atlas(glyph.secondary_atlas_location());
    fastuidraw::uvec4 uint_values;
    fastuidraw::vec2 tex_size(atlas.size());
    fastuidraw::vec2 tex_xy(atlas.location());
    fastuidraw::vec2 secondary_tex_xy(secondary_atlas.location());
    fastuidraw::vec2 t_bl(tex_xy), t_tr(t_bl + tex_size);
    fastuidraw::vec2 t2_bl(secondary_tex_xy), t2_tr(t2_bl + tex_size);
    fastuidraw::vec2 glyph_size(SCALE * glyph.layout().m_size);
    fastuidraw::vec2 p_bl, p_tr;

    /* ISSUE: we are assuming horizontal layout; we should probably
       change the inteface so that caller chooses how to adjust
       positions with the choices:
         adjust_using_horizontal,
         adjust_using_vertical,
         no_adjust
     */
    if(orientation == fastuidraw::PainterEnums::y_increases_downwards)
      {
        p_bl.x() = p.x() + SCALE * glyph.layout().m_horizontal_layout_offset.x();
        p_tr.x() = p_bl.x() + glyph_size.x();

        p_bl.y() = p.y() - SCALE * glyph.layout().m_horizontal_layout_offset.y();
        p_tr.y() = p_bl.y() - glyph_size.y();
      }
    else
      {
        p_bl = p + SCALE * glyph.layout().m_horizontal_layout_offset;
        p_tr = p_bl + glyph_size;
      }

    /* secondary_atlas.layer() can be -1 to indicate that
       the glyph does not have secondary atlas, when changed
       to an unsigned value it is ungood, to compensate we
       will "do something".
     */
    uint_values.x() = 0u;
    uint_values.y() = glyph.geometry_offset();
    uint_values.z() = filter_atlas_layer(atlas.layer());
    uint_values.w() = filter_atlas_layer(secondary_atlas.layer());

    dst[0].m_attrib0 = fastuidraw::pack_vec4(t_bl.x(), t_bl.y(), t2_bl.x(), t2_bl.y());
    dst[0].m_attrib1 = fastuidraw::pack_vec4(p_bl.x(), p_bl.y(), 0.0f, 0.0f);
    dst[0].m_attrib2 = uint_values;

    dst[1].m_attrib0 = fastuidraw::pack_vec4(t_tr.x(), t_bl.y(), t2_tr.x(), t2_bl.y());
    dst[1].m_attrib1 = fastuidraw::pack_vec4(p_tr.x(), p_bl.y(), 0.0f, 0.0f);
    dst[1].m_attrib2 = uint_values;

    dst[2].m_attrib0 = fastuidraw::pack_vec4(t_tr.x(), t_tr.y(), t2_tr.x(), t2_tr.y());
    dst[2].m_attrib1 = fastuidraw::pack_vec4(p_tr.x(), p_tr.y(), 0.0f, 0.0f);
    dst[2].m_attrib2 = uint_values;

    dst[3].m_attrib0 = fastuidraw::pack_vec4(t_bl.x(), t_tr.y(), t2_bl.x(), t2_tr.y());
    dst[3].m_attrib1 = fastuidraw::pack_vec4(p_bl.x(), p_tr.y(), 0.0f, 0.0f);
    dst[3].m_attrib2 = uint_values;
  }

  class FillGlyphsPrivate
  {
  public:
    FillGlyphsPrivate(fastuidraw::const_c_array<fastuidraw::vec2> glyph_positions,
                      fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                      fastuidraw::const_c_array<float> scale_factors,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation);

    FillGlyphsPrivate(fastuidraw::const_c_array<fastuidraw::vec2> glyph_positions,
                      fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                      float render_pixel_size,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation);

    FillGlyphsPrivate(fastuidraw::const_c_array<fastuidraw::vec2> glyph_positions,
                      fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                      enum fastuidraw::PainterEnums::glyph_orientation orientation);

    void
    compute_number_glyphs(void);

    fastuidraw::const_c_array<fastuidraw::vec2> m_glyph_positions;
    fastuidraw::const_c_array<fastuidraw::Glyph> m_glyphs;
    fastuidraw::const_c_array<float> m_scale_factors;
    enum fastuidraw::PainterEnums::glyph_orientation m_orientation;
    std::pair<bool, float> m_render_pixel_size;
    unsigned int m_number_glyphs;
    std::vector<unsigned int> m_cnt_by_type;
  };
}

//////////////////////////////////
// FillGlyphsPrivate methods
FillGlyphsPrivate::
FillGlyphsPrivate(fastuidraw::const_c_array<fastuidraw::vec2> glyph_positions,
                  fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                  fastuidraw::const_c_array<float> scale_factors,
                  enum fastuidraw::PainterEnums::glyph_orientation orientation):
  m_glyph_positions(glyph_positions),
  m_glyphs(glyphs),
  m_scale_factors(scale_factors),
  m_orientation(orientation),
  m_render_pixel_size(false, 1.0f),
  m_number_glyphs(0)
{
  assert(glyph_positions.size() == glyphs.size());
  assert(scale_factors.empty() || scale_factors.size() == glyphs.size());
}

FillGlyphsPrivate::
FillGlyphsPrivate(fastuidraw::const_c_array<fastuidraw::vec2> glyph_positions,
                  fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                  float render_pixel_size,
                  enum fastuidraw::PainterEnums::glyph_orientation orientation):
  m_glyph_positions(glyph_positions),
  m_glyphs(glyphs),
  m_orientation(orientation),
  m_render_pixel_size(true, render_pixel_size),
  m_number_glyphs(0)
{
  assert(glyph_positions.size() == glyphs.size());
}

FillGlyphsPrivate::
FillGlyphsPrivate(fastuidraw::const_c_array<fastuidraw::vec2> glyph_positions,
                  fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                  enum fastuidraw::PainterEnums::glyph_orientation orientation):
  m_glyph_positions(glyph_positions),
  m_glyphs(glyphs),
  m_orientation(orientation),
  m_render_pixel_size(false, 1.0f),
  m_number_glyphs(0)
{
  assert(glyph_positions.size() == glyphs.size());
}

void
FillGlyphsPrivate::
compute_number_glyphs(void)
{
  for(unsigned int i = 0, endi = m_glyphs.size(); i < endi; ++i)
    {
      enum fastuidraw::return_code R;

      if(m_glyphs[i].valid())
        {
          R = m_glyphs[i].upload_to_atlas();
          if(R != fastuidraw::routine_success)
            {
              return;
            }
          ++m_number_glyphs;

          if(m_cnt_by_type.size() <= m_glyphs[i].type())
            {
              m_cnt_by_type.resize(1 + m_glyphs[i].type(), 0);
            }
          ++m_cnt_by_type[m_glyphs[i].type()];
        }
    }
}


//////////////////////////////////////////
// PainterAttributeDataFillerGlyphs methods
fastuidraw::PainterAttributeDataFillerGlyphs::
PainterAttributeDataFillerGlyphs(const_c_array<vec2> glyph_positions,
                                 const_c_array<Glyph> glyphs,
                                 const_c_array<float> scale_factors,
                                 enum PainterEnums::glyph_orientation orientation)
{
  m_d = FASTUIDRAWnew FillGlyphsPrivate(glyph_positions, glyphs, scale_factors, orientation);
}

fastuidraw::PainterAttributeDataFillerGlyphs::
PainterAttributeDataFillerGlyphs(const_c_array<vec2> glyph_positions,
                                 const_c_array<Glyph> glyphs,
                                 float render_pixel_size,
                                 enum PainterEnums::glyph_orientation orientation)
{
  m_d = FASTUIDRAWnew FillGlyphsPrivate(glyph_positions, glyphs, render_pixel_size, orientation);
}

fastuidraw::PainterAttributeDataFillerGlyphs::
PainterAttributeDataFillerGlyphs(const_c_array<vec2> glyph_positions,
                                 const_c_array<Glyph> glyphs,
                                 enum PainterEnums::glyph_orientation orientation)
{
  m_d = FASTUIDRAWnew FillGlyphsPrivate(glyph_positions, glyphs, orientation);
}

fastuidraw::PainterAttributeDataFillerGlyphs::
~PainterAttributeDataFillerGlyphs()
{
  FillGlyphsPrivate *d;
  d = reinterpret_cast<FillGlyphsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::PainterAttributeDataFillerGlyphs::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_increments) const
{
  FillGlyphsPrivate *d;
  d = reinterpret_cast<FillGlyphsPrivate*>(m_d);

  d->compute_number_glyphs();
  number_attributes = 4 * d->m_number_glyphs;
  number_indices = 6 * d->m_number_glyphs;
  number_attribute_chunks = d->m_cnt_by_type.size();
  number_index_chunks = d->m_cnt_by_type.size();
  number_z_increments = 0;
}

void
fastuidraw::PainterAttributeDataFillerGlyphs::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attrib_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements,
          c_array<int> index_adjusts) const
{
  FillGlyphsPrivate *d;
  d = reinterpret_cast<FillGlyphsPrivate*>(m_d);
  for(unsigned int i = 0, c = 0, endi = d->m_cnt_by_type.size(); i < endi; ++i)
    {
      attrib_chunks[i] = attribute_data.sub_array(4 * c, 4 * d->m_cnt_by_type[i]);
      index_chunks[i] = index_data.sub_array(6 * c, 6 * d->m_cnt_by_type[i]);
      index_adjusts[i] = 0;
      c += d->m_cnt_by_type[i];
    }

  assert(zincrements.empty());
  FASTUIDRAWunused(zincrements);

  std::vector<unsigned int> current(attrib_chunks.size(), 0);
  for(unsigned int g = 0; g < d->m_number_glyphs; ++g)
    {
      if(d->m_glyphs[g].valid())
        {
          float scale;
          unsigned int t;

          scale = (d->m_render_pixel_size.first) ?
            d->m_render_pixel_size.second / d->m_glyphs[g].layout().m_pixel_size :
            (d->m_scale_factors.empty()) ? 1.0f : d->m_scale_factors[g];

          t = d->m_glyphs[g].type();
          pack_glyph_attributes(d->m_orientation, d->m_glyph_positions[g],
                                d->m_glyphs[g], scale,
                                const_cast_c_array(attrib_chunks[t].sub_array(4 * current[t], 4)));
          pack_glyph_indices(const_cast_c_array(index_chunks[t].sub_array(6 * current[t], 6)), 4 * current[t]);
          ++current[t];
        }
    }
}
