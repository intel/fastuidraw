/*!
 * \file path_dash_effect.hpp
 * \brief file path_dash_effect.hpp
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

#include <fastuidraw/path_effect.hpp>

namespace fastuidraw
{
/*!\addtogroup Paths
 * @{
 */

  /*!
   * A \ref PathDashEffect implemented \ref PathEffect to
   * apply a dash pattern to a path.
   */
  class PathDashEffect:public PathEffect
  {
  public:
    PathDashEffect(void);
    ~PathDashEffect();

    PathDashEffect&
    clear(void);

    PathDashEffect&
    add_dash(float draw, float skip);

    PathDashEffect&
    dash_offset(float v);

    virtual
    void
    process_chain(const segment_chain &chain, Storage &dst) const override;

    virtual
    void
    process_join(const TessellatedPath::join &join, Storage &dst) const override;

    virtual
    void
    process_cap(const TessellatedPath::cap &cap, Storage &dst) const override;

  private:
    void *m_d;
  };

/*! @} */
}
