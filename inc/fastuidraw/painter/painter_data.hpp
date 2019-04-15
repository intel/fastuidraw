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
#include <fastuidraw/painter/shader/painter_custom_brush_shader.hpp>

namespace fastuidraw
{

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
       * \param p value with which to initialize \ref m_value
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
       * Pointer to value.
       */
      const T *m_value;

      /*!
       * Value pre-packed and ready for reuse.
       */
      PainterPackedValue<T> m_packed_value;

      /*!
       * Returns true if either \ref m_value or
       * \ref m_packed_value is not nullptr.
       */
      bool
      has_data(void) const
      {
        return m_packed_value || m_value != nullptr;
      }

      /*!
       * If \ref m_packed_value is non-nullptr, returns the value
       * behind it (i.e. PainterPackedValue<T>::value()), otherwise
       * returns the dereference of \ref m_value.
       */
      const T&
      data(void) const
      {
        FASTUIDRAWassert(m_packed_value || m_value != nullptr);
        return m_packed_value ?
          m_packed_value.value() :
          *m_value;
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
      make_packed(PainterPackedValuePool &pool)
      {
        if (!m_packed_value && m_value != nullptr)
          {
            m_packed_value = pool.create_packed_value(*m_value);
            m_value = nullptr;
          }
      }
    };

    /*!
     * \brief
     * A CustomBrush is just a conveniance to wrap a
     * pointer to a \ref PainterCustomBrushShader
     * together with a value<PainterCustomBrushShaderData>.
     */
    class CustomBrush
    {
    public:
      /*!
       * Ctor.
       * \param sh value with which to initialize \ref m_shader
       * \param d value with which to initialize \ref m_data
       */
      CustomBrush(const PainterCustomBrushShader *sh,
                  const value<PainterCustomBrushShaderData> &d
                  = value<PainterCustomBrushShaderData>()):
        m_shader(sh),
        m_data(d)
      {}

      /*!
       * Ctor.
       * \param sh value with which to initialize \ref m_shader
       * \param d value with which to initialize \ref m_data
       */
      CustomBrush(const value<PainterCustomBrushShaderData> &d,
                  const PainterCustomBrushShader *sh):
        m_shader(sh),
        m_data(d)
      {}

      /*!
       * What \ref PainterCustomBrushShader is used
       */
      const PainterCustomBrushShader *m_shader;

      /*!
       * What, if any, data for \ref m_shader to use.
       */
      value<PainterCustomBrushShaderData> m_data;
    };

    /*!
     * \brief
     * A brush_value stores the brush applied; it stores either
     * a \ref value for a \ref PainterBrush or a \ref value
     * for a \ref PainterCustomBrushShaderData together with
     * a value from \ref PainterCustomBrushShader::ID().
     */
    class brush_value
    {
    public:
      /*!
       * Empty ctor that initializes to not have a brush source
       * (custom or \ref PainterBrush).
       */
      brush_value(void):
        m_custom_brush_shader(nullptr)
      {}

      /*!
       * Ctor to set the brush_value to source from a
       * \ref PainterBrush.
       */
      brush_value(const value<PainterBrush> &v)
      {
        set(v);
      }

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
       * \ref PainterBrush.
       */
      brush_value(const PainterPackedValue<PainterBrush> &v)
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
       * Set to source from a \ref PainterBrush
       */
      void
      set(const value<PainterBrush> &v)
      {
        FASTUIDRAWassert(v.has_data());
        m_fixed_function_brush = v;
        m_custom_brush_shader_data = value<PainterCustomBrushShaderData>();
        m_custom_brush_shader = nullptr;
      }

      /*!
       * Set to source from a custom brush shader
       */
      void
      set(const CustomBrush &br)
      {
        FASTUIDRAWassert(br.m_shader);
        m_fixed_function_brush = value<PainterBrush>();
        m_custom_brush_shader = br.m_shader;
        m_custom_brush_shader_data = br.m_data;
      }

      /*!
       * Returns the value<PainterBrush> value, asserts if the
       * brush_value is set to brush using a custom shader brush.
       */
      const value<PainterBrush>&
      fixed_function_brush(void) const
      {
        FASTUIDRAWassert(!m_custom_brush_shader);
        return m_fixed_function_brush;
      }

