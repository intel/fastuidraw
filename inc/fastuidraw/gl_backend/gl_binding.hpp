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
  for each GL function. If GL_DEBUG is defined, each GL call will be preceded by
  a callback and postceeded by another call back. If logging
  is on (see log_gl_commands(bool)), the precede call back
  will print (file, line and arguments) to the LoggerBase
  object specified by gl_binding::logger(LoggerBase*). The post-process
  call back will call glGetError until no errors are returned and
  log the call and errors to the LoggerBase object specified with
  gl_binding::logger(LoggerBase*). If logging is on (see
  gl_binding::log_gl_commands(bool)) then regardless of any errors
  a log message will be written indicating the GL call
  succeeded.

  This is implemented by creating a macro for each
  GL call. If GL_DEBUG is not defined, none of these
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
  a NULL pointer. For the cases where the GL implementation
  does not have tha function, the function pointer
  returned will then point to a do-nothing function.
  To check if a GL implementation has a given function
  use the macro-function <B>FASTUIDRAWglfunctionExists</B>
  which returns non-zero if the GL implementation has
  the function. Some example code:

  \code
  //get a function pointer for a GL function
  //which takes no arguments and returns nothing.
  void (*functionPointer)(void) = NULL;
  if(FASTUIDRAWglfunctionExists(glSomething)
    {
      functionPointer = FASTUIDRAWglfunctionPointer(glSomeFunction);
    }
  else
    {
      //FASTUIDRAWglfunctionPointer(glSomeFunction) is NOT NULL,
      //rather it maps to a no-op function.
      //in this example we leave the value of
      //functionPointer as NULL to indicate the function
      //is not supported by the GL implementation.
    }
  \endcode

  Calling a GL function through a function pointer
  will bypass the GL error checking though.

  The gl_binding system requires that an application provides
  a function which the binding system uses to fetch function
  pointers for the GL API, this is set via
  gl_binding::get_proc_function().
 */
namespace gl_binding {

/*!
  A LoggerBase defines the interface for logging
  GL commands
 */
class LoggerBase:noncopyable
{
public:
  virtual
  ~LoggerBase()
  {}

  /*!
    To be implemented by a derived class to record
    GL log.
    \param msg C-string of log-message with error results
    \param function_name GL function name
    \param file_name file of orignating GL call
    \param line line number of orignating GL call
    \param fptr pointer to GL function originating the call
   */
  virtual
  void
  log(const char *msg, const char *function_name,
      const char *file_name, int line,
      void* fptr) = 0;
};

/*!
  Set the LoggerBase object to which
  to send GL command logs. Initial value is
  NULL. Logging is only possbile to
  be active if GL_DEBUG is defined.
  \param l object to use, a NULL value indicates
           to not send logging commands anywhere
 */
void
logger(LoggerBase *l);

/*!
  Returns the LoggerBase object used
  to log GL commands, initial value is NULL.
 */
LoggerBase*
logger(void);


/*!
  Returns true if logs all
  GL API calls. If returns false,
  only log those GL API calls
  that generated GL errors.
  Default value is false.
  Logging is only possbile to
  be active if GL_DEBUG is defined.
 */
bool
log_gl_commands(void);

/*!
  Set if log all GL API calls.
  If false, only log those
  GL API calls that generated GL
  errors. If true log ALL GL
  API calls. Default value is false.
  \param v value to set for logging.
  Logging is only possbile to
  be active if GL_DEBUG is defined.
 */
void
log_gl_commands(bool v);

/*!
  Sets the function that the system uses
  to fetch the function pointers for GL or GLES.
  \param get_proc value to use, default is NULL.
 */
void
get_proc_function(void* (*get_proc)(const char*));

}
/*! @} */
}
