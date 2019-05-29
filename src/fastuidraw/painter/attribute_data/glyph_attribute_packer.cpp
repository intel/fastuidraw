/*!
 * \file glyph_attribute_packer.cpp
 * \brief file glyph_attribute_packer.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <fastuidraw/painter/attribute_data/glyph_attribute_packer.hpp>

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

  class Packer:public fastuidraw::GlyphAttributePacker
  {
  public:
    Packer(enum fastuidraw::PainterEnums::screen_orientation orientation,
           enum fastuidraw::PainterEnums::glyph_layout_type layout):
      m_orientation(orientation),
      m_layout(layout)
    {}

    void
    glyph_position_from_metrics(fastuidraw::GlyphMetrics metrics,
                                const fastuidraw::vec2 position, float scale_factor,
                                fastuidraw::vec2 *p_bl, fastuidraw::vec2 *p_tr) const override;

    void
    glyph_position(fastuidraw::Glyph glyph,
                   const fastuidraw::vec2 position, float scale_factor,
                   fastuidraw::vec2 *p_bl, fastuidraw::vec2 *p_tr) const override;

    void
    compute_needed_room(fastuidraw::GlyphRenderer glyph_renderer,
                        fastuidraw::c_array<const fastuidraw::GlyphAttribute> glyph_attributes,
                        unsigned int *out_num_indices,
                        unsigned int *out_num_attributes) const override;

    void
    realize_attribute_data(fastuidraw::GlyphRenderer glyph_renderer,
                           fastuidraw::c_array<const fastuidraw::GlyphAttribute> glyph_attributes,
                           fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                           fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                           const fastuidraw::vec2 p_bl,
                           const fastuidraw::vec2 p_tr) const override;

  private:
    void
    compute_box_helper(fastuidraw::GlyphMetrics metrics,
                       fastuidraw::vec2 p, float scale_factor, fastuidraw::vec2 render_size,
                       fastuidraw::vec2 *p_bl, fastuidraw::vec2 *p_tr) const;

    enum fastuidraw::PainterEnums::screen_orientation m_orientation;
    enum fastuidraw::PainterEnums::glyph_layout_type m_layout;
  };

  class StandardPackerHolder:fastuidraw::noncopyable
  {
  public:
    static
    const fastuidraw::GlyphAttributePacker&
    packer(enum fastuidraw::PainterEnums::screen_orientation orientation,
           enum fastuidraw::PainterEnums::glyph_layout_type layout);

  private:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::GlyphAttributePacker> ref;
    typedef fastuidraw::vecN<ref, fastuidraw::PainterEnums::number_screen_orientation> PerOrientation;

    StandardPackerHolder(void);

    fastuidraw::vecN<PerOrientation, fastuidraw::PainterEnums::number_glyph_layout> m_packers;
  };
}

///////////////////////////////////////
// Packer methods
void
Packer::
compute_box_helper(fastuidraw::GlyphMetrics metrics,
                   fastuidraw::vec2 p, float scale_factor, fastuidraw::vec2 size,
                   fastuidraw::vec2 *ptr_bl, fastuidraw::vec2 *ptr_tr) const
{
  using namespace fastuidraw;

  vec2 glyph_size(scale_factor * size);
  vec2 layout_offset(0.0f, 0.0f);
  vec2 &p_bl(*ptr_bl);
  vec2 &p_tr(*ptr_tr);

  FASTUIDRAWassert(metrics.valid());
  layout_offset = (m_layout == PainterEnums::glyph_layout_horizontal) ?
    metrics.horizontal_layout_offset() :
    metrics.vertical_layout_offset();

  if (m_orientation == PainterEnums::y_increases_downwards)
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
}

void
Packer::
glyph_position_from_metrics(fastuidraw::GlyphMetrics metrics,
                            fastuidraw::vec2 position, float scale_factor,
                            fastuidraw::vec2 *p_bl, fastuidraw::vec2 *p_tr) const
{
  compute_box_helper(metrics, position, scale_factor,
                     metrics.size(), p_bl, p_tr);
}

void
Packer::
glyph_position(fastuidraw::Glyph glyph,
               const fastuidraw::vec2 p, float scale_factor,
               fastuidraw::vec2 *p_bl, fastuidraw::vec2 *p_tr) const
{
  FASTUIDRAWassert(glyph.valid());
  compute_box_helper(glyph.metrics(), p, scale_factor,
                     glyph.render_size(), p_bl, p_tr);
}

void
Packer::
compute_needed_room(fastuidraw::GlyphRenderer glyph_renderer,
                    fastuidraw::c_array<const fastuidraw::GlyphAttribute> glyph_attributes,
                    unsigned int *out_num_indices,
                    unsigned int *out_num_attributes) const
{
  FASTUIDRAWunused(glyph_renderer);
  FASTUIDRAWunused(glyph_attributes);
  *out_num_indices = 6;
  *out_num_attributes = 4;
}

void
Packer::
realize_attribute_data(fastuidraw::GlyphRenderer glyph_renderer,
                       fastuidraw::c_array<const fastuidraw::GlyphAttribute> glyph_attributes,
                       fastuidraw::c_array<fastuidraw::PainterIndex> dst_index,
                       fastuidraw::c_array<fastuidraw::PainterAttribute> dst,
                       const fastuidraw::vec2 p_bl,
                       const fastuidraw::vec2 p_tr) const
{
  using namespace fastuidraw;
  FASTUIDRAWunused(glyph_renderer);

  dst_index[0] = GlyphAttribute::bottom_left_corner;
  dst_index[1] = GlyphAttribute::bottom_right_corner;
  dst_index[2] = GlyphAttribute::top_right_corner;
  dst_index[3] = GlyphAttribute::bottom_left_corner;
  dst_index[4] = GlyphAttribute::top_left_corner;
  dst_index[5] = GlyphAttribute::top_right_corner;
  for (unsigned int corner_enum = 0; corner_enum < 4; ++corner_enum)
    {
      pack_glyph_attribute(&dst[corner_enum], corner_enum, p_bl, p_tr, glyph_attributes);
    }
}

///////////////////////////////////////////
// StandardPackerHolder methods
StandardPackerHolder::
StandardPackerHolder(void)
{
  using namespace fastuidraw;
  for (unsigned int orientation = 0; orientation < PainterEnums::number_screen_orientation; ++orientation)
    {
      for (unsigned int layout = 0; layout < PainterEnums::number_glyph_layout; ++layout)
        {
          m_packers[orientation][layout] = FASTUIDRAWnew Packer(static_cast<enum PainterEnums::screen_orientation>(orientation),
                                                                static_cast<enum PainterEnums::glyph_layout_type>(layout));
        }
    }
}

const fastuidraw::GlyphAttributePacker&
StandardPackerHolder::
packer(enum fastuidraw::PainterEnums::screen_orientation orientation,
       enum fastuidraw::PainterEnums::glyph_layout_type layout)
{
  static StandardPackerHolder H;
  return *H.m_packers[orientation][layout];
}

////////////////////////////////////////////
// fastuidraw::GlyphAttributePacker methods
const fastuidraw::GlyphAttributePacker&
fastuidraw::GlyphAttributePacker::
standard_packer(enum PainterEnums::screen_orientation orientation,
                enum PainterEnums::glyph_layout_type layout)
{
  return StandardPackerHolder::packer(orientation, layout);
}
