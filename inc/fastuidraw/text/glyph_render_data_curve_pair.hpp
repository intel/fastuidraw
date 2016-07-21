/*!
 * \file glyph_render_data_curve_pair.hpp
 * \brief file glyph_render_data_curve_pair.hpp
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


#pragma once

#include <fastuidraw/text/glyph_render_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  /*!
    A GlyphRenderDataCurvePair represents the data needed
    to build a scalable glyph that uses a curve-pair
    analytic algorithm for rendering. A texel can have
    up to two curves intersecting it. If there are two
    curves, then they must be neighbor curves of a contour
    of the generating glyph.The glyphs must be rendered at
    a sufficient resolution so that when rendering glyphs,
    hinting does not play any significant role AND if for
    each texel, if more that one curve intersects a texel,
    then it is only two and those curves are neighbors.

    Data is uploaded to atlas as follows:
       * values in CurvePairGlyphData::m_active_curve_pair are converted
         as follows:
          * CurvePairGlyphData::completely_empty_texel is realized as 0
          * CurvePairGlyphData::completely_full_texel is realized as 1
          * all other values are stored as the original value plus 2
          * if all the converted values are no more than 255, then the valus are
            stored in the texel backing store at Glyph::atlas_location().
          * if atleast one coverted value is atleast 256, then the lower 8-bits
            of each value are in the texel backing store at Glyph::atlas_location()
            and the high 8-bits are stored at Glyph::secondary_atlas_location()
       * The curve pair data is packed into the geometry backing store of the atlas
         with the location a multiple of GlyphAtlasTexelBackingStoreBase::blocks_per_curvepair_entry().
         Each entry is takes up GlyphAtlasTexelBackingStoreBase::blocks_per_curvepair_entry()
         blocks and the values are packed according to the enumeration CurvePairGlyphData::geometry_packing.
         In addition, the value for CurvePairGlyphData::entry::m_p is incremented by
         Glyph::atlas_location().location(). By doing so, the computation of coverage
         needs only the texel coordinate (because the curve data is effectively moved to
         the coordinates of texel coordinates).
   */
  class GlyphRenderDataCurvePair:public GlyphRenderData
  {
  public:
    /*!
      A per_curve represents the "raw" data for
      a single curve (linear or quadratic).
     */
    class per_curve
    {
    public:
      //@{
      /**
        Linear coefficients for parobola rotated used
        to compute pseudo-distance to curve
       */
      float m_m0, m_m1;
      //@}

      /*!
        Rotation for curve to represent curve
        as a parabola aligned to the coordiante axis.
       */
      vec2 m_q;

      /*!
        Quadratic scale factor needed.
       */
      float m_quad_coeff;
    };

    /*!
      Enumeration to describe if a texel has curves
      or if not if the texel is completely inside or
      outside.
     */
    enum entry_type
      {
        /*!
          Indicates that an entry has curve data and is
          only partially covered by the inside of an outline.
         */
        entry_has_curves,

        /*!
          Indicates that an entry has no curve data and is
          completely covered by the inside of an outline.
         */
        entry_completely_covered,

        /*!
          Indicates that an entry has no curve data and is
          completely uncovered by the inside of an outline.
         */
        entry_completely_uncovered
      };

    /*!
      Represents the data of two neighboring curves in the outline of a glyph.
      TODO: document how to compute pseudo-distance from value in entry class
     */
    class entry
    {
    public:
      /*!
        Ctor. Construct an entry from curve data.
        \param pts is an array of the data used by the curve
        \param curve0_count must be 2 or 3; the curve into the common point (given by
                            m_curve0 is held in pts.sub_array(0, curve0_count). A value of
                            2 indicates a flat edge; a value of 3 indicates a degree 2
                            bezier curve. The curve coming out of the common point is given
                            by pts.sub_array(curve0_count - 1) (since its start point
                            is the end point of curve going in).
       */
      entry(const_c_array<vec2> pts, int curve0_count);

      /*!
        Construct an entry indicating that a point is always "inside"
        or "outside", i.e. the curves created always return the same
        value.
        \param inside if true pseudo distance returns always positive value,
                      if false, pseudo distance returns always negative value.
       */
      explicit
      entry(bool inside);

      /*!
        The ending point of the curve 0 and
        the starting point of curve 1.
       */
      vec2 m_p;

      //@{
      /**
        The curves of the entry
       */
      per_curve m_curve0, m_curve1;
      //@}

      /*!
        Is true, when computing coverage of a point
        take the max of the pseduo-distance of
        the curves from the point, otherwise take the
        min.
       */
      bool m_use_min;

      /*!
        Represents the pseudo-distance value to use
        for each curve if the point is not in the
        shadow of the curve.
       */
      float m_zeta;

      /*!
        The type of the entry.
       */
      enum entry_type m_type;
    };

  enum
    {
      /*!
        A value for active_curve_pair() indicating
        that there are no curves inside the texel and
        the texel is completely inside the glyph
       */
      completely_full_texel = 0xFFFF,

      /*!
        A value for active_curve_pair() indicating
        that there are no curves inside the texel and
        the texel is completely outside the glyph
       */
      completely_empty_texel = 0xFFFE
    };

    /*!
      Sequence of enumerations describing how each entry of
      geometry_data() is packed into geometry data of
      Glyph.
    */
    enum geometry_packing
      {
        pack_offset_p_x, /*!< offset for entry::m_p.x() */
        pack_offset_p_y, /*!< offset for entry::m_p.y() */
        pack_offset_zeta, /*!< offset for entry::m_zeta */
        /*!
          offset to entry::m_use_min encoded as
            false --> 0.0
            true  --> 1.0
         */
        pack_offset_combine_rule,
        pack_offset_curve0_m0, /*!< offset for entry::m_curve0.m_m0 */
        pack_offset_curve0_m1, /*!< offset for entry::m_curve0.m_m1 */
        pack_offset_curve0_q_x, /*!< offset for entry::m_curve0.m_q.x() */
        pack_offset_curve0_q_y, /*!< offset for entry::m_curve0.m_q.y() */
        pack_offset_curve0_quad_coeff, /*!< offset for entry::m_curve0.m_quad_coeff */
        pack_offset_curve1_m0, /*!< offset for entry::m_curve1.m_m0 */
        pack_offset_curve1_m1, /*!< offset for entry::m_curve1.m_m1 */
        pack_offset_curve1_q_x, /*!< offset for entry::m_curve1.m_q.x() */
        pack_offset_curve1_q_y, /*!< offset for entry::m_curve1.m_q.y() */
        pack_offset_curve1_quad_coeff, /*!< offset for entry::m_curve1.m_quad_coeff */

        number_elements_to_pack
      };


    /*!
      Ctor, initialized the resolution as (0,0) and the
      geometry data to be empty.
     */
    GlyphRenderDataCurvePair(void);

    ~GlyphRenderDataCurvePair();

    /*!
      Returns the resolution of the glyph including
      padding. The padding is to be 1 pixel wide
      on the bottom and on the right, i.e.
      GlyphAtlas::Padding::m_right = GlyphAtlas::Padding::m_bottom = 1
      and GlyphAtlas::Padding::m_left = GlyphAtlas::Padding::m_top = 0.
     */
    ivec2
    resolution(void) const;

    /*!
      The texel value at (x,y) is stored
      at location I where
      I = x + y * resolution().x().
      The texel value is an index into
      geometry_data() of the data
      needed to render the glyph at that
      location.
      Two values are reserved:
       - completely_full_texel indicates that texel
         has no curves through it and is inside the glyph
       - completely_empty_texel indicates that texel
         has no curves through it and is outside the glyph
     */
    const_c_array<uint16_t>
    active_curve_pair(void) const;

    /*!
      The texel value at (x,y) is stored
      at location I where
      I = x + y * resolution().x().
      The texel value is an index into
      geometry_data() of the data
      needed to render the glyph at that
      location.
      Two values are reserved:
       - completely_full_texel indicates that texel
         has no curves through it and is inside the glyph
       - completely_empty_texel indicates that texel
         has no curves through it and is outside the glyph
     */
    c_array<uint16_t>
    active_curve_pair(void);

    /*!
      Represents all the geometry data of a
      CurvePairGlyphData needed to render it.
     */
    const_c_array<entry>
    geometry_data(void) const;

    /*!
      Represents all the geometry data of a
      CurvePairGlyphData needed to render it.
     */
    c_array<entry>
    geometry_data(void);

    /*!
      Change the resolution
      \param sz new resolution size
     */
    void
    resize_active_curve_pair(ivec2 sz);

    /*!
      Resize the size of geometry_data()
      \param sz new size for geometry_data()
     */
    void
    resize_geometry_data(int sz);

    virtual
    enum fastuidraw::return_code
    upload_to_atlas(const reference_counted_ptr<GlyphAtlas> &atlas,
                    GlyphLocation &atlas_location,
                    GlyphLocation &secondary_atlas_location,
                    int &geometry_offset,
                    int &geometry_length) const;

  private:
    void *m_d;
  };
/*! @} */
}
