/*!
 * \file glyph_attribute_packer.hpp
 * \brief file glyph_attribute_packer.hpp
 *
 * Copyright 2019 by Intel.
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

#include <fastuidraw/text/glyph.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * A GlyphAttributePacker provides an interface to customize
   * how glyph attribute and index data is realized by a \ref
   * GlyphRun and \ref GlyphSequence.
   */
  class GlyphAttributePacker:public reference_counted<GlyphAttributePacker>::concurrent
  {
  public:
    virtual
    ~GlyphAttributePacker()
    {}

    /*!
     * To be implemented by a derived class to return
     * the positions of the bottom-left and top-right
     * coordinates of a glyph at the named position
     * using only the information from \ref GlyphMetrics.
     * \param metrics GlyphMertics to use to compute the bounding
     *                box
     * \param position position of bottom left of glyph
     * \param scale_factor the amount by which to scale from the
     *                     \ref Glyph object's coordinates
     * \param[out] p_bl location to which to write the bottom-left
     *                  coordinate of the glyph
     * \param[out] p_tr location to which to write the top-right
     *                  coordinate of the glyph
     */
    virtual
    void
    glyph_position_from_metrics(GlyphMetrics metrics,
                                const vec2 position, float scale_factor,
                                vec2 *p_bl, vec2 *p_tr) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the positions of the bottom-left and top-right
     * coordinates of a glyph at the named position.
     * \param glyph Glyph to realize
     * \param position position of bottom left of glyph
     * \param scale_factor the amount by which to scale from the
     *                     \ref Glyph object's coordinates
     * \param[out] p_bl location to which to write the bottom-left
     *                  coordinate of the glyph
     * \param[out] p_tr location to which to write the top-right
     *                  coordinate of the glyph
     */
    virtual
    void
    glyph_position(Glyph glyph,
                   const vec2 position, float scale_factor,
                   vec2 *p_bl, vec2 *p_tr) const = 0;

    /*!
     * To be implemented by a derived class to return
     * how many indices and attributes are needed to
     * realize a single glyph.
     * \param glyph_renderer \ref Glyph::renderer() of \ref Glyph to realize
     * \param glyph_attributes \ref Glyph::attributes() of \ref Glyph to realize
     * \param[out] out_num_indices number of indices needed
     *                             to realize the glyph
     * \param[out] out_num_attributes number of attributes
     *                                needed to realize the glyph
     */
    virtual
    void
    compute_needed_room(GlyphRenderer glyph_renderer,
                        c_array<const GlyphAttribute> glyph_attributes,
                        unsigned int *out_num_indices,
                        unsigned int *out_num_attributes) const = 0;

    /*!
     * To be implemented by a derived class to provide the
     * attributes and indices to realize a glyph.
     * \param glyph_renderer \ref Glyph::renderer() of \ref Glyph to realize
     * \param glyph_attributes \ref Glyph::attributes() of \ref Glyph to realize
     * \param dst_indices location to which to write the indices;
     *                    the values are offsets into dst_attribs
     * \param dst_attribs location to which to write the attributes
     * \param p_bl position of bottom left of glyph as emited by
     *             \ref glyph_position()
     * \param p_tr position of top right of glyph as emited by
     *             \ref glyph_position()
     */
    virtual
    void
    realize_attribute_data(GlyphRenderer glyph_renderer,
                           c_array<const GlyphAttribute> glyph_attributes,
                           c_array<PainterIndex> dst_indices,
                           c_array<PainterAttribute> dst_attribs,
                           const vec2 p_bl, const vec2 p_tr) const = 0;

    /*!
     * Returns a \ref GlyphAttributePacker suitable for the
     * specified \ref PainterEnums::screen_orientation and \ref
     * PainterEnums::glyph_layout_type that packs each single
     * \ref Glyph object as exactly 4 attributes and 6 indices as
     * follows:
     *   - PainterAttribute::m_attrib0 .xy -> position in item coordinates of the
     *                                        vertex of the quad to draw the glyph (float)
     *   - PainterAttribute::m_attrib0 .zw -> the difference in item coordinates
     *                                        between the bottom-left vertex position
     *                                        and the top-right vertex position.
     *   - PainterAttribute::m_attrib1 .x  -> Glyph::attribute()[0]
     *   - PainterAttribute::m_attrib1 .y  -> Glyph::attribute()[1]
     *   - PainterAttribute::m_attrib1. z  -> Glyph::attribute()[2]
     *   - PainterAttribute::m_attrib1 .w  -> Glyph::attribute()[3]
     *   - PainterAttribute::m_attrib2 .x  -> Glyph::attribute()[4]
     *   - PainterAttribute::m_attrib2 .y  -> Glyph::attribute()[5]
     *   - PainterAttribute::m_attrib2 .z  -> Glyph::attribute()[6]
     *   - PainterAttribute::m_attrib2 .w  -> Glyph::attribute()[7]
     * \param orientation screen orientation to use
     * \param layout glyph layout to use
     */
    static
    const GlyphAttributePacker&
    standard_packer(enum PainterEnums::screen_orientation orientation,
                    enum PainterEnums::glyph_layout_type layout);
  };

/*! @} */
}
