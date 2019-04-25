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
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

namespace fastuidraw
{
  class PainterPackedValuePool;

/*!\addtogroup Painter
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
     * Holds both a PainterPackedValue and a pointer to a value.
     * If \ref m_packed_value is valid, then its value is used. If
     * it is nullptr then the value pointed to by \ref m_value is used.
     */
    template<typename T>
    class value
    {
    public:
      /*!
       * Ctor from a value.
       * \param p value with which to initialize, the object point to
       *          by p must stay in scope until either make_packed()
       *          is called or the dtor of this \ref value object is
       *          called.
       */
      value(const T *p = nullptr):
        m_value(p)
      {}

      /*!
       * Ctor from a packed value.
       * \param p value with which to initialize \ref m_packed_value
       */
      value(const PainterPackedValue<T> &p):
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

    /*!
     * \brief
     * A CustomBrush is just a conveniance to wrap a
     * pointer to a \ref PainterBrushShader
     * together with a value<PainterBrushShaderData>.
     */
    class CustomBrush
    {
    public:
      /*!
       * Ctor.
       * \param sh value with which to initialize \ref m_shader
       * \param d value with which to initialize \ref m_data
       */
      CustomBrush(const PainterBrushShader *sh,
                  const value<PainterBrushShaderData> &d
                  = value<PainterBrushShaderData>()):
        m_shader(sh),
        m_data(d)
      {}

      /*!
       * Ctor.
       * \param sh value with which to initialize \ref m_shader
       * \param d value with which to initialize \ref m_data
       */
      CustomBrush(const value<PainterBrushShaderData> &d,
                  const PainterBrushShader *sh):
        m_shader(sh),
        m_data(d)
      {}

      /*!
       * What \ref PainterBrushShader is used
       */
      const PainterBrushShader *m_shader;

      /*!
       * What, if any, data for \ref m_shader to use.
       */
      value<PainterBrushShaderData> m_data;
    };

    /*!
     * \brief
     * A brush_value stores the brush applied; it stores either
     * a \ref value for a \ref PainterBrush or a \ref value
     * for a \ref PainterBrushShaderData together with
     * a value from \ref PainterBrushShader::ID().
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
      brush_value(const CustomBrush &br)
      {
        set(br);
      }

      /*!
       * Ctor to set the brush_value to source from a
       * \ref PainterBrush packed.
       */
      brush_value(const value<PainterBrushShaderData> &brush_data)
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
      set(const CustomBrush &br)
      {
        m_brush_shader_data = br.m_data;
        m_brush_shader = br.m_shader;
      }

      /*!
       * Set to source from a packed PainterBrush value.
       */
      void
      set(const value<PainterBrushShaderData> &brush_data)
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
       * Returns the value<PainterBrushShaderData> holding
       * the brush data.
       */
      const value<PainterBrushShaderData>&
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
      value<PainterBrushShaderData> m_brush_shader_data;

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
    value<PainterItemShaderData> m_item_shader_data;

    /*!
     * value for blend shader data
     */
    value<PainterBlendShaderData> m_blend_shader_data;

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
    set(const CustomBrush &value)
    {
      m_brush.set(value);
      return *this;
    }

    /*!
     * Sets \ref m_item_shader_data
     */
    PainterData&
    set(const value<PainterItemShaderData> &value)
    {
      m_item_shader_data = value;
      return *this;
    }

    /*!
     * Sets \ref m_blend_shader_data
     */
    PainterData&
    set(const value<PainterBlendShaderData> &value)
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

  /*!
   * Conveniance typedef
   */
  typedef PainterData::CustomBrush PainterCustomBrush;

/*! @} */
}
