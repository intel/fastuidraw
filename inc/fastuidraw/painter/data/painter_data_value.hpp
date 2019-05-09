/*!
 * \file painter_data_value.hpp
 * \brief file painter_data_value.hpp
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

#include <fastuidraw/painter/data/painter_packed_value.hpp>

namespace fastuidraw
{
  class PainterPackedValuePool;

/*!\addtogroup PainterShaderData
 * @{
 */

  /*!
   * \brief
   * Element of \ref PainterData to hold shader data either
   * reference directly to unpacked data or to reuseable
   * packed data.
   *
   * Holds both a PainterPackedValue and a pointer to a value.
   * If \ref m_packed_value is valid, then its value is used. If
   * it is nullptr then the value pointed to by \ref m_value is used.
   */
  template<typename T>
  class PainterDataValue
  {
  public:
    /*!
     * Ctor from a value.
     * \param p value with which to initialize, the object pointed
     *          to by p must stay in scope until either make_packed()
     *          is called or the dtor of this \ref PainterDataValue
     *          object is called.
     */
    PainterDataValue(const T *p = nullptr):
      m_value(p)
    {}

    /*!
     * Ctor from a packed value.
     * \param p value with which to initialize \ref m_packed_value
     */
    PainterDataValue(const PainterPackedValue<T> &p):
      m_value(nullptr),
      m_packed_value(p)
    {}

    /*!
     * Only makes sense for \ref PainterBrushShaderData,
     * see \ref PainterBrushShaderData::bind_images().
     */
    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const
    {
      return (m_value) ?
        m_value->bind_images() :
        m_packed_value.bind_images();
    }

    /*!
     * If \ref m_packed_value is null, then sets it
     * to a packed value created by the passed \ref
     * PainterPackedValuePool. In addition, sets
     * \ref m_value to nullptr.
     * \param pool \ref PainterPackedValuePool from
     *             which to create the packed value
     */
    void
    make_packed(PainterPackedValuePool &pool);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * m_packed_value
     * \endcode
     */
    bool
    packed(void) const
    {
      return m_packed_value;
    }

    /*!
     * Pointer to value.
     */
    const T *m_value;

    /*!
     * Value pre-packed and ready for reuse.
     */
    PainterPackedValue<T> m_packed_value;
  };
/*! @} */
}
