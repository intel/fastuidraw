/*!
 * \file glyph.cpp
 * \brief file glyph.cpp
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
#include <fastuidraw/text/glyph.hpp>

namespace
{
  inline
  fastuidraw::vec2
  glyph_position(const fastuidraw::vec2 &p_bl, const fastuidraw::vec2 &p_tr,
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
                       const fastuidraw::vec2 &p_bl, const fastuidraw::vec2 &p_tr,
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
}

void
fastuidraw::Glyph::
pack_glyph(unsigned int attrib_loc, c_array<PainterAttribute> dst,
           unsigned int index_loc, c_array<PainterIndex> dst_index,
           const vec2 p, float scale_factor,
           enum fastuidraw::PainterEnums::screen_orientation orientation,
           enum fastuidraw::PainterEnums::glyph_layout_type layout) const
{
  if (!valid())
    {
      dst = dst.sub_array(attrib_loc, 4);
      dst_index = dst_index.sub_array(index_loc, 6);
      std::fill(dst_index.begin(), dst_index.end(), attrib_loc);
      std::fill(dst.begin(), dst.end(), PainterAttribute());
      return;
    }

  Rect rect;
  vec2 glyph_size(0.0f, 0.0f);
  vec2 p_bl, p_tr;
  vec2 layout_offset(0.0f, 0.0f);

  glyph_size = scale_factor * render_size();
  layout_offset = (layout == fastuidraw::PainterEnums::glyph_layout_horizontal) ?
    metrics().horizontal_layout_offset() :
    metrics().vertical_layout_offset();

  if (orientation == fastuidraw::PainterEnums::y_increases_downwards)
    {
      p_bl.x() = p.x() + scale_factor * layout_offset.x();
      p_tr.x() = p_bl.x() + glyph_size.x();

      p_bl.y() = p.y() - scale_factor * layout_offset.y();
      p_tr.y() = p_bl.y() - glyph_size.y();
    }
  else
    {
      p_bl = p + scale_factor * layout_offset;
      p_tr = p_bl + glyph_size;
    }

  pack_raw(attributes(), attrib_loc, dst,
           index_loc, dst_index, p_bl, p_tr);
}

void
fastuidraw::Glyph::
pack_raw(c_array<const GlyphAttribute> glyph_attributes,
         unsigned int attrib_loc, c_array<PainterAttribute> dst,
         unsigned int index_loc, c_array<PainterIndex> dst_index,
         const vec2 p_bl, const vec2 p_tr)
{
  dst = dst.sub_array(attrib_loc, 4);
  dst_index = dst_index.sub_array(index_loc, 6);

  dst_index[0] = attrib_loc + fastuidraw::GlyphAttribute::bottom_left_corner;
  dst_index[1] = attrib_loc + fastuidraw::GlyphAttribute::bottom_right_corner;
  dst_index[2] = attrib_loc + fastuidraw::GlyphAttribute::top_right_corner;
  dst_index[3] = attrib_loc + fastuidraw::GlyphAttribute::bottom_left_corner;
  dst_index[4] = attrib_loc + fastuidraw::GlyphAttribute::top_left_corner;
  dst_index[5] = attrib_loc + fastuidraw::GlyphAttribute::top_right_corner;

  for (unsigned int corner_enum = 0; corner_enum < 4; ++corner_enum)
    {
      pack_glyph_attribute(&dst[corner_enum], corner_enum, p_bl, p_tr, glyph_attributes);
    }
}
