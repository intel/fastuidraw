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
   */
  class PainterBrushShaderData:noncopyable
  {
  public:
    virtual
    ~PainterBrushShaderData()
    {}

    /*!
     * To be implemented by a derived class to pack the
     * data.
     * \param dst location to which to pack the data
     */
    virtual
    void
    pack_data(c_array<generic_data> dst) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the length needed to pack the data.
     */
    virtual
    unsigned int
    data_size(void) const = 0;

    /*!
     * To be optionally implemented by a derived class to
     * save references to resources that need to be resident
     * after packing. Default implementation does nothing.
     * \param dst location to which to save resources.
     */
    virtual
    void
    save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const
    {
      FASTUIDRAWunused(dst);
    }

    /*!
     * To be optionally implemented by a derived class to
     * return the number of resources that need to be resident
     * after packing. Default implementation returns 0.
     */
    virtual
    unsigned int
    number_resources(void) const
    {
      return 0;
    }

    /*!
     * To be implemented by a derived class to return a \ref
     * c_array of references to \ref Image objects whose
     * Image::type() value is \ref Image::context_texture2d.
     * The i'th entry in the returned array will be bound to
     * the i'th external texture slot of the backend via the
     * \ref PainterDrawBreakAction objected returned by \ref
     * PainterBackend::bind_image(). Default implementation
     * is to return an empty array.
     */
    virtual
    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const
    {
      return c_array<const reference_counted_ptr<const Image> >();
    }
  };

/*! @} */

} //namespace fastuidraw
