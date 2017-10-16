/*!
 * \file egl_binding.cpp
 * \brief file egl_binding.cpp
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
#include <fastuidraw/ngl_egl.hpp>
#include <fastuidraw/egl_binding.hpp>

namespace
{
  fastuidraw::APICallbackSet&
  ngl(void)
  {
    static fastuidraw::APICallbackSet R;
    return R;
  }

  #ifdef FASTUIDRAW_DEBUG
    thread_local EGLint egl_error_code = EGL_SUCCESS;
  #endif

  std::string
  egl_error_check(void)
  {
    auto p = FASTUIDRAWeglfunctionPointer(eglGetError);
    EGLint error_code;
    std::ostringstream str;

    for(error_code = p(); error_code != EGL_SUCCESS; error_code = p())
      {
        #ifdef FASTUIDRAW_DEBUG
          {
            egl_error_code = error_code;
          }
        #endif

        switch(error_code)
          {
          default:
            str << "0x" << std::hex << error_code;
          }
      }
    return str.str();
  }
}

/////////////////////////////////
// fastuidraw::egl_binding::CallbackEGL methods
fastuidraw::egl_binding::CallbackEGL::
CallbackEGL(void):
  APICallbackSet::CallBack(&ngl())
{}

///////////////////////////////
// egl_binding methods
void
fastuidraw::egl_binding::
on_load_function_error(c_string fname)
{
  std::cerr << "Unable to load function: \"" << fname << "\"\n";
}

void
fastuidraw::egl_binding::
call_unloadable_function(c_string fname)
{
  /* Should we just assume the loggers will
   * emit?
   */
  std::cerr << "Call to unloadable function: \"" << fname << "\"\n";
  ngl().call_unloadable_function(fname);
}

void
fastuidraw::egl_binding::
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
fastuidraw::egl_binding::
post_call(c_string call_string_values,
          c_string call_string_src,
          c_string function_name,
          void *function_ptr,
          c_string src_file, int src_line)
{
  std::string error;
  error = egl_error_check();

  /* Should we just assume the loggers will
   * emit?
   */
  if(!error.empty())
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
fastuidraw::egl_binding::
get_proc_function(void* (*get_proc)(c_string), bool load_functions)
{
  ngl().get_proc_function(get_proc);
  if(load_functions && get_proc != nullptr)
    {
      load_all_functions(false);
    }
}

void*
fastuidraw::egl_binding::
get_proc(c_string function_name)
{
  return ngl().get_proc(function_name);
}

EGLint
fastuidraw::egl_binding::
get_error(void)
{
  #ifdef FASTUIDRAW_DEBUG
    {
      EGLint return_value;
      return_value = egl_error_code;
      egl_error_code = EGL_SUCCESS;
      return return_value;
    }
  #else
    {
      return eglGetError();
    }
  #endif
}
