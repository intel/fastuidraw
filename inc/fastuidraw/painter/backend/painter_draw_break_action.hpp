/*!
 * \file painter_draw_break_action.hpp
 * \brief file painter_draw_break_action.hpp
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


#ifndef FASTUIDRAW_PAINTER_DRAW_BREAK_ACTION_HPP
#define FASTUIDRAW_PAINTER_DRAW_BREAK_ACTION_HPP

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/gpu_dirty_state.hpp>

namespace fastuidraw
{
///@cond
class PainterBackend;
///@endcond

/*!\addtogroup PainterBackend
 * @{
 */

  /*!\brief
   * A \ref PainterDrawBreakAction represents an action on
   * the 3D API between two indices to be fed the the GPU; a
   * \ref PainterDrawBreakAction will imply an draw break in
   * the underlying 3D API. Typical example of such an action
   * would be to bind a texture.
   */
  class PainterDrawBreakAction:
    public reference_counted<PainterDrawBreakAction>::concurrent
  {
  public:
    /*!
     * To be implemented by a derived class to execute the action
     * and to return what portions of the GPU state are made dirty
     * by the action.
     * \param backend the \ref PainterBackend object from which this
     *                action is called as it issues 3D API commands.
     */
    virtual
    gpu_dirty_state
    execute(PainterBackend *backend) const = 0;
  };
/*! @} */
}

#endif
