/*!
 * \file path_enums.hpp
 * \brief file path_enums.hpp
 *
 * Copyright 2018 by Intel.
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

namespace fastuidraw  {

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * Class to encapsulate enumerations used by \ref Path and
 * \ref PathContour.
 */
class PathEnums
{
public:
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

  /*!
   * Enumeration to specify indexes into a \ref c_array
   * on how much a path's geometry is inflated by stroking.
   */
  enum path_geometry_inflation_index_t
    {
      /*!
       * Index into \ref c_array to indicate how much
       * the path geometry is inflated in pixels after its
       * inflation in in item coordinates and having the current
       * transformation matrix applied.
       */
      pixel_space_distance = 0,

      /*!
       * Index into \ref c_array to indicate how much
       * the path geometry is inflated in path cordinates
       * before the transformation matrix applied or the
       * inflation by \ref pixel_space_distance is applied
       */
      item_space_distance,

      /*!
       * Index into \ref c_array to indicate how much
       * the bounding box for taking miter-joins is inflated
       * in pixels after its inflation in in item coordinates
       * and having the current transformation matrix applied.
       */
      pixel_space_distance_miter_joins,

      /*!
       * Index into \ref c_array to indicate how much
       * the bounding box for taking miter-joins is inflated
       * in path cordinates before the transformation matrix
       * applied or the inflation by \ref
       * pixel_space_distance_miter_joins is applied
       */
      item_space_distance_miter_joins,

      path_geometry_inflation_index_count,
    };
};
/*! @} */

}
