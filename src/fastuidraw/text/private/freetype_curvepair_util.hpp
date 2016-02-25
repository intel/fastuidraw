/*!
 * \file freetype_curvepair_util.hpp
 * \brief file freetype_curvepair_util.hpp
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

#include "freetype_util.hpp"

namespace fastuidraw
{
  class GlyphLayoutData;
  class GlyphRenderDataCurvePair;

namespace detail
{
  class CurvePairGenerator
  {
  public:
    /* ctor needs access to FT_Outline
       \param outline FT_Outline from which to generate values
       \param bitmap_sz the size of the bitmap that freetype would use
                        to render the FT_Outline. This size is also
                        the size used to create the index data
       \param bitmap_offset the offset of the FT_Outline that freetype
                            would use when rendering
     NOTE: One can use other values for bitmap_sz and bitmap_offset,
           just make sure the transformations and target texel size
           is what you want!
     */
    explicit
    CurvePairGenerator(FT_Outline outline, ivec2 bitmap_sz, ivec2 bitmap_offset,
                       GlyphRenderDataCurvePair &output);

    ~CurvePairGenerator();

    /* but actual extraction does not.
     */
    void
    extract_data(GlyphRenderDataCurvePair &output);

  private:
    void *m_contour_emitter;
    void *m_outline_data;
    std::vector<point_type> m_pts;
  };



} //namesapce fastuidraw
} //namespace detail
