/*!
 * \file api_callback.hpp
 * \brief file api_callback.hpp
 *
 * Copyright 2017 by Intel.
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

#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
  /*!
    An APICallbackSet represents a collection of functors
    to be called before and after each function call from
    a collection of functions. This class is used by the
    \ref fastuidraw::gl_binding.
   */
  class APICallbackSet:fastuidraw::noncopyable
  {
  public:
    /*!
      A Callback is a functor to be called before
      and after each function.
     */
    class CallBack:reference_counted<CallBack>::default_base
    {
    public:
      /*!
        Ctor
        \param parent APICallbackSet on which to attach the object
       */
      explicit
      CallBack(APICallbackSet *parent);

      virtual
      ~CallBack();

      /*!
        Set if the CallBack is active.
      */
      void
      active(bool b);

      /*!
        Returns true if and only if the CallBack is active
      */
      bool
      active(void) const;

      /*!
        To be implemented by a derived class to record
        just before a call.
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
        just after a GL call.
        \param call_string_value string showing call's values
        \param call_string_src string showing function call as it appears in source
        \param function_name name of function called
        \param error_string error string generated
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

    private:
      void *m_d;
    };

    /*!
      Ctor.
     */
    APICallbackSet(void);

    virtual
    ~APICallbackSet();

    /*!
      Sets the function that the system uses
      to fetch the function pointers for GL or GLES.
      \param get_proc value to use, default is nullptr.
      \param fetch_functions if true, fetch all GL/GLES functions
                             immediately instead of fetching on
                             first call.
    */
    void
    get_proc_function(void* (*get_proc)(c_string));

    /*!
      Fetches a GL/GLES function using the function fetcher
      passed to get_proc_function().
      \param function name of function to fetch
    */
    void*
    get_proc(c_string function);

  protected:
    /*!
      To be called by an implementation before issuing a function
      call.
     */
    void
    pre_call(c_string call_string_values,
             c_string call_string_src,
             c_string function_name,
             void *function_ptr,
             c_string src_file, int src_line);

    /*!
      To be called by an impementation after issuing a function
      call.
     */
    void
    post_call(c_string call_string_values,
              c_string call_string_src,
              c_string function_name,
              c_string error_string,
              void *function_ptr,
              c_string src_file, int src_line);

    /*!
      To be called by an implementation when attempting to
      call a function whose function pointer is null
     */
    void
    call_unloadable_function(c_string fname);

  private:
    void *m_d;
  };
}
