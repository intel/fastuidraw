/*!
 * \file painter_value.hpp
 * \brief file painter_value.hpp
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
    A ClipEquations stores the clip equation for
    PainterPacker. Each vec3 gives a clip
    equation in 3D API clip coordinats (i.e. after
    ItemMatrix transformation is applied) as
    dot(clip_vector, p) >= 0.
  */
  class PainterClipEquations
  {
  public:
    /*!
      Ctor, initializes all clip equations as \f$ z \geq 0\f$
    */
    PainterClipEquations(void):
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
    A PainterItemMatrix holds the value for the
    transformation from item coordinates to the coordinates
    in which the clipping rectangle applies.
  */
  class PainterItemMatrix
  {
  public:
    /*!
      Ctor from a float3x3
      \param m value with which to initailize m_item_matrix
    */
    PainterItemMatrix(const float3x3 &m):
      m_item_matrix(m)
    {}

    /*!
      Ctor, initializes m_item_matrix as the
      identity matrix
    */
    PainterItemMatrix(void)
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
    Common base class to PainterItemShaderData and
    PainterBlendShaderData to hold shader data for
    custom shaders.
   */
  class PainterShaderData
  {
  public:
    /*!
      Ctor. Copies the data into the data store.
      \param pdata data from which to copy
     */
    explicit
    PainterShaderData(const_c_array<generic_data> pdata);

    /*!
      Inits as having no data.
     */
    PainterShaderData(void);

    /*!
      Copy ctor
     */
    PainterShaderData(const PainterShaderData &obj);

    ~PainterShaderData();

    /*!
      Assignment operator
     */
    PainterShaderData&
    operator=(const PainterShaderData &rhs);

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
    PainterItemShaderData holds custom data for item shaders
   */
  class PainterItemShaderData:public PainterShaderData
  {
  public:
    /*!
      Ctor. Copies the data into the data store.
      \param pdata data from which to copy
     */
    explicit
    PainterItemShaderData(const_c_array<generic_data> pdata):
      PainterShaderData(pdata)
    {}

    /*!
      Inits as having no data.
     */
    PainterItemShaderData(void)
    {}
  };

  /*!
    PainterBlendShaderData holds custom data for blend shaders
   */
  class PainterBlendShaderData:public PainterShaderData
  {
  public:
    /*!
      Ctor. Copies the data into the data store.
      \param pdata data from which to copy
     */
    explicit
    PainterBlendShaderData(const_c_array<generic_data> pdata):
      PainterShaderData(pdata)
    {}

    /*!
      Inits as having no data.
     */
    PainterBlendShaderData(void)
    {}
  };

  /*!
    Class to specify stroking parameters, data is packed
    as according to PainterPacking::stroke_data_offset_t.
   */
  class PainterStrokeParams:public PainterItemShaderData
  {
  public:
    /*!
      Ctor.
     */
    PainterStrokeParams(void);

    /*!
      The miter limit for miter joins
     */
    float
    miter_limit(void) const;

    /*!
      Set the value of miter_limit(void) const
     */
    PainterStrokeParams&
    miter_limit(float f);

    /*!
      The stroking width
     */
    float
    width(void) const;

    /*!
      Set the value of width(void) const
     */
    PainterStrokeParams&
    width(float f);
  };


/*! @} */

} //namespace fastuidraw
