/*!
 * \file painter_data.hpp
 * \brief file painter_data.hpp
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

#include <fastuidraw/painter/painter_packed_value.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterData provides the data for how for a
    Painter to draw content.
   */
  class PainterData
  {
  public:
    /*!
      Holds both a PainterPackedValue and a pointer to a value.
      If m_packed_value is valid, then its value is used. If
      it is NULL and m_value is non-NULL, then the value
      behind m_value is used. Otherwise a value made from
      default ctor of T is used.
     */
    template<typename T>
    class value
    {
    public:
      /*!
        Ctor from a value value.
        \param p value with which to initialize m_value
       */
      value(const T *p = NULL):
        m_value(p)
      {}

      /*!
        Ctor from a packed value.
        \param p value with which to initialize m_packed_value
       */
      value(const PainterPackedValue<T> &p):
        m_value(NULL),
        m_packed_value(p)
      {}

      /*!
        Pointer to value.
       */
      const T *m_value;

      /*!
        Value pre-packed and ready for reuse.
       */
      PainterPackedValue<T> m_packed_value;

      const T&
      data(void) const
      {
        return m_packed_value ?
          m_packed_value.value() :
          *m_value;
      }
    };

    /*!
      Ctor. Intitializes all fields as default nothings.
     */
    PainterData(void)
    {}

    /*!
      Ctor to initialize one field.
      \param r1 calls one of the set() functions relying on C++
                conversion and template logic to select the correct
                field to set.
     */
    template<typename T1>
    PainterData(const T1 &r1)
    {
      set(r1);
    }

    /*!
      Ctor to initialize two fields.
      \param r1 calls one of the set() functions relying on C++
                conversion and template logic to select the correct
                field to set.
      \param r2 calls one of the set() functions relying on C++
                conversion and template logic to select the correct
                field to set.
     */
    template<typename T1, typename T2>
    PainterData(const T1 &r1, const T2 &r2)
    {
      set(r1);
      set(r2);
    }

    /*!
      Ctor to initialize three fields.
      \param r1 calls one of the set() functions relying on C++
                conversion and template logic to select the correct
                field to set.
      \param r2 calls one of the set() functions relying on C++
                conversion and template logic to select the correct
                field to set.
      \param r3 calls one of the set() functions relying on C++
                conversion and template logic to select the correct
                field to set.
     */
    template<typename T1, typename T2, typename T3>
    PainterData(const T1 &r1, const T2 &r2, const T3 &r3)
    {
      set(r1);
      set(r2);
      set(r3);
    }

    /*!
      value for brush
     */
    value<PainterBrush> m_brush;

    /*!
      value for item shader data
    */
    value<PainterItemShaderData> m_item_shader_data;

    /*!
      value for blend shader data
    */
    value<PainterBlendShaderData> m_blend_shader_data;

    /*!
      Sets \ref m_brush
     */
    PainterData&
    set(const value<PainterBrush> &value)
    {
      m_brush = value;
      return *this;
    }

    /*!
      Sets \ref m_item_shader_data
     */
    PainterData&
    set(const value<PainterItemShaderData> &value)
    {
      m_item_shader_data = value;
      return *this;
    }

    /*!
      Sets \ref m_blend_shader_data
     */
    PainterData&
    set(const value<PainterBlendShaderData> &value)
    {
      m_blend_shader_data = value;
      return *this;
    }
  };

/*! @} */
}
