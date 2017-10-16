/*!
 * \file egl_binding.hpp
 * \brief file egl_binding.hpp
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
#include <fastuidraw/util/api_callback.hpp>

namespace fastuidraw {


/*!\addtogroup GLUtility
  @{
 */

/*!
  \brief
  Provides interface for application to use EGL where function pointers
  are auto-resolved transparently and under debug provides error checking.
  Built as a seperate libNEGL.

  Short version:
   - application should call fastuidraw::egl_binding::get_proc_function()
     to set the function which will be used to fetch EGL function pointers.
   - If an application wishes, it can include <fastuidraw/ngl_egl.hpp>.
     The header will replace EGL functions with macros. Under release
     the macros are to function pointers that automatically set themselves up
     correcty. For debug, the macros preceed and postceed each EGL
     function call with error checking call backs so an application writer
     can quickly know what line/file triggered an EGL error. If an application does
     not wish to use the macro system (and will also need to fetch function pointers
     somehow), it can just include EGL/egl.h (and optionally EGL/eglext.h).
   - When using libNEGL, because NEGL automatically calls eglGetError() after each
     EGL call, one cannot use eglGetError() calls to determine how to recover.
     To get functionality of eglGetError() and application should call
     egl_binding::get_error() which returns the most recent EGL error code
     (and resets its internal value to EGL_SUCCESS).

  Long Version:

  The namespace egl_binding provides an interface for an application
  to specify how to fetch EGL function pointers (see
  fastuidraw::egl_binding::get_proc_function()) and additional
  functionality of where to write/store EGL error messages. An application
  can also use this functionality by including <fastuidraw/ngl_egl.hpp>.
  The header will create a macro for each EGL function. If FASTUIDRAW_DEBUG
  is defined, each EGL call will be preceded by a callback and postceeded by
  another call back. The preceed callback to the EGL call will call the
  implementation of CallbackEGL::pre_call() of each active CallbackEGL object.
  The post-process callback will repeatedly call eglGetError (until it returns
  no error) to build an error-string. If the error string is non-empty, it
  is printed to stderr. In addition, regardless if the error-string is non-empty,
  CallbackEGL::post_call() of each active CallbackEGL is called.

  This is implemented by creating a macro for each
  EGL call. If FASTUIDRAW_DEBUG is not defined, none of these
  logging and error calls backs are executed.
  The mechanism is implemented by defining a macro
  for each EGL function, hence using a EGL function
  name as a function pointer will fail to compile
  and likely give an almost impossible to read
  error message.

  To fetch the function pointer of a EGL function,
  use the macro <B>FASTUIDRAWeglfunctionPointer</B> together
  with <B>FASTUIDRAWeglfunctionExists</B>. The macro
  <B>FASTUIDRAWeglfunctionPointer</B> will NEVER return
  a nullptr pointer. For the cases where the EGL implementation
  does not have that function, the function pointer
  returned will then point to a do-nothing function.
  To check if a EGL implementation has a given function
  use the macro-function <B>FASTUIDRAWeglfunctionExists</B>
  which returns non-zero if the EGL implementation has
  the function. Some example code:

  \code
  //get a function pointer for a EGL function
  //which takes no arguments and returns nothing.
  void (*functionPointer)(void) = nullptr;
  if(FASTUIDRAWeglfunctionExists(eglSomething)
    {
      functionPointer = FASTUIDRAWeglfunctionPointer(eglSomeFunction);
    }
  else
    {
      //FASTUIDRAWeglfunctionPointer(eglSomeFunction) is NOT nullptr,
      //rather it maps to a no-op function.
      //in this example we leave the value of
      //functionPointer as nullptr to indicate the function
      //is not supported by the EGL implementation.
    }
  \endcode

  Calling a EGL function through a function pointer
  will bypass the EGL error checking ad callbacks though.
  One issue with using FASTUIDRAWeglfunctionExists is that a
  number of EGL implementations will return a function pointer,
  even if the implementation does not support it. As always,
  when fetching function pointers, one should check the EGL
  version and EGL extension string(s) to know if the EGL
  implementation supports that function.

  The egl_binding system requires that an application provides
  a function which the binding system uses to fetch function
  pointers for the EGL API, this is set via
  egl_binding::get_proc_function().
 */
namespace egl_binding {

/*!
  \brief
  A CallbackEGL defines the interface (via its base class)
  for callbacks before and after each EGL call.
 */
class CallbackEGL:public APICallbackSet::CallBack
{
public:
  CallbackEGL(void);
};

/*!
  Returns the most recent EGL error code; an application should call
  get_error() instead of eglGetError() to understand EGL errors because
  NEGL dispatch automatically calls eglGetError after each EGL API
  call which resets the error.
 */
EGLint
get_error(void);

/*!
  Sets the function that the system uses
  to fetch the function pointers for EGL or EGLES.
  \param get_proc value to use, default is nullptr.
  \param fetch_functions if true, fetch all EGL functions
                         immediately instead of fetching on
                         first call.
 */
void
get_proc_function(void* (*get_proc)(c_string),
                  bool fetch_functions = true);

/*!
  Fetches a EGL function using the function fetcher
  passed to get_proc_function().
  \param function name of function to fetch
 */
void*
get_proc(c_string function);

}
/*! @} */
}
