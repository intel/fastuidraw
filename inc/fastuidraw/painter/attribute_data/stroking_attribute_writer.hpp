/*!
 * \file stroking_attribute_writer.hpp
 * \brief file stroking_attribute_writer.hpp
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

#include <fastuidraw/path_effect.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader/painter_stroke_shader.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_writer.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterAttribute
 * @{
 */
  /*!
   * \brief
   * A StrokingAttributeWriter is an implementation of \ref
   * PainterAttributeWriter for the purpose of stroking a
   * path.
   */
  class StrokingAttributeWriter:public PainterAttributeWriter
  {
  public:
    /*!
     */
    class StrokingMethod
    {
    public:
      enum PainterEnums::join_style m_js;
      enum StrokedPointPacking::cap_type_t m_cp;
      float m_thresh;
    };

    /*!
     * Ctor, initializes the StrokingAttributeWriter to stroke nothing.
     */
    StrokingAttributeWriter(void);

    ~StrokingAttributeWriter();

    /*!
     * Change the \ref StrokingAttributeWriter to stroke the results of
     * \ref PathEffect
     * \param src the results of a \ref PathEffect. The object given
     *            must stay in scope and its contents remain unchanged
     *            until the next call to set_source().
     * \param shader what \ref PainterStrokeShader to use
     * \param js what join style to apply to the joins
     * \param cp what cap style to apply to the caps
     * \param draw_edges if false, do not stroke the edges, i.e. only stroke the
     *                   caps and joins.
     */
    void
    set_source(const PathEffect::Storage &src,
               const PainterStrokeShader &shader,
               const StrokingMethod &method,
               enum PainterEnums::stroking_method_t tp,
               enum PainterStrokeShader::shader_type_t aa,
               bool draw_edges = true);

    virtual
    bool
    requires_coverage_buffer(void) const override;

    virtual
    unsigned int
    state_length(void) const override;

    virtual
    bool
    initialize_state(WriteState *state) const override;

    virtual
    void
    on_new_store(WriteState *state) const override;

    virtual
    bool
    write_data(c_array<PainterAttribute> dst_attribs,
               c_array<PainterIndex> dst_indices,
               unsigned int attrib_location,
               WriteState *state,
               unsigned int *num_attribs_written,
               unsigned int *num_indices_written) const override;
  private:
    void *m_d;
  };
/*! @} */
}
