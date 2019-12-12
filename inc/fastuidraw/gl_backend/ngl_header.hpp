/*!
 * \file ngl_header.hpp
 * \brief file ngl_header.hpp
 *
 * Copyright 2016 by Intel.
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

#ifdef __EMSCRIPTEN__
  #include <fastuidraw/gl_backend/ngl_emscripten_gles3.hpp>
#elif defined(FASTUIDRAW_GL_USE_GLES)
  #include <fastuidraw/gl_backend/ngl_gles3.hpp>
#else
  #include <fastuidraw/gl_backend/ngl_gl.hpp>
#endif
