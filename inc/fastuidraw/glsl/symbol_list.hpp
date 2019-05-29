/*!
 * \file symbol_list.hpp
 * \brief file symbol_list.hpp
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

#include <fastuidraw/glsl/varying_list.hpp>
#include <fastuidraw/glsl/shareable_value_list.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */

    /*!
     * \brief
     * A \ref symbol_list embodies a \ref varying_list along with a set
     * of shareable values for the vertex and fragment shaders.
     */
    class symbol_list
    {
    public:
      /*!
       * Ctor.
       * \param varyings value with which to initialize \ref m_varying_list
       * \param vert_sharables value with which to initialize \ref m_vert_shareable_values
       * \param frag_sharables value with which to initialize \ref m_frag_shareable_values
       */
      symbol_list(const varying_list &varyings = varying_list(),
                  const shareable_value_list &vert_sharables = shareable_value_list(),
                  const shareable_value_list &frag_sharables = shareable_value_list()):
        m_varying_list(varyings),
        m_vert_shareable_values(vert_sharables),
        m_frag_shareable_values(frag_sharables)
      {}

      /*!
       * Varyings of a from the vertex shader to the fragment shader
       */
      varying_list m_varying_list;

      /*!
       * List of shareable values to callers of vertex shader
       */
      shareable_value_list m_vert_shareable_values;

      /*!
       * List of shareable values to callers of fragment shader
       */
      shareable_value_list m_frag_shareable_values;
    };

/*! @} */
  }
}
