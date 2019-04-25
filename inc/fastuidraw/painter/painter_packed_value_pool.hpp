/*!
 * \file painter_packed_value_pool.hpp
 * \brief file painter_packed_value_pool.hpp
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

#include <fastuidraw/painter/painter_data.hpp>
#include <fastuidraw/painter/painter_packed_value.hpp>

namespace fastuidraw
{

  /*!
   * \brief
   * A PainterPackedValuePool can be used to create PainterPackedValue
   * objects.
   *
   * Just like PainterPackedValue, PainterPackedValuePool is
   * NOT thread safe, as such it is not a safe operation to use the
   * same PainterPackedValuePool object from multiple threads at the
   * same time. A fixed PainterPackedValuePool can create \ref
   * PainterPackedValue objects used by different \ref Painter objects.
   */
  class PainterPackedValuePool:noncopyable
  {
  public:
    /*!
     * Ctor.
     */
    explicit
    PainterPackedValuePool(void);

    ~PainterPackedValuePool();

    /*!
     * Create and return a PainterPackedValue<PainterItemShaderData>
     * object for the value of a PainterItemShaderData object.
     * \param value data to pack into returned PainterPackedValue
     */
    PainterPackedValue<PainterItemShaderData>
    create_packed_value(const PainterItemShaderData &value);

    /*!
     * Create and return a PainterPackedValue<PainterBlendShaderData>
     * object for the value of a PainterBlendShaderData object.
     * \param value data to pack into returned PainterPackedValue
     */
    PainterPackedValue<PainterBlendShaderData>
    create_packed_value(const PainterBlendShaderData &value);

    /*!
     * Create and return a PainterPackedValue<PainterBrushShaderData>
     * object for the value of a PainterBrushShaderData object.
     * \param value data to pack into returned PainterPackedValue
     */
    PainterPackedValue<PainterBrushShaderData>
    create_packed_value(const PainterBrushShaderData &value);

    /*!
     * Returns a \ref PainterData::brush_value whose data is
     * packed from a \ref PainterBrush value.
     */
    PainterData::brush_value
    create_packed_brush(const PainterBrush &brush)
    {
      PainterData::brush_value R(&brush);
      R.make_packed(*this);
      return R;
    }

    /*!
     * Returns a \ref PainterData::brush_value whose data is
     * packed from a \ref CustomBrush value.
     */
    PainterData::brush_value
    create_packed_brush(const PainterData::CustomBrush &brush)
    {
      PainterData::brush_value R(brush);
      R.make_packed(*this);
      return R;
    }

  private:
    void *m_d;
  };

/*! @} */

  template<typename T>
  void
  PainterData::value<T>::
  make_packed(PainterPackedValuePool &pool)
  {
    if (!m_packed_value && m_value != nullptr)
      {
        m_packed_value = pool.create_packed_value(*m_value);
        m_value = nullptr;
      }
  }

} //namespace
