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
  void
  pack_glyph_indices(fastuidraw::c_array<fastuidraw::PainterIndex> dst, unsigned int aa)
  {
    FASTUIDRAWassert(dst.size() == 6);
    dst[0] = aa + fastuidraw::GlyphAttribute::bottom_left_corner;
    dst[1] = aa + fastuidraw::GlyphAttribute::bottom_right_corner;
    dst[2] = aa + fastuidraw::GlyphAttribute::top_right_corner;
    dst[3] = aa + fastuidraw::GlyphAttribute::bottom_left_corner;
    dst[4] = aa + fastuidraw::GlyphAttribute::top_left_corner;
    dst[5] = aa + fastuidraw::GlyphAttribute::top_right_corner;
  }

  inline
  fastuidraw::vec2
  glyph_position(fastuidraw::vec2 &p_bl, const fastuidraw::vec2 &p_tr,
                 unsigned int corner_enum)
  {
    fastuidraw::vec2 v;

    v.x() = (corner_enum & fastuidraw::GlyphAttribute::right_corner_mask) ? p_tr.x() : p_bl.x();
    v.y() = (corner_enum & fastuidraw::GlyphAttribute::top_corner_mask)   ? p_tr.y() : p_bl.y();
    return v;
  }

  uint32_t
  pack_single_attribute(unsigned int src,
                        fastuidraw::c_array<const fastuidraw::GlyphAttribute> a,
                        unsigned int corner_enum)
  {
    if (a.size() > src)
      {
        return a[src].m_data[corner_enum];
      }
    else
      {
        return 0u;
      }
  }

  inline
  void
  pack_glyph_attribute(fastuidraw::PainterAttribute *dst, unsigned int corner_enum,
                       fastuidraw::vec2 &p_bl, const fastuidraw::vec2 &p_tr,
                       fastuidraw::c_array<const fastuidraw::GlyphAttribute> glyph_attribs)
  {
    fastuidraw::vec2 p(glyph_position(p_bl, p_tr, corner_enum));
    fastuidraw::vec2 wh;
    unsigned int srcI, dstI;

    wh = (p_tr - p_bl);

    dst->m_attrib0.x() = fastuidraw::pack_float(p.x());
    dst->m_attrib0.y() = fastuidraw::pack_float(p.y());
    dst->m_attrib0.z() = fastuidraw::pack_float(wh.x());
    dst->m_attrib0.w() = fastuidraw::pack_float(wh.y());

    for (srcI = 0, dstI = 0; dstI < 4; ++dstI, ++srcI)
      {
        dst->m_attrib1[dstI] = pack_single_attribute(srcI, glyph_attribs, corner_enum);
      }

    for (dstI = 0; dstI < 4; ++dstI, ++srcI)
      {
        dst->m_attrib2[dstI] = pack_single_attribute(srcI, glyph_attribs, corner_enum);
      }
  }

  inline
  void
  pack_glyph_attributes(enum fastuidraw::PainterEnums::screen_orientation orientation,
                        enum fastuidraw::PainterEnums::glyph_layout_type layout,
                        fastuidraw::vec2 p, fastuidraw::Glyph glyph, float SCALE,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> dst)
  {
    FASTUIDRAWassert(glyph.valid());

    fastuidraw::c_array<const fastuidraw::GlyphAttribute> glyph_attributes(glyph.attributes());
    fastuidraw::vec2 glyph_size(SCALE * glyph.metrics().size());
    fastuidraw::vec2 p_bl, p_tr;
    fastuidraw::vec2 layout_offset;

    layout_offset = (layout == fastuidraw::PainterEnums::glyph_layout_horizontal) ?
      glyph.metrics().horizontal_layout_offset() :
      glyph.metrics().vertical_layout_offset();

    /* ISSUE: we are assuming horizontal layout; we should probably
     * change the interface so that caller chooses how to adjust
     * positions with the choices:
     *   adjust_using_horizontal,
     *   adjust_using_vertical,
     *   no_adjust
     */
    if (orientation == fastuidraw::PainterEnums::y_increases_downwards)
      {
        p_bl.x() = p.x() + SCALE * layout_offset.x();
        p_tr.x() = p_bl.x() + glyph_size.x();

        p_bl.y() = p.y() - SCALE * layout_offset.y();
        p_tr.y() = p_bl.y() - glyph_size.y();
      }
    else
      {
        p_bl = p + SCALE * glyph.metrics().horizontal_layout_offset();
        p_tr = p_bl + glyph_size;
      }

    for (unsigned int corner_enum = 0; corner_enum < 4; ++corner_enum)
      {
        pack_glyph_attribute(&dst[corner_enum], corner_enum,
                             p_bl, p_tr, glyph_attributes);
      }
  }

  class FillGlyphsPrivate
  {
  public:
    FillGlyphsPrivate(fastuidraw::c_array<const fastuidraw::vec2> glyph_positions,
                      fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
                      fastuidraw::c_array<const float> scale_factors,
                      enum fastuidraw::PainterEnums::screen_orientation orientation,
                      enum fastuidraw::PainterEnums::glyph_layout_type layout);

    FillGlyphsPrivate(fastuidraw::c_array<const fastuidraw::vec2> glyph_positions,
                      fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
                      float render_pixel_size,
                      enum fastuidraw::PainterEnums::screen_orientation orientation,
                      enum fastuidraw::PainterEnums::glyph_layout_type layout);

    FillGlyphsPrivate(fastuidraw::c_array<const fastuidraw::vec2> glyph_positions,
                      fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
                      enum fastuidraw::PainterEnums::screen_orientation orientation,
                      enum fastuidraw::PainterEnums::glyph_layout_type layout);

    void
    compute_number_glyphs(void);

    fastuidraw::c_array<const fastuidraw::vec2> m_glyph_positions;
    fastuidraw::c_array<const fastuidraw::Glyph> m_glyphs;
    fastuidraw::c_array<const float> m_scale_factors;
    enum fastuidraw::PainterEnums::screen_orientation m_orientation;
    enum fastuidraw::PainterEnums::glyph_layout_type m_layout;
    std::pair<bool, float> m_render_pixel_size;
    unsigned int m_number_glyphs;
    std::vector<unsigned int> m_cnt_by_type;
  };
}

