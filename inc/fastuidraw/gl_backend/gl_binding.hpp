/*!
 * \file gl_binding.hpp
 * \brief file gl_binding.hpp
 *
 * Adapted from: ngl_backend.hpp of WRATH:
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

#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw {


/*!\addtogroup GLUtility
  @{
 */

/*!
  \brief
  Provides interface for application to use GL and for application
  to specify to FastUIDraw GL backend how to get GL function pointers.
  Part of the GL backend libraries, libFastUIDrawGL and libFastUIDrawGLES.

  Short version:
   - application should call fastuidraw::gl_binding::get_proc_function()
     to set the function which the GL backend for FastUIDraw will use to
     fetch GL function pointers.
   - If an application wishes, it can include <fastuidraw/gl_backend/ngl_header.hpp>.
     The header will replace GL (and GLES) functions with macros. Under release
     the macros are to function pointers that automatically set themselves up
     correcty. For debug, the macros preceed and postceed each GL (and GLES)
     function call with error checking call backs so an application writer
     can quickly know what line/file triggered a GL error. If an application does
     not wish to use the macro system (and will also need to fetch function pointers
     somehow) but remain GL/GLES agnostic, it can instead include <fastuidraw/gl_backend/gl_header.hpp>.

  Long Version:

  The namespace gl_binding provides an interface for an application
  to specify to the GL module of FastUIDraw how to fetch function
  pointers (fastuidraw::gl_binding::get_proc_function()) and additional
  functionality of where to write/store GL error messages. An application
  can also use this functionality by including <fastuidraw/gl_backend/ngl_header.hpp>.
  The header <fastuidraw/gl_backend/ngl_header.hpp> will create a macro
  for each GL function. If FASTUIDRAW_DEBUG is defined, each GL call will be preceded by
  a callback and postceeded by another call back. The preceed callback to the
  GL call will call (if one is active) the implementation of GLCallBack::pre_call()
  of the active GLCallBack object. The post-process callback will repeatedly
  call glGetError (until it returns no error) to build an error-string.
  If the error string is non-empty, it is printed to stderr. In addition,
  regardless if the error-string is non-empty, the GLCallBack::post_call() is
  called of the active GLCallBack.

  This is implemented by creating a macro for each
  GL call. If FASTUIDRAW_DEBUG is not defined, none of these
  logging and error calls backs are executed.
  The mechanism is implemented by defining a macro
  for each GL function, hence using a GL function
  name as a function pointer will fail to compile
  and likely give an almost impossible to read
  error message. To use this, an application needs to
  only include <fastuidraw/gl_backend/ngl_header.hpp>.

  To fetch the function pointer of a GL function,
  use the macro <B>FASTUIDRAWglfunctionPointer</B> together
  with <B>FASTUIDRAWglfunctionExists</B>. The macro
  <B>FASTUIDRAWglfunctionPointer</B> will NEVER return
  a nullptr pointer. For the cases where the GL implementation
  does not have tha function, the function pointer
  returned will then point to a do-nothing function.
  To check if a GL implementation has a given function
  use the macro-function <B>FASTUIDRAWglfunctionExists</B>
  which returns non-zero if the GL implementation has
  the function. Some example code:

  \code
  //get a function pointer for a GL function
  //which takes no arguments and returns nothing.
  void (*functionPointer)(void) = nullptr;
  if(FASTUIDRAWglfunctionExists(glSomething)
    {
      functionPointer = FASTUIDRAWglfunctionPointer(glSomeFunction);
    }
  else
    {
      //FASTUIDRAWglfunctionPointer(glSomeFunction) is NOT nullptr,
      //rather it maps to a no-op function.
      //in this example we leave the value of
      //functionPointer as nullptr to indicate the function
      //is not supported by the GL implementation.
    }
  \endcode

  Calling a GL function through a function pointer
  will bypass the GL error checking though. One issue
  with using FASTUIDRAWglfunctionExists is that a number
  of GL implementations will return a function pointer,
  even if the implementation does not support it. As
  always, when fetching function pointers, one should
  check the GL version and GL extension string(s) to
  know if the GL implementation supports that function.

  The gl_binding system requires that an application provides
  a function which the binding system uses to fetch function
  pointers for the GL API, this is set via
  gl_binding::get_proc_function().
 */
namespace gl_binding {

/*!
  \brief
  A GLCallBack defines the interface for callbacks
  before and after each GL/GLES call.
 */
class GLCallBack:
    public reference_counted<GLCallBack>::default_base
{
public:
  /*!
    Ctor; on creation a GLCallBack is made active.
   */
  GLCallBack(void);

  ~GLCallBack();

  /*!
    Set if the GLCallBack is active.
   */
  void
  active(bool b);

  /*!
    Returns true if and only if the GLCallBack is active
   */
  bool
  active(void) const;

  /*!
    To be implemented by a derived class to record
    just before a GL call; if one calls GL functions
    within a callback, then one must call them through
    the function pointers \ref FASTUIDRAWglfunctionPointer()
    to prevent infinite recursion.
    \param call_string_value string showing call's values
    \param call_string_src string showing function call as it appears in source
    \param function_name name of function called
    \param function_ptr pointer to GL function originating the call
    \param src_file file of orignating GL call
    \param src_line line number of orignating GL call
   */
  virtual
  void
  pre_call(c_string call_string_values,
           c_string call_string_src,
           c_string function_name,
           void *function_ptr,
           c_string src_file, int src_line) = 0;

  /*!
    To be implemented by a derived class to record
    just after a GL call; if one calls GL functions
    within a callback, then one must call them through
    the function pointers \ref FASTUIDRAWglfunctionPointer()
    to prevent infinite recursion.
    \param call_string_value string showing call's values
    \param call_string_src string showing function call as it appears in source
    \param function_name name of function called
    \param error_string error string generated from calling glGetError
                        after GL call returns
    \param function_ptr pointer to GL function originating the call
    \param src_file file of orignating GL call
    \param src_line line number of orignating GL call
   */
  virtual
  void
  post_call(c_string call_string_values,
            c_string call_string_src,
            c_string function_name,
            c_string error_string,
            void *function_ptr,
            c_string src_file, int src_line) = 0;

  virtual
  void
  on_call_unloadable_function(c_string function_name)
  {
    FASTUIDRAWunused(function_name);
  }

  virtual
  void
  on_load_function_fail(c_string function_name)
  {
    FASTUIDRAWunused(function_name);
  }

private:
  void *m_d;
};

/*!
  Sets the function that the system uses
  to fetch the function pointers for GL or GLES.
  \param get_proc value to use, default is nullptr.
  \param fetch_functions if true, fetch all GL/GLES functions
                         immediately instead of fetching on
                         first call.
 */
void
get_proc_function(void* (*get_proc)(c_string),
                  bool fetch_functions = true);

/*!
  Fetches a GL/GLES function using the function fetcher
  passed to get_proc_function().
  \param function name of function to fetch
 */
void*
get_proc(c_string function);

}
/*! @} */
}
