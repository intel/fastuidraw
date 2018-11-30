/*!
 * \file gl_binding.cpp
 * \brief file gl_binding.cpp
 *
 * Adapted from: ngl_backend.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#include <iostream>
#include <sstream>
#include <list>
#include <mutex>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/api_callback.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>

namespace
{
  std::string
  gl_error_check(void)
  {
    auto p = FASTUIDRAWglfunctionPointer(glGetError);
    GLenum error_code;
    std::ostringstream str;

    for(error_code = p(); error_code != GL_NO_ERROR; error_code = p())
      {
        switch(error_code)
        {
        case GL_INVALID_ENUM:
          str << "GL_INVALID_ENUM ";
          break;
        case GL_INVALID_VALUE:
          str << "GL_INVALID_VALUE ";
          break;
        case GL_INVALID_OPERATION:
          str << "GL_INVALID_OPERATION ";
          break;
        case GL_OUT_OF_MEMORY:
          str << "GL_OUT_OF_MEMORY ";
          break;

        default:
          str << "0x" << std::hex << error_code;
        }
      }
    return str.str();
  }

  fastuidraw::APICallbackSet&
  ngl(void)
  {
    #ifdef FASTUIDRAW_GL_USE_GLES
    static fastuidraw::APICallbackSet R("libNGLES");
    #else
    static fastuidraw::APICallbackSet R("libNGL");
    #endif

    return R;
  }
}

/////////////////////////////////
// fastuidraw::gl_binding::CallbackGL methods
fastuidraw::gl_binding::CallbackGL::
CallbackGL(void):
  APICallbackSet::CallBack(&ngl())
{}

///////////////////////////////
// gl_binding methods
namespace fastuidraw
{
  namespace gl_binding
  {
    void on_load_function_error(const char *fname);
    void call_unloadable_function(const char *fname);
    void post_call(const char *call, const char *src, const char *function_name, void* fptr, const char *fileName, int line);
    void pre_call(const char *call, const char *src, const char *function_name, void* fptr, const char *fileName, int line);
    void load_all_functions(bool);
  }
}

void
fastuidraw::gl_binding::
on_load_function_error(c_string fname)
{
  std::cerr << ngl().label() << ": Unable to load function: \"" << fname << "\"\n";
}

void
fastuidraw::gl_binding::
call_unloadable_function(c_string fname)
{
  /* Should we just assume the loggers will
   * emit?
   */
  std::cerr << ngl().label() << ": Call to unloadable function: \"" << fname << "\"\n";
  ngl().call_unloadable_function(fname);
}

void
fastuidraw::gl_binding::
pre_call(c_string call_string_values,
         c_string call_string_src,
         c_string function_name,
         void *function_ptr,
         c_string src_file, int src_line)
{
  ngl().pre_call(call_string_values, call_string_src,
                 function_name, function_ptr,
                 src_file, src_line);
}

void
fastuidraw::gl_binding::
post_call(c_string call_string_values,
          c_string call_string_src,
          c_string function_name,
          void *function_ptr,
          c_string src_file, int src_line)
{
  std::string error;
  error = gl_error_check();

  /* Should we just assume the loggers will
   * emit?
   */
  if (!error.empty())
    {
      std::cerr << "[" << src_file << "," << std::dec
                << src_line << "] "
                << call_string_values << "{"
                << error << "}\n";
    }

  ngl().post_call(call_string_values, call_string_src,
                  function_name, error.c_str(),
                  function_ptr, src_file, src_line);
}

void
fastuidraw::gl_binding::
get_proc_function(void* (*get_proc)(c_string), bool load_functions)
{
  ngl().get_proc_function(get_proc);
  if (load_functions && get_proc != nullptr)
    {
      load_all_functions(false);
    }
}

void
fastuidraw::gl_binding::
get_proc_function(void *data,
                  void* (*get_proc)(void *, c_string),
                  bool load_functions)
{
  ngl().get_proc_function(data, get_proc);
  if (load_functions && get_proc != nullptr)
    {
      load_all_functions(false);
    }
}

void*
fastuidraw::gl_binding::
get_proc(c_string function_name)
{
  return ngl().get_proc(function_name);
}
