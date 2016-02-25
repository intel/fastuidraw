/*!
 * \file painter_state.hpp
 * \brief file painter_state.hpp
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


#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    Namespace encapsulating the classes that hold the values
    for the drawing state of a PainterPacker.
   */
  namespace PainterState
  {
    /*!
      A ClipEquations stores the clip equation for
      PainterPacker. Each vec3 gives a clip
      equation in 3D API clip coordinats (i.e. after
      ItemMatrix transformation is applied) as
      dot(clip_vector, p) >= 0.
    */
    class ClipEquations
    {
    public:
      /*!
        Ctor, initializes all clip equations as \f$ z \geq 0\f$
       */
      ClipEquations(void):
        m_clip_equations(vec3(0.0f, 0.0f, 1.0f))
      {}

      /*!
        Pack the values of this ClipEquations
        \param alignment alignment of the data store
                         in units of generic_data, see
                         PainterBackend::Configuration::alignment()
        \param dst place to which to pack data
       */
      void
      pack_data(unsigned int alignment, c_array<generic_data> dst) const;

      /*!
        Returns the length of the data needed to encode the data.
        Data is padded to be multiple of alignment.
        \param alignment alignment of the data store
                         in units of generic_data, see
                         PainterBackend::Configuration::alignment()
       */
      unsigned int
      data_size(unsigned int alignment) const
      {
        using namespace PainterPacking;
        return round_up_to_multiple(clip_equations_data_size, alignment);
      }

      /*!
        Each element of m_clip_equations specifies a
        clipping plane in 3D API clip-space as
        \code
        dot(m_clip_equations[i], p) >= 0
        \endcode
       */
      vecN<vec3, 4> m_clip_equations;
    };

    /*!
      A ItemMatrix holds the value for the
      transformation from item coordinates to the coordinates
      in which the clipping rectangle applies.
    */
    class ItemMatrix
    {
    public:
      /*!
        Ctor from a float3x3
        \param m value with which to initailize m_item_matrix
       */
      ItemMatrix(const float3x3 &m):
        m_item_matrix(m)
      {}

      /*!
        Ctor, initializes m_item_matrix as the
        identity matrix
       */
      ItemMatrix(void)
      {}

      /*!
        Returns the length of the data needed to encode the data.
        Data is padded to be multiple of alignment.
        \param alignment alignment of the data store
                         in units of generic_data, see
                         PainterBackend::Configuration::alignment()
       */
      unsigned int
      data_size(unsigned int alignment) const
      {
        using namespace PainterPacking;
        return round_up_to_multiple(item_matrix_data_size, alignment);
      }

      /*!
        Pack the values of this ItemMatrix
        \param alignment alignment of the data store
                         in units of generic_data, see
                         PainterBackend::Configuration::alignment()
        \param dst place to which to pack data
       */
      void
      pack_data(unsigned int alignment, c_array<generic_data> dst) const;

      /*!
        The 3x3 matrix tranforming from item coordinate
        to the coordinates of the clipping rectange.
      */
      float3x3 m_item_matrix;
    };

    /*!
      Common base class to VertexShaderData and
      FragmentShaderData to hold shader data for
      custom shaders.
     */
    class ShaderData
    {
    public:
      /*!
        Ctor. Copies the data into the data store.
        \param pdata data from which to copy
       */
      explicit
      ShaderData(const_c_array<generic_data> pdata);

      /*!
        Inits as having no data.
       */
      ShaderData(void);

      /*!
        Copy ctor
       */
      ShaderData(const ShaderData &obj);

      ~ShaderData();

      /*!
        Assignment operator
       */
      ShaderData&
      operator=(const ShaderData &rhs);

      /*!
        Returns a writeable pointer to the backing store of the data
       */
      c_array<generic_data>
      data(void);

      /*!
        Returns a readable pointer to the backing store of the data
       */
      const_c_array<generic_data>
      data(void) const;

      /*!
        Resize the data store. After resize, previous value returned
        by data() are not guaranteed to be valid.
       */
      void
      resize_data(unsigned int sz);

      /*!
        Returns the length of the data needed to encode the data.
        Data is padded to be multiple of alignment.
        \param alignment alignment of the data store
                         in units of generic_data, see
                         PainterBackend::Configuration::alignment()
       */
      unsigned int
      data_size(unsigned int alignment) const;

      /*!
        Copies the values of m_data.
        \param alignment alignment of the data store
                         in units of generic_data, see
                         PainterBackend::Configuration::alignment()
        \param dst place to which to pack data
       */
      void
      pack_data(unsigned int alignment, c_array<generic_data> dst) const;

    private:
      void *m_d;
    };

    /*!
      VertexShaderData holds custom data for vertex shaders
     */
    class VertexShaderData:public ShaderData
    {
    public:
      /*!
        Ctor. Copies the data into the data store.
        \param pdata data from which to copy
       */
      explicit
      VertexShaderData(const_c_array<generic_data> pdata):
        ShaderData(pdata)
      {}

      /*!
        Inits as having no data.
       */
      VertexShaderData(void)
      {}
    };

    /*!
      FragmentShaderData holds custom data for fragment shaders
     */
    class FragmentShaderData:public ShaderData
    {
    public:
      /*!
        Ctor. Copies the data into the data store.
        \param pdata data from which to copy
       */
      explicit
      FragmentShaderData(const_c_array<generic_data> pdata):
        ShaderData(pdata)
      {}

      /*!
        Inits as having no data.
       */
      FragmentShaderData(void)
      {}
    };

    /*!
      Class to specify stroking parameters, data is packed
      as according to PainterPacking::stroke_data_offset_t.
     */
    class StrokeParams:public VertexShaderData
    {
    public:
      /*!
        Ctor.
       */
      StrokeParams(void);

      /*!
        The miter limit for miter joins
       */
      float
      miter_limit(void) const;

      /*!
        Set the value of miter_limit(void) const
       */
      StrokeParams&
      miter_limit(float f);

      /*!
        The stroking width
       */
      float
      width(void) const;

      /*!
        Set the value of width(void) const
       */
      StrokeParams&
      width(float f);
    };

  } //namespace PainterState

/*! @} */

} //namespace fastuidraw
