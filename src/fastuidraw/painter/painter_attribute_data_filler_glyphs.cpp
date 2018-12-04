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
          && G.render_size().x() > 0
          && G.render_size().y() > 0)
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
          && d->m_glyphs[g].render_size().x() > 0
          && d->m_glyphs[g].render_size().y() > 0)
        {
          float scale;
          unsigned int t;

          scale = (d->m_render_pixel_size.first) ?
            d->m_render_pixel_size.second / d->m_glyphs[g].metrics().units_per_EM() :
            (d->m_scale_factors.empty()) ? 1.0f : d->m_scale_factors[g];

          t = d->m_glyphs[g].type();
          d->m_glyphs[g].pack_glyph(4 * current[t], const_cast_c_array(attrib_chunks[t]),
                                    6 * current[t], const_cast_c_array(index_chunks[t]),
                                    d->m_glyph_positions[g], scale,
                                    d->m_orientation, d->m_layout);
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
	  && G.render_size().x() > 0
	  && G.render_size().y() > 0)
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
	  && G.render_size().x() > 0
	  && G.render_size().y() > 0)
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
          G.pack_glyph(current_attr, dst_attribs,
                       current_idx, dst_indices,
                       glyph_positions[i], scale,
                       orientation, layout);

	  current_attr += 4u;
	  current_idx += 6u;
	}
    }

  return routine_success;
}
