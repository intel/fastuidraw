/*!
 * \file gl_context_properties.hpp
 * \brief file gl_context_properties.hpp
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>

namespace fastuidraw
{
  namespace gl
  {

/*!\addtogroup GLUtility
  @{
 */

    /*!
      \brief
      A ContextProperties provides an interface to
      query GL/GLES version and extensions.
     */
    class ContextProperties:noncopyable
    {
    public:
      /*!
        Ctor.
        \param make_ready if true, query GL context at ctor (instead of lazily)
                          to generate GL context information
       */
      explicit
      ContextProperties(bool make_ready = false);

      ~ContextProperties();

      /*!
        Return the GL/GLES version of the GL context
        with the major version in [0] and the minor
        version in [1].
       */
      vecN<int, 2>
      version(void) const;

      /*!
        Returns the GL major version, equivalent to
        \code
        version()[0]
        \endcode
       */
      int
      major_version(void) const
      {
        return version()[0];
      }

      /*!
        Returns the GL minor version, equivalent to
        \code
        version()[1]
        \endcode
       */
      int
      minor_version(void) const
      {
        return version()[1];
      }

      /*!
        Returns true if the context is OpenGL ES,
        returns value if the context is OpenGL.
       */
      bool
      is_es(void) const;

      /*!
        Returns true if the context supports the named
        extension.
       */
      bool
      has_extension(c_string ext) const;

    private:
      void *m_d;
    };
/*! @} */
  }
}