//////////////////////////////////
// FillGlyphsPrivate methods
FillGlyphsPrivate::
FillGlyphsPrivate(fastuidraw::c_array<const fastuidraw::vec2> glyph_positions,
                  fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
                  fastuidraw::c_array<const float> scale_factors,
                  enum fastuidraw::PainterEnums::screen_orientation orientation,
                  enum fastuidraw::PainterEnums::glyph_layout_type layout):
  m_glyph_positions(glyph_positions),
  m_glyphs(glyphs),
  m_scale_factors(scale_factors),
  m_orientation(orientation),
  m_layout(layout),
  m_render_pixel_size(false, 1.0f),
  m_number_glyphs(0)
{
  FASTUIDRAWassert(glyph_positions.size() == glyphs.size());
  FASTUIDRAWassert(scale_factors.empty() || scale_factors.size() == glyphs.size());
}

FillGlyphsPrivate::
FillGlyphsPrivate(fastuidraw::c_array<const fastuidraw::vec2> glyph_positions,
                  fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
                  float render_pixel_size,
                  enum fastuidraw::PainterEnums::screen_orientation orientation,
                  enum fastuidraw::PainterEnums::glyph_layout_type layout):
  m_glyph_positions(glyph_positions),
  m_glyphs(glyphs),
  m_orientation(orientation),
  m_layout(layout),
  m_render_pixel_size(true, render_pixel_size),
  m_number_glyphs(0)
{
  FASTUIDRAWassert(glyph_positions.size() == glyphs.size());
}

