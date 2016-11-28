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



#include <assert.h>
#include <iostream>
#include <sstream>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>

namespace
{
  class ngl_data:fastuidraw::noncopyable
  {
  public:
    ngl_data(void):
      m_log(NULL),
      m_log_all(false),
      m_proc(0)
    {}

    fastuidraw::gl_binding::LoggerBase *m_log;
    bool m_log_all;
    void* (*m_proc)(const char*);
  };

  ngl_data&
  ngl(void)
  {
    static ngl_data R;
    return R;
  }
}

bool
fastuidraw::gl_binding::
log_gl_commands(void)
{
  return ngl().m_log_all;
}

void
fastuidraw::gl_binding::
log_gl_commands(bool v)
{
  ngl().m_log_all = v;
}

fastuidraw::gl_binding::LoggerBase*
fastuidraw::gl_binding::
logger(void)
{
  return ngl().m_log;
}

void
fastuidraw::gl_binding::
logger(LoggerBase *ptr)
{
  ngl().m_log = ptr;
}

void
fastuidraw::gl_binding::
on_load_function_error(const char *fname)
{
  std::cerr << "Unable to load function: \"" << fname << "\"\n";
}


void
fastuidraw::gl_binding::
ErrorCheck(const char *call, const char *src_call,
           const char *function_name,
           const char *fileName, int line,
           void* fptr)
{
  int errorcode;
  int count;

  (void)src_call;

  if(logger() == NULL)
    {
      return;
    }

  if(fptr == FASTUIDRAWglfunctionPointer(glGetError))
    {
      return;
    }

  errorcode = glGetError();
  if(errorcode == GL_NO_ERROR and !log_gl_commands())
    {
      return;
    }

  std::ostringstream str;

  str << "[" << fileName << "," << line << "] " << call << "{";
  for(count = 0; errorcode != GL_NO_ERROR; ++count, errorcode = glGetError() )
    {
      if(count != 0)
        {
          str << ",";
        }

      switch(errorcode)
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
          str << "\n\tUnknown errorcode: 0x" << std::hex << errorcode;
        }
    }
  if(count == 0)
    {
      str << "Post-Call";
    }
  str << "}\n";

  logger()->log(str.str().c_str(), function_name, fileName, line, fptr);
}


void
fastuidraw::gl_binding::
preErrorCheck(const char *call, const char *src_call,
              const char *function_name,
              const char *fileName, int line,
              void* fptr)
{
  (void)function_name;
  (void)fptr;
  (void)src_call;

  if(log_gl_commands() && fptr != FASTUIDRAWglfunctionPointer(glGetError) && logger())
    {
      std::ostringstream str;
      str << "[" << fileName << "," << line << "] " << call << "{Pre-Call}\n";
      logger()->log(str.str().c_str(), function_name, fileName, line, fptr);
    }
}


void
fastuidraw::gl_binding::
get_proc_function(void* (*get_proc)(const char*))
{
  ngl().m_proc = get_proc;
}

void*
fastuidraw::gl_binding::
loadFunction(const char *name)
{
  void *return_value;
  if(ngl().m_proc)
    {
      return_value = ngl().m_proc(name);
    }
  else
    {
      return_value = NULL;
    }
  return return_value;
}
