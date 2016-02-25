/*!
 * \file gl_header.hpp
 * \brief file gl_header.hpp
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


#ifdef FASTUIDRAW_GL_USE_GLES
#include <fastuidraw/gl_backend/ngl_gles3.hpp>
#else
#include <fastuidraw/gl_backend/ngl_gl.hpp>
#endif
