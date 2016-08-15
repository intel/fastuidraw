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
      Ctor. A derived class from PainterShaderData
      should set \ref m_data.
     */
    PainterShaderData(void);

    /*!
      Copy ctor, calls DataBase::copy() to
      copy the data behind \ref m_data.
     */
    PainterShaderData(const PainterShaderData &obj);

    ~PainterShaderData();

    /*!
      Assignment operator
     */
    PainterShaderData&
    operator=(const PainterShaderData &rhs);

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
      Pack the values of this object
      \param alignment alignment of the data store
                       in units of generic_data, see
                       PainterBackend::Configuration::alignment()
      \param dst place to which to pack data
    */
    void
    pack_data(unsigned int alignment, c_array<generic_data> dst) const;

    /*!
      Class that holds the actual data and packs the data.
      A class derived from PainterShaderData should set the
      field \ref m_data to point to an object derived from
      DataBase for the purpose of holding and packing data.
     */
    class DataBase
    {
    public:
      virtual
      ~DataBase()
      {}

      /*!
        To be implemented by a derived class to create
        a copy of itself.
       */
      virtual
      DataBase*
      copy(void) const = 0;

      /*!
        To be implemented by a derived class to return
        the length of the data needed to encode the data.
        Data is padded to be multiple of alignment.
        \param alignment alignment of the data store
               in units of generic_data, see
               PainterBackend::Configuration::alignment()
       */
      virtual
      unsigned int
      data_size(unsigned int alignment) const = 0;


      /*!
        To be implemtend by a derive dclass to pack its data.
        \param alignment alignment of the data store
               in units of generic_data, see
               PainterBackend::Configuration::alignment()
        \param dst place to which to pack data
      */
      virtual
      void
      pack_data(unsigned int alignment, c_array<generic_data> dst) const = 0;
    };

  protected:
    /*!
      Initialized as NULL by the ctor PainterShaderData(void).
      A derived class of PainterShaderData should assign \ref
      m_data to point to an object derived from DataBase.
      That object is the object that is to determine the
      size of data to pack and how to pack the data into
      the data store buffer.
     */
    DataBase *m_data;
  };

  /*!
    PainterItemShaderData holds custom data for item shaders
   */
  class PainterItemShaderData:public PainterShaderData
  {};

  /*!
    PainterBlendShaderData holds custom data for blend shaders
   */
  class PainterBlendShaderData:public PainterShaderData
  {};
/*! @} */

} //namespace fastuidraw
