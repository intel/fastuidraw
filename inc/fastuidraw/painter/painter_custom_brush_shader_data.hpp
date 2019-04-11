/*!
 * \file painter_custom_brush_shader_data.hpp
 * \brief file painter_custom_brush_shader_data.hpp
 *
 * Copyright 2019 by Intel.
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

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/image.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * Base class to hold custom data for custom brush shaders.
   *
   * Derived classes CANNOT add any data or virtual functions.
   * The class PainterCustomBrushShaderData is essentially a
   * wrapper over a PainterCustomBrushShaderData::DataBase
   * object that handles holding data and copying itself (for
   * the purpose of copying PainterCustomBrushShaderData
   * objects).
   */
  class PainterCustomBrushShaderData
  {
  public:
    /*!
     * \brief
     * Class that holds the actual data and packs the data.
     *
     * A class derived from PainterCustomBrushShaderData should set the
     * field \ref m_data to point to an object derived from
     * DataBase for the purpose of holding and packing data.
     */
    class DataBase
    {
    public:
      virtual
      ~DataBase()
      {}

      /*!
       * To be implemented by a derived class to create
       * a copy of itself.
       */
      virtual
      DataBase*
      copy(void) const = 0;

      /*!
       * To be implemented by a derived class to return
       * a \ref c_array of references to \ref Image
       * objects whose Image::type() value is \ref
       * Image::context_texture2d. The i'th entry in
       * the returned array will be bound to the i'th
       * external texture slot of the backend via the
       * \ref PainterDrawBreakAction objected returned
       * by \ref PainterBackend::bind_image().
       */
      virtual
      c_array<const reference_counted_ptr<const Image> >
      bind_images(void) const = 0;

      /*!
       * To be implemented by a derived class to return
       * the length of the data needed to encode the data.
       */
      virtual
      unsigned int
      data_size(void) const = 0;

      /*!
       * To be implemtend by a derive class to pack its data.
       * \param dst place to which to pack data
       */
      virtual
      void
      pack_data(c_array<generic_data> dst) const = 0;
    };

    /*!
     * Ctor. A derived class from PainterCustomBrushShaderData
     * should set \ref m_data.
     */
    PainterCustomBrushShaderData(void);

    /*!
     * Copy ctor, calls DataBase::copy() to
     * copy the data behind \ref m_data.
     */
    PainterCustomBrushShaderData(const PainterCustomBrushShaderData &obj);

    ~PainterCustomBrushShaderData();

    /*!
     * Assignment operator
     */
    PainterCustomBrushShaderData&
    operator=(const PainterCustomBrushShaderData &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterCustomBrushShaderData &obj);

    /*!
     * Return a \ref c_array of references to \ref
     * Image objects whose Image::type() value is
     * \ref Image::context_texture2d. The i'th entry
     * in the returned array will be bound to the i'th
     * external texture slot of the backend via the
     * \ref PainterDrawBreakAction objected returned
     * by \ref PainterBackend::bind_image().
     */
    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const;

    /*!
     * Returns the length of the data needed to encode the data.
     * The returned value is guaranteed to be a multiple of 4.
     */
    unsigned int
    data_size(void) const;

    /*!
     * Pack the values of this object
     * \param dst place to which to pack data
     */
    void
    pack_data(c_array<generic_data> dst) const;

    /*!
     * Returns a pointer to the underlying object holding
     * the data of the PainterCustomBrushShaderData.
     */
    const DataBase*
    data_base(void) const
    {
      return m_data;
    }

  protected:
    /*!
     * Initialized as nullptr by the ctor PainterCustomBrushShaderData(void).
     * A derived class of PainterCustomBrushShaderData should assign \ref
     * m_data to point to an object derived from DataBase.
     * That object is the object that is to determine the
     * size of data to pack and how to pack the data into
     * the data store buffer.
     */
    DataBase *m_data;
  };

/*! @} */

} //namespace fastuidraw
