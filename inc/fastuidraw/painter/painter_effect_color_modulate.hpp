/*!
 * \file painter_effect_color_modulate.hpp
 * \brief file painter_effect_color_modulate.hpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/painter_effect.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterEffectColorModulate represents modulating by
   * a color.
   */
  class PainterEffectColorModulate:public PainterEffect
  {
  public:
    PainterEffectColorModulate(void);

    /*!
     * Set the color by which to modulate the image.
     */
    void
    color(const vec4 &color);

    /*!
     * Return the color by which to modulate the image.
     */
    const vec4&
    color(void) const;

    virtual
    reference_counted_ptr<PainterEffect>
    copy(void) const override;
  };
/*! @} */
};
