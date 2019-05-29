/*!
 * \file painter_effect.hpp
 * \brief file painter_effect.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#pragma once

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/image.hpp>
#include <fastuidraw/painter/shader_data/painter_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * A PainterEffectParams represents the parameters for a
   * \ref PainterEffect. A \ref PainterEffect derived object
   * will use a \ref PainterEffectParams derived object for
   * its parameters. A user of \ref PainterEffect will
   * use the correct \ref PainterEffectParams derived object
   * when calling \ref PainterEffect::brush(). A \ref
   * PainterEffectParams is passed by non-constant reference
   * to \ref PainterEffect::brush(). A typical implementation
   * of \ref PainterEffect and \ref PainterEffectParams is
   * that the call to \ref PainterEffect::brush() will modify
   * the contents of the passed \ref PainterEffectParams
   * object so that it backs the correct value for the return
   * value of type \ref PainterData::brush_value. The callers
   * of \ref PainterEffect::brush() must guarantee that a fixed
   * \ref PainterEffectParams is not used simutaneosly by
   * multiple threads.
   */
  class PainterEffectParams:noncopyable
  {
  public:
    virtual
    ~PainterEffectParams()
    {}
  };

  /*!
   * \brief
   * A \ref PainterEffect represents the interface to
   * define and effect to apply to image data. At its
   * core, it is made up of a sequence of passes.
   */
  class PainterEffect:
    public reference_counted<PainterEffect>::concurrent
  {
  public:
    virtual
    ~PainterEffect()
    {}

    /*!
     * To be implemented by a derived class to return the
     * number of passes the \ref PainterEffect has.
     */
    virtual
    unsigned int
    number_passes(void) const = 0;

    /*!
     * To be implemented by a derived class to return the
     * brush made from the passed \ref Image value. The
     * returned PainterData::brush_value value needs to be
     * valid until the \ref PainterEffectParams dtor is called
     * or the next call to brush() {called on from \ref
     * PainterEffect} passing the same \ref PainterEffectParams.
     * The passed image is guaranteed to have Image::type() as
     * value \ref Image::bindless_texture2d or \ref
     * Image::context_texture2d. The method brush() is to
     * be thread safe with respect to the \ref PainterEffect
     * object, but NOT with respect to the \ref PainterEffectParams
     * object.
     * \param pass the effect pass
     * \param image the image to which the effect is applied
     * \param brush_rect the brush coordinates of the rect
     *                   drawn
     * \param params the parameters of the effect
     */
    virtual
    PainterData::brush_value
    brush(unsigned pass,
          const reference_counted_ptr<const Image> &image,
	  const Rect &brush_rect,
          PainterEffectParams &params) const = 0;
  };
/*! @} */
}
