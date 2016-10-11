/*!
 * \file painter_attribute_data_filler_path_stroked.hpp
 * \brief painter_attribute_data_filler_path_stroked.hpp
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

#include <fastuidraw/painter/painter_attribute_data_filler.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_stroke_shader.hpp>
#include <fastuidraw/stroked_path.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterAttributeDataFillerPathEdges is an implementation for
    PainterAttributeDataFiller to construct index and attribute
    data to draw the edges of a stroked path. The data is so that
    it provides all that is needed to stroke a path regardless of
    stroking width or even dash pattern. The StrokedPath::point
    valus from the source StrokedPath::Edges are packed via
    \ref point::pack_point().
   */
  class PainterAttributeDataFillerPathEdges:public PainterAttributeDataFiller
  {
  public:
    enum
      {
        without_closing_edge,
        with_closing_edge
      };

    /*!
      Ctor.
      \param edges StrokedPath::Edges from which to construct attribute and index data.
     */
    explicit
    PainterAttributeDataFillerPathEdges(const StrokedPath::Edges &edges);

    ~PainterAttributeDataFillerPathEdges();

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(c_array<PainterAttribute> attributes,
              c_array<PainterIndex> indices,
              c_array<const_c_array<PainterAttribute> > attrib_chunks,
              c_array<const_c_array<PainterIndex> > index_chunks,
              c_array<unsigned int> zincrements) const;

    /*!
      Returns the StrokedPath::Edges from which this object fills
      data (set at ctor).
     */
    const StrokedPath::Edges&
    edges(void) const;

  private:
    void *m_d;
  };

  /*!
    A PainterAttributeDataFillerPathCaps is an implementation for
    PainterAttributeDataFiller to construct index and attribute
    data to draw the edges of a stroked path. The data is so that
    it provides all that is needed to stroke a path regardless of
    stroking width or even dash pattern. The StrokedPath::point
    valus from the source StrokedPath::Caps are packed via
    \ref point::pack_point().
   */
  class PainterAttributeDataFillerPathCaps:public PainterAttributeDataFiller
  {
  public:
    /*!
      Ctor.
      \param caps StrokedPath::Caps from which to construct attribute and index data.
     */
    explicit
    PainterAttributeDataFillerPathCaps(const StrokedPath::Caps &caps);

    ~PainterAttributeDataFillerPathCaps();

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(c_array<PainterAttribute> attributes,
              c_array<PainterIndex> indices,
              c_array<const_c_array<PainterAttribute> > attrib_chunks,
              c_array<const_c_array<PainterIndex> > index_chunks,
              c_array<unsigned int> zincrements) const;

    /*!
      Returns the StrokedPath::Caps from which this object fills
      data (set at ctor).
     */
    const StrokedPath::Caps&
    caps(void) const;

  private:
    void *m_d;
  };

  /*!
    A PainterAttributeDataFillerPathJoins is an implementation for
    PainterAttributeDataFiller to construct index and attribute
    data to draw the edges of a stroked path. The data is so that
    it provides all that is needed to stroke a path regardless of
    stroking width or even dash pattern. The StrokedPath::point
    valus from the source StrokedPath::Joins are packed via
    \ref point::pack_point().
   */
  class PainterAttributeDataFillerPathJoins:public PainterAttributeDataFiller
  {
  public:
    enum
      {
        without_closing_edge,
        with_closing_edge,
        specific_join0,
      };

    /*!
      Ctor.
      \param joins StrokedPath::Joins from which to construct attribute and index data.
     */
    explicit
    PainterAttributeDataFillerPathJoins(const StrokedPath::Joins &joins);

    ~PainterAttributeDataFillerPathJoins();

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(c_array<PainterAttribute> attributes,
              c_array<PainterIndex> indices,
              c_array<const_c_array<PainterAttribute> > attrib_chunks,
              c_array<const_c_array<PainterIndex> > index_chunks,
              c_array<unsigned int> zincrements) const;

    /*!
      Returns the StrokedPath::Joins from which this object fills
      data (set at ctor).
     */
    const StrokedPath::Joins&
    joins(void) const;

    /*!
      Implementation for DashEvaluatorBase::number_joins().
     */
    static
    unsigned int
    number_joins(const PainterAttributeData &data, bool edge_closed);

    /*!
      Implementation for DashEvaluatorBase::named_join_chunk().
     */
    static
    unsigned int
    named_join_chunk(unsigned int J);

  private:
    void *m_d;
  };
/*! @} */
}