FillGlyphsPrivate::
FillGlyphsPrivate(fastuidraw::c_array<const fastuidraw::vec2> glyph_positions,
                  fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
                  enum fastuidraw::PainterEnums::screen_orientation orientation,
                  enum fastuidraw::PainterEnums::glyph_layout_type layout):
  m_glyph_positions(glyph_positions),
  m_glyphs(glyphs),
  m_orientation(orientation),
  m_layout(layout),
  m_render_pixel_size(false, 1.0f),
  m_number_glyphs(0)
{
  FASTUIDRAWassert(glyph_positions.size() == glyphs.size());
}

void
FillGlyphsPrivate::
compute_number_glyphs(void)
{
  for(const auto &G : m_glyphs)
    {
      if (G.valid()
          && G.metrics().size().x() > 0
          && G.metrics().size().y() > 0)
        {
          FASTUIDRAWassert(G.uploaded_to_atlas());
          ++m_number_glyphs;
          if (m_cnt_by_type.size() <= G.type())
            {
              m_cnt_by_type.resize(1 + G.type(), 0);
            }
          ++m_cnt_by_type[G.type()];
        }
    }
}

//////////////////////////////////////////
// PainterAttributeDataFillerGlyphs methods
fastuidraw::PainterAttributeDataFillerGlyphs::
PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                 c_array<const Glyph> glyphs,
                                 c_array<const float> scale_factors,
                                 enum PainterEnums::screen_orientation orientation,
                                 enum PainterEnums::glyph_layout_type layout)
{
  m_d = FASTUIDRAWnew FillGlyphsPrivate(glyph_positions, glyphs,
                                        scale_factors, orientation, layout);
}

fastuidraw::PainterAttributeDataFillerGlyphs::
PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                 c_array<const Glyph> glyphs,
                                 float render_pixel_size,
                                 enum PainterEnums::screen_orientation orientation,
                                 enum PainterEnums::glyph_layout_type layout)
{
  m_d = FASTUIDRAWnew FillGlyphsPrivate(glyph_positions, glyphs,
                                        render_pixel_size, orientation, layout);
}

fastuidraw::PainterAttributeDataFillerGlyphs::
PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                 c_array<const Glyph> glyphs,
                                 enum PainterEnums::screen_orientation orientation,
                                 enum PainterEnums::glyph_layout_type layout)
{
  m_d = FASTUIDRAWnew FillGlyphsPrivate(glyph_positions, glyphs,
                                        orientation, layout);
}

fastuidraw::PainterAttributeDataFillerGlyphs::
~PainterAttributeDataFillerGlyphs()
{
  FillGlyphsPrivate *d;
  d = static_cast<FillGlyphsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PainterAttributeDataFillerGlyphs::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_ranges) const
{
  FillGlyphsPrivate *d;
  d = static_cast<FillGlyphsPrivate*>(m_d);

  d->compute_number_glyphs();
  number_attributes = 4 * d->m_number_glyphs;
  number_indices = 6 * d->m_number_glyphs;
  number_attribute_chunks = d->m_cnt_by_type.size();
  number_index_chunks = d->m_cnt_by_type.size();
  number_z_ranges = 0;
}

