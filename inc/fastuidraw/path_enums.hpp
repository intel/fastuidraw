/*!
 * \file path_enums.hpp
 * \brief file path_enums.hpp
 *
 * Copyright 2018 by Intel.
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

namespace fastuidraw  {

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * Namespace to encapsulate enumerations used by \ref Path and
 * \ref PathContour.
 */
namespace PathEnums
{
  /*!
   * A type_t specifies if an interpolator_base is to be
   * viewed as starting a new edge or continuing a previous
   * edge.
   */
  enum edge_type_t
    {
      /*!
       * Indicates the start of a new edge, thus the meeting place
       * between the edges is given a join whose style how to be
       * drawn is controllable.
       */
      starts_new_edge,

      /*!
       * Indicates that the edge is a continuation of the previous
       * edge.
       */
      continues_edge,
    };
  }
/*! @} */

}
