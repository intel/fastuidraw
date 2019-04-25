/*!
 * \file painter_packed_value.hpp
 * \brief file painter_packed_value.hpp
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


#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>
#include <fastuidraw/painter/painter_custom_brush_shader_data.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/backend/painter_item_matrix.hpp>
#include <fastuidraw/painter/backend/painter_clip_equations.hpp>
#include <fastuidraw/painter/backend/painter_brush_adjust.hpp>

namespace fastuidraw
{
  class PainterPackedValuePool;

  template<typename T>
  class PainterPackedValue;

/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * (Private) base class used for PainterPackedValue
   */
  class PainterPackedValueBase
  {
  private:
    friend class PainterPackedValuePool;
    template<typename> friend class PainterPackedValue;

    ~PainterPackedValueBase();
    PainterPackedValueBase(void);
    PainterPackedValueBase(const PainterPackedValueBase&);

    explicit
    PainterPackedValueBase(void*);

    const PainterPackedValueBase&
    operator=(const PainterPackedValueBase&);

    void
    swap(PainterPackedValueBase &obj);

    c_array<generic_data>
    packed_data(void) const;

    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const;

    void *m_d;
  };

  /*!
   * \brief
   * A PainterPackedValue represents a handle to an object that stores
   * packed state data and tracks if that underlying data is already is
   * already copied to PainterDraw::m_store.
   *
   * If already on a store, then rather than copying the data again, the data
   * is reused. The object behind the handle is NOT thread safe. In addition
   * the underlying reference count is not either. Hence any access (even dtor,
   * copy ctor and equality operator) on a fixed object cannot be done from
   * multiple threads simutaneously. A fixed PainterPackedValue can be used
   * by different Painter objects, subject to that the access is done from
   * same thread.
   */
  template<typename T>
  class PainterPackedValue:PainterPackedValueBase
  {
  private:
    typedef void (PainterPackedValue::*unspecified_bool_type)(void);

  public:
    /*!
     * Ctor, initializes handle to nullptr, i.e.
     * no underlying value object.
     */
    PainterPackedValue(void)
    {}

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterPackedValue &obj)
    {
      PainterPackedValueBase::swap(obj);
    }

    /*!
     * Resets the object to not refer to anything.
     * Provided as a conveniance, equivalent to
     * \code
     * PainterPackedValue tmp;
     * swap(tmp);
     * \endcode
     */
    void
    reset(void)
    {
      PainterPackedValue tmp;
      swap(tmp);
    }

    /*!
     * Returns the data of the PainterPackedValue
     */
    c_array<generic_data>
    packed_data(void) const
    {
      return PainterPackedValueBase::packed_data();
    }

    /*!
     * Returns the images needed for binding,
     * only makes sense when T = \ref PainterBrushShaderData
     */
    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const
    {
      return PainterPackedValueBase::bind_images();
    }

    /*!
     * Used to allow using object as a boolean without
     * accidentally converting to a boolean (since it
     * returns a pointer to a function).
     */
    operator
    unspecified_bool_type(void) const
    {
      return this->m_d ? &PainterPackedValue::reset : 0;
    }

    /*!
     * Comparison operator to underlying object
     * storing value.
     * \param rhs handle to which to compare
     */
    bool
    operator==(const PainterPackedValue &rhs) const
    {
      return this->m_d == rhs.m_d;
    }

    /*!
     * Comparison operator to underlying object
     * storing value.
     * \param rhs handle to which to compare
     */
    bool
    operator!=(const PainterPackedValue &rhs) const
    {
      return this->m_d != rhs.m_d;
    }

    /*!
     * Comparison operator to underlying object
     * storing value.
     * \param rhs handle to which to compare
     */
    bool
    operator<(const PainterPackedValue &rhs) const
    {
      return this->m_d < rhs.m_d;
    }

    /*!
     * Pointer to opaque data of PainterPackedValueBase, used
     * internally by fastuidraw. Do not touch!
     */
    void*
    opaque_data(void) const
    {
      return this->m_d;
    }

  private:
    friend class PainterPackedValuePool;

    explicit
    PainterPackedValue(void *d):
      PainterPackedValueBase(d)
    {}
  };

/*! @} */
}
