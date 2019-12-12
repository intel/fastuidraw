/*!
 * \file gl_header.hpp
 * \brief file gl_header.hpp
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
  #include <GLES3/gl3.h>
#elif defined(FASTUIDRAW_GL_USE_GLES)
  #include <GLES3/gl3platform.h>
  #include <GLES3/gl3.h>
  #include <GLES3/gl31.h>
  #include <GLES3/gl32.h>
  #include <GLES2/gl2ext.h>
#else
  #include <GL/glcorearb.h>
#endif
