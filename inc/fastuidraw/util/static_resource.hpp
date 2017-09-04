/*!
 * \file static_resource.hpp
 * \brief file static_resource.hpp
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

namespace fastuidraw
{
/*!\addtogroup Utility
  @{
 */

  /*!
    Generate and store a resource for use. Once a resource is added it
    cannot be removed.
    \param resource_label "name" of resource, the string is copied
    \param value "value" of resource, the data behind value is copied
   */
  void
  generate_static_resource(c_string resource_label, c_array<const uint8_t> value);

  /*!
    Returns the data behind a resource. If no resource is found,
    returns an empty const_c_array.
    \param resource_label label of resource as specified
                          by generate_static_resource().
   */
  c_array<const uint8_t>
  fetch_static_resource(c_string resource_label);

  /*!
    \brief
    Provided as a conveniance. The ctor calls
    generate_static_resource().
   */
  class static_resource
  {
  public:
    /*!
      Ctor.
      On construction, calls generate_static_resource().
      \param resource_label resource label to pass to generate_static_resource()
      \param value value of resource to pass to generate_static_resource()
     */
    static_resource(c_string resource_label, c_array<const uint8_t> value);
  };
/*! @} */
}
