/*!
 * \file painter_shader_data.hpp
 * \brief file painter_shader_data.hpp
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

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * Common base class to \ref PainterItemShaderData,
   * \ref PainterCustomBrushShaderData and \ref
   * PainterBlendShaderData to hold shader data for
   * custom shaders.
   */
  class PainterShaderData
  {
  public:
    /*!
     * Ctor.
     */
    PainterShaderData(void);

    /*!
     * Copy ctor.
     */
    PainterShaderData(const PainterShaderData &obj);

    virtual
    ~PainterShaderData();

    /*!
     * Assignment operator
     */
    PainterShaderData&
    operator=(const PainterShaderData &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterShaderData &obj);

    /*!
     * Returns the data packed. The length of the returned
     * array is guaranteed to be a multiple of 4.
     */
    c_array<const generic_data>
    packed_data(void) const;

    /*!
     * Returns the resources used by this \ref PainterShaderData
     */
    c_array<const reference_counted_ptr<const resource_base> >
    resources(void) const;

    /*!
     * To be implemented by a derived class to pack the
     * data.
     * \param dst location to which to pack the data
     */
    virtual
    void
    pack_data(c_array<generic_data> dst) const
    {
      FASTUIDRAWunused(dst);
    }

    /*!
     * To be implemented by a derived class to return
     * the length needed to pack the data.
     */
    virtual
    unsigned int
    data_size(void) const
    {
      return 0;
    }

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
     * To be called by a derived class to indicate that the nature of
     * the data so that either data_size() or pack_data() will do
     * something different than what the last call to them did.
     */
    void
    mark_dirty(void);

  private:
    void *m_d;
  };

  /*!
   * \brief
   * PainterItemShaderData holds custom data for item shaders
   */
  class PainterItemShaderData:public PainterShaderData
  {};

  /*!
   * \brief
   * PainterBlendShaderData holds custom data for blend shaders
   */
  class PainterBlendShaderData:public PainterShaderData
  {};

/*! @} */

} //namespace fastuidraw