void
fastuidraw::PainterAttributeDataFillerGlyphs::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<c_array<const PainterAttribute> > attrib_chunks,
          c_array<c_array<const PainterIndex> > index_chunks,
          c_array<range_type<int> > zranges,
          c_array<int> index_adjusts) const
{
  FillGlyphsPrivate *d;
  d = static_cast<FillGlyphsPrivate*>(m_d);
  for(unsigned int i = 0, c = 0, endi = d->m_cnt_by_type.size(); i < endi; ++i)
    {
      attrib_chunks[i] = attribute_data.sub_array(4 * c, 4 * d->m_cnt_by_type[i]);
      index_chunks[i] = index_data.sub_array(6 * c, 6 * d->m_cnt_by_type[i]);
      index_adjusts[i] = 0;
      c += d->m_cnt_by_type[i];
    }

  FASTUIDRAWassert(zranges.empty());
  FASTUIDRAWunused(zranges);

  std::vector<unsigned int> current(attrib_chunks.size(), 0);
  for(unsigned int g = 0, endg = d->m_glyphs.size(); g < endg; ++g)
    {
      if (d->m_glyphs[g].valid()
          && d->m_glyphs[g].metrics().size().x() > 0
          && d->m_glyphs[g].metrics().size().y() > 0)
        {
          float scale;
          unsigned int t;

          scale = (d->m_render_pixel_size.first) ?
            d->m_render_pixel_size.second / d->m_glyphs[g].metrics().units_per_EM() :
            (d->m_scale_factors.empty()) ? 1.0f : d->m_scale_factors[g];

          t = d->m_glyphs[g].type();
          pack_glyph_attributes(d->m_orientation, d->m_layout,
                                d->m_glyph_positions[g], d->m_glyphs[g], scale,
                                const_cast_c_array(attrib_chunks[t].sub_array(4 * current[t], 4)));
          pack_glyph_indices(const_cast_c_array(index_chunks[t].sub_array(6 * current[t], 6)), 4 * current[t]);
          ++current[t];
        }
    }
}

enum fastuidraw::return_code
fastuidraw::PainterAttributeDataFillerGlyphs::
compute_number_attributes_indices_needed(c_array<const Glyph> glyphs,
					 unsigned int *out_number_attributes,
					 unsigned int *out_number_indices)
{
  enum glyph_type tp(invalid_glyph);
  unsigned int a(0u), i(0u);

  for (Glyph G : glyphs)
    {
      if (G.valid()
	  && G.metrics().size().x() > 0
	  && G.metrics().size().y() > 0)
	{
	  if (tp == invalid_glyph)
	    {
	      tp = G.type();
	    }
	  else if (tp != G.type())
	    {
	      *out_number_attributes = 0u;
	      *out_number_indices = 0u;
	      return routine_fail;
	    }

	  a += 4u;
	  i += 6u;
	}
    }

  *out_number_attributes = a;
  *out_number_indices = i;

  return routine_success;
}

enum fastuidraw::return_code
fastuidraw::PainterAttributeDataFillerGlyphs::
pack_attributes_indices(c_array<const vec2> glyph_positions,
			c_array<const Glyph> glyphs,
			float render_pixel_size,
			enum PainterEnums::screen_orientation orientation,
			enum PainterEnums::glyph_layout_type layout,
			c_array<PainterAttribute> dst_attribs,
			c_array<PainterIndex> dst_indices)
{
  unsigned int current_attr(0), current_idx(0);
  enum glyph_type tp(invalid_glyph);

  for (unsigned int i = 0; i < glyphs.size(); ++i)
    {
      Glyph G(glyphs[i]);

      if (G.valid()
	  && G.metrics().size().x() > 0
	  && G.metrics().size().y() > 0)
	{
	  if (tp == invalid_glyph)
	    {
	      tp = G.type();
	    }
	  else if (tp != G.type())
	    {
	      return routine_fail;
	    }

	  if (current_attr + 4 > dst_attribs.size())
	    {
	      return routine_fail;
	    }

	  if (current_idx + 6 > dst_indices.size())
	    {
	      return routine_fail;
	    }

	  float scale;

	  scale = render_pixel_size / G.metrics().units_per_EM();
	  pack_glyph_attributes(orientation, layout, glyph_positions[i], G, scale,
				dst_attribs.sub_array(current_attr, 4));
	  pack_glyph_indices(dst_indices.sub_array(current_idx, 6), current_attr);

	  current_attr += 4u;
	  current_idx += 6u;
	}
    }

  return routine_success;
}
