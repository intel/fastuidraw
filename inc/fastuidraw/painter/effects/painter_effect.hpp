/*!
 * \file painter_effect.hpp
 * \brief file painter_effect.hpp
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

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/image.hpp>
#include <fastuidraw/painter/painter_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A \ref PainterEffectPass represents the interface to
   * define a single pass of a \ref PainterEffect. A pass is
   * essentially a mutable brush changed by what image to
   * which it will be applied. A single PainterEffectPass
   * is to be used by only one thread at a time.
   */
  class PainterEffectPass:
    public reference_counted<PainterEffectPass>::concurrent
  {
  public:
    virtual
    ~PainterEffectPass()
    {}

    /*!
     * To be implemented by a derived class to return the
     * brush made from the passed \ref Image value. The
     * returned PainterData::brush_value value needs to be
     * valid until the \ref PainterEffectPass dtor is called
     * or the next all to brush(). The passed image is
     * guaranteed to have Image::type() as value \ref
     * Image::bindless_texture2d or \ref Image::context_texture2d.
     */
    virtual
    PainterData::brush_value
    brush(const reference_counted_ptr<const Image> &image) = 0;
  };

  /*!
   * \brief
   * A \ref PainterEffect represents the interface to
   * define and effect to apply to image data. At its
   * core, it is made up of a sequence of passes. A
   * derived class is expected to expand on PainterEffect
   * to allow to specify properties of an effect (for
   * example blur properties for a blur effect) and
   * to reimplment copy().
   */
  class PainterEffect:
    public reference_counted<PainterEffect>::concurrent
  {
  public:
    /*!
     * Ctor; initializes as having no passes.
     */
    PainterEffect(void);

    virtual
    ~PainterEffect();

    /*!
     * Returns the passes of this \ref PainterEffect
     */
    c_array<const reference_counted_ptr<PainterEffectPass> >
    passes(void) const;

    /*!
     * To be implemented by a derived class to create a deep copy
     * of this \ref PainterEffect object.
     */
    virtual
    reference_counted_ptr<PainterEffect>
    copy(void) const = 0;

  protected:
    /*!
     * Add a pass to the effect. The passed \ref PainterEffectPass
     * is to be only used by this \ref PainterEffect object.
     */
    void
    add_pass(const reference_counted_ptr<PainterEffectPass> &effect);

    /*!
     * Provided as a conveniance to add several passes.
     * Equivalent to
     * \code
     * for (; begin != end; ++begin)
     *  {
     *    add_pass(*iter);
     *  }
     * \endcode
     */
    template<typename iterator>
    void
    add_passes(iterator begin, iterator end)
    {
      for (; begin != end; ++begin)
        {
          add_pass(*begin);
        }
    }

  private:
    void *m_d;
  };
/*! @} */
}