      /*!
       * Returns the value<PainterCustomBrushShaderData> value,
       * asserts if the brush_value is not set to brush using a
       * custom shader brush.
       */
      const value<PainterCustomBrushShaderData>&
      custom_brush_shader_data(void) const
      {
        FASTUIDRAWassert(m_custom_brush_shader);
        return m_custom_brush_shader_data;
      }

      /*!
       * Returns nullptr if the brush_value is set to brush using
       * using \ref PainterBrush; otherwise returns a pointer
       * to the PainterCustomBrushShader used.
       */
      const PainterCustomBrushShader*
      custom_shader_brush(void) const
      {
        return m_custom_brush_shader;
      }

      /*!
       * If custom_shader_brush() is nullptr, make \ref
       * fixed_function_brush() packed, otherwise mae
       * custom_brush_shader_data() packed.
       */
      void
      make_packed(PainterPackedValuePool &pool)
      {
        if (m_custom_brush_shader)
          {
            m_custom_brush_shader_data.make_packed(pool);
          }
        else
          {
            m_fixed_function_brush.make_packed(pool);
          }
      }

      /*!
       * Returns if the active element has data
       */
      bool
      has_data(void) const
      {
        return m_custom_brush_shader ?
          m_custom_brush_shader_data.has_data() :
          m_fixed_function_brush.has_data();
      }

      /*!
       * Provided as a conveniance, returns the PainterBrush::shader()
       * if backed by a \ref PainterBrush, otherwise returns
       * \ref PainterCustomBrushShader::ID() if backed by a custom
       * brush.
       */
      uint32_t
      shader(void) const
      {
        return (m_custom_brush_shader) ?
          m_custom_brush_shader->ID() :
          fixed_function_shader();
      }

      /*!
       * Provided as a conveniance, returns 0 if backed by a
       * \ref PainterBrush, otherwise returns the value of
       * PainterCustomBrushShader::group() if backed by a custom
       * brush.
       */
      uint32_t
      shader_group(void) const
      {
        return (m_custom_brush_shader) ?
          m_custom_brush_shader->group() :
          0u;
      }

      /*!
       * Provided as a conveniance, returns an array holding
       * the reference to \ref PainterBrush::image() if backed
       * by a \ref PainterBrush, otherwise returns the value of
       * PainterCustomBrushShaderData::bind_images() if backed
       * by a custom brush.
       */
      c_array<const reference_counted_ptr<const Image> >
      bind_images(void) const
      {
         return (m_custom_brush_shader) ?
           custom_bind_images() :
           fixed_function_bind_images();
      }

    private:
      uint32_t
      fixed_function_shader(void) const
      {
        FASTUIDRAWassert(!m_custom_brush_shader);
        return (m_fixed_function_brush.has_data()) ?
          m_fixed_function_brush.data().shader() :
          0u;
      }

      c_array<const reference_counted_ptr<const Image> >
      fixed_function_bind_images(void) const
      {
        FASTUIDRAWassert(!m_custom_brush_shader);

        /* It is ok to take the address of PainterBrush::image()
         * because it returns a const-reference which means that
         * the reference object is not a temporary
         */
        return (m_fixed_function_brush.has_data()
                && m_fixed_function_brush.data().image_requires_binding()) ?
          c_array<const reference_counted_ptr<const Image> >(&m_fixed_function_brush.data().image(), 1) :
          c_array<const reference_counted_ptr<const Image> >();
      }

      c_array<const reference_counted_ptr<const Image> >
      custom_bind_images(void) const
      {
        FASTUIDRAWassert(m_custom_brush_shader);
        return (m_custom_brush_shader_data.has_data()) ?
          m_custom_brush_shader_data.data().bind_images() :
          c_array<const reference_counted_ptr<const Image> >();
      }

      /*!
       * The \ref value for a brush implemented via
       * \ref PainterBrush. Only has effect if \ref
       * m_custom_brush_shader is nullptr.
       */
      value<PainterBrush> m_fixed_function_brush;

      /*!
       * The \ref value for the brush data for a brush
       * implemented via \ref PainterCustomBrushShader.
       * Only has effect if \ref m_custom_brush_shader
       * is not nullptr.
       */
      value<PainterCustomBrushShaderData> m_custom_brush_shader_data;

      /*!
       * If non-null, indicates that the brush is a realized
       * by a custom brush shader.
       */
      const PainterCustomBrushShader *m_custom_brush_shader;
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
    set(const PainterPackedValue<PainterBrush> &value)
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
