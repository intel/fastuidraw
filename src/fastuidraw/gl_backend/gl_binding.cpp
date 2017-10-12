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



#include <fastuidraw/util/util.hpp>
#include <iostream>
#include <sstream>
#include <list>
#include <mutex>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>

namespace
{
  class GLCallBackPrivate
  {
  public:
    typedef fastuidraw::gl_binding::GLCallBack GLCallBack;
    typedef std::list<GLCallBack*> GLCallBackList;

    GLCallBackList::iterator m_location;
    bool m_active;
  };

  class ngl_data:fastuidraw::noncopyable
  {
  public:
    ngl_data(void):
      m_proc(0)
    {}

    std::mutex m_mutex;
    GLCallBackPrivate::GLCallBackList m_callbacks;
    void* (*m_proc)(fastuidraw::c_string);
  };

  ngl_data&
  ngl(void)
  {
    static ngl_data R;
    return R;
  }

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
}

/////////////////////////////////////
//GLCallBack methods
fastuidraw::gl_binding::GLCallBack::
GLCallBack(void)
{
  GLCallBackPrivate *d;

  ngl().m_mutex.lock();
  d = FASTUIDRAWnew GLCallBackPrivate();
  d->m_location = ngl().m_callbacks.insert(ngl().m_callbacks.end(), this);
  d->m_active = true;
  ngl().m_mutex.unlock();

  m_d = d;
}

fastuidraw::gl_binding::GLCallBack::
~GLCallBack(void)
{
  GLCallBackPrivate *d;
  d = static_cast<GLCallBackPrivate*>(m_d);

  if (d->m_active)
    {
      ngl().m_mutex.lock();
      ngl().m_callbacks.erase(d->m_location);
      ngl().m_mutex.unlock();
    }
  FASTUIDRAWdelete(d);
}

bool
fastuidraw::gl_binding::GLCallBack::
active(void) const
{
  GLCallBackPrivate *d;
  d = static_cast<GLCallBackPrivate*>(m_d);
  return d->m_active;
}

void
fastuidraw::gl_binding::GLCallBack::
active(bool b)
{
  GLCallBackPrivate *d;
  d = static_cast<GLCallBackPrivate*>(m_d);

  if (b == d->m_active)
    {
      return;
    }

  ngl().m_mutex.lock();
  d->m_active = b;
  if(d->m_active)
    {
      d->m_location = ngl().m_callbacks.insert(ngl().m_callbacks.end(), this);
    }
  else
    {
      ngl().m_callbacks.erase(d->m_location);
    }
  ngl().m_mutex.unlock();
}

///////////////////////////////
// gl_binding methods
void
fastuidraw::gl_binding::
on_load_function_error(c_string fname)
{
  /* Should we just assume the loggers will
   * emit?
   */
  std::cerr << "Unable to load function: \"" << fname << "\"\n";
  ngl().m_mutex.lock();
  for(auto iter = ngl().m_callbacks.begin(),
        end =  ngl().m_callbacks.end();
      iter != end; ++iter)
    {
      (*iter)->on_load_function_fail(fname);
    }
  ngl().m_mutex.unlock();
}

void
fastuidraw::gl_binding::
call_unloadable_function(c_string fname)
{
  /* Should we just assume the loggers will
   * emit?
   */
  std::cerr << "Call to unloadable function: \"" << fname << "\"\n";
  ngl().m_mutex.lock();
  for(auto iter = ngl().m_callbacks.begin(),
        end =  ngl().m_callbacks.end();
      iter != end; ++iter)
    {
      (*iter)->on_call_unloadable_function(fname);
    }
  ngl().m_mutex.unlock();
}

void
fastuidraw::gl_binding::
pre_call(c_string call_string_values,
         c_string call_string_src,
         c_string function_name,
         void *function_ptr,
         c_string src_file, int src_line)
{
  ngl().m_mutex.lock();
  for(auto iter = ngl().m_callbacks.begin(),
        end =  ngl().m_callbacks.end();
      iter != end; ++iter)
    {
      (*iter)->pre_call(call_string_values, call_string_src,
                        function_name, function_ptr,
                        src_file, src_line);
    }
  ngl().m_mutex.unlock();
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
  if(!error.empty())
    {
      std::cerr << "[" << src_file << "," << src_line << "] "
                << call_string_values << "{"
                << error << "}\n";
    }

  ngl().m_mutex.lock();
  for(auto iter = ngl().m_callbacks.rbegin(),
        end =  ngl().m_callbacks.rend();
      iter != end; ++iter)
    {
      (*iter)->post_call(call_string_values, call_string_src,
                         function_name, error.c_str(),
                         function_ptr, src_file, src_line);
    }
  ngl().m_mutex.unlock();
}

void
fastuidraw::gl_binding::
get_proc_function(void* (*get_proc)(c_string), bool load_functions)
{
  ngl().m_proc = get_proc;
  if(load_functions && get_proc != nullptr)
    {
      load_all_functions(false);
    }
}

void*
fastuidraw::gl_binding::
get_proc(c_string function_name)
{
  void *return_value;
  if(ngl().m_proc)
    {
      return_value = ngl().m_proc(function_name);
    }
  else
    {
      return_value = nullptr;
    }
  return return_value;
}
