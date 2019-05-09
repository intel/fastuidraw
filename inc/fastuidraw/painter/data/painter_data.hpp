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

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/painter/data/painter_data_value.hpp>
#include <fastuidraw/painter/data/painter_shader_data.hpp>
#include <fastuidraw/painter/data/painter_brush_shader_data.hpp>
#include <fastuidraw/painter/painter_custom_brush.hpp>
#include <fastuidraw/painter/painter_brush.hpp>

namespace fastuidraw
{
  class PainterPackedValuePool;

/*!\addtogroup PainterShaderData
 * @{
 */

  /*!
   * \brief
   * A PainterData provides the data for how for a
   * Painter to draw content.
   */
  class PainterData
  {
  public:
    /*!
     * \brief
     * A brush_value stores the brush applied; it stores
     * a pointer to a \ref PainterBrushShader together
     * with a PainterDataValue<PainterBrushShaderData>.
     * If the pointer to the \ref PainterBrushShader is null,
     * then it indicates to use the standard brush shader,
     * \ref PainterBrushShaderSet::standard_brush().
     */
    class brush_value
    {
    public:
      /*!
       * Empty ctor that initializes to not have a brush source
       * (custom or \ref PainterBrush).
       */
      brush_value(void):
        m_brush_shader(nullptr)
      {}

      /*!
       * Ctor to set the brush_value to source from a
       * \ref PainterBrush.
       */
      brush_value(const PainterBrush *v)
      {
        set(v);
      }

      /*!
       * Ctor to set the brush_value to source from a
       * custom brush.
       */
      brush_value(const PainterCustomBrush &br)
      {
        set(br);
      }

      /*!
       * Ctor to set the brush_value to source from a
       * \ref PainterBrush packed.
       */
      brush_value(const PainterDataValue<PainterBrushShaderData> &brush_data)
      {
        set(brush_data);
      }

      /*!
       * Set to source from a \ref PainterBrush
       */
      void
      set(const PainterBrush *v)
      {
        FASTUIDRAWassert(v);
        m_brush_shader_data = v;
        m_brush_shader = nullptr;
      }

      /*!
       * Set to source from a custom brush shader
       */
      void
      set(const PainterCustomBrush &br)
      {
        m_brush_shader_data = br.m_data;
        m_brush_shader = br.m_shader;
      }

      /*!
       * Set to source from a packed PainterBrush value.
       */
      void
      set(const PainterDataValue<PainterBrushShaderData> &brush_data)
      {
        m_brush_shader_data = brush_data;
        m_brush_shader = nullptr;
      }

      /*!
       * Returns the \ref PainterBrushShader for the brush;
       * a value of nullptr indicates to used the default
       * brush that processes \ref PainterBrush data.
       */
      const PainterBrushShader*
      brush_shader(void) const
      {
        return m_brush_shader;
      }

      /*!
       * Returns the PainterDataValue<PainterBrushShaderData> holding
       * the brush data.
       */
      const PainterDataValue<PainterBrushShaderData>&
      brush_shader_data(void) const
      {
        return m_brush_shader_data;
      }

      /*!
       * Packs the brush shader data.
       */
      void
      make_packed(PainterPackedValuePool &pool)
      {
        m_brush_shader_data.make_packed(pool);
      }

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * brush_shader_data().packed().
       * \endcode
       */
      bool
      packed(void) const
      {
        return m_brush_shader_data.packed();
      }

    private:
      /*!
       * The \ref value for the brush data
       */
      PainterDataValue<PainterBrushShaderData> m_brush_shader_data;

      /*!
       * If non-null, indicates that the brush is a realized
       * by a custom brush shader.
       */
      const PainterBrushShader *m_brush_shader;
    };

    /*!
     * Ctor. Intitializes all fields as default nothings.
     */
    PainterData(void)
    {}

    /*!
     * Ctor to initialize one field.
     * \param r1 calls one of the set() functions relying on C++
     *           conversion and template logic to select the correct
     *           field to set.
     */
    template<typename T1>
    PainterData(const T1 &r1)
    {
      set(r1);
    }

    /*!
     * Ctor to initialize two fields.
     * \param r1 calls one of the set() functions relying on C++
     *           conversion and template logic to select the correct
     *           field to set.
     * \param r2 calls one of the set() functions relying on C++
     *           conversion and template logic to select the correct
     *           field to set.
     */
    template<typename T1, typename T2>
    PainterData(const T1 &r1, const T2 &r2)
    {
      set(r1);
      set(r2);
    }

    /*!
     * Ctor to initialize three fields.
     * \param r1 calls one of the set() functions relying on C++
     *           conversion and template logic to select the correct
     *           field to set.
     * \param r2 calls one of the set() functions relying on C++
     *           conversion and template logic to select the correct
     *           field to set.
     * \param r3 calls one of the set() functions relying on C++
     *           conversion and template logic to select the correct
     *           field to set.
     */
    template<typename T1, typename T2, typename T3>
    PainterData(const T1 &r1, const T2 &r2, const T3 &r3)
    {
      set(r1);
      set(r2);
      set(r3);
    }

    /*!
     * value for brush (fixed-function or custom brush shading).
     */
    brush_value m_brush;

    /*!
     * value for item shader data
     */
    PainterDataValue<PainterItemShaderData> m_item_shader_data;

    /*!
     * value for blend shader data
     */
    PainterDataValue<PainterBlendShaderData> m_blend_shader_data;

    /*!
     * Sets \ref m_brush
     */
    PainterData&
    set(const brush_value &value)
    {
      m_brush = value;
      return *this;
    }

    /*!
     * Sets \ref m_brush
     */
    PainterData&
    set(const PainterBrush *value)
    {
      m_brush = value;
      return *this;
    }

    /*!
     * Sets \ref m_brush
     */
    PainterData&
    set(const PainterCustomBrush &value)
    {
      m_brush.set(value);
      return *this;
    }

    /*!
     * Sets \ref m_item_shader_data
     */
    PainterData&
    set(const PainterDataValue<PainterItemShaderData> &value)
    {
      m_item_shader_data = value;
      return *this;
    }

    /*!
     * Sets \ref m_blend_shader_data
     */
    PainterData&
    set(const PainterDataValue<PainterBlendShaderData> &value)
    {
      m_blend_shader_data = value;
      return *this;
    }

    /*!
     * Call value::make_packed() on \ref m_brush,
     * \ref m_item_shader_data, and \ref
     * m_blend_shader_data.
     * \param pool \ref PainterPackedValuePool from
     *             which to create the packed value
     */
    void
    make_packed(PainterPackedValuePool &pool)
    {
      m_brush.make_packed(pool);
      m_item_shader_data.make_packed(pool);
      m_blend_shader_data.make_packed(pool);
    }
  };

/*! @} */
}
