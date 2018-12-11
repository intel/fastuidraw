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
   * An APICallbackSet represents a collection of functors
   * to be called before and after each function call from
   * a collection of functions. This class is used by the
   * fastuidraw::gl_binding and fastuidraw::egl_binding.
   */
  class APICallbackSet:fastuidraw::noncopyable
  {
  public:
    /*!
     * A Callback is a functor to be called before
     * and after each function.
     */
    class CallBack:public reference_counted<CallBack>::default_base
    {
    public:
      /*!
       * Ctor
       * \param parent APICallbackSet on which to attach the object
       */
      explicit
      CallBack(APICallbackSet *parent);

      virtual
      ~CallBack();

      /*!
       * Set if the CallBack is active.
       */
      void
      active(bool b);

      /*!
       * Returns true if and only if the CallBack is active
       */
      bool
      active(void) const;

      /*!
       * To be implemented by a derived class to record
       * just before a call.
       * \param call_string_values string showing call's values
       * \param call_string_src string showing function call as it appears in source
       * \param function_name name of function called
       * \param function_ptr pointer to function originating the call
       * \param src_file file of orignating call
       * \param src_line line number of orignating call
       */
      virtual
      void
      pre_call(c_string call_string_values,
               c_string call_string_src,
               c_string function_name,
               void *function_ptr,
               c_string src_file, int src_line) = 0;

      /*!
       * To be implemented by a derived class to record
       * just after a GL call.
       * \param call_string_values string showing call's values
       * \param call_string_src string showing function call as it appears in source
       * \param function_name name of function called
       * \param error_string error string generated
       * \param function_ptr pointer to function originating the call
       * \param src_file file of orignating call
       * \param src_line line number of orignating call
       */
      virtual
      void
      post_call(c_string call_string_values,
                c_string call_string_src,
                c_string function_name,
                c_string error_string,
                void *function_ptr,
                c_string src_file, int src_line) = 0;

      /*!
       * To be implemented by a derived class; called when a message
       * is emitted.
       * \param message message emitted.
       * \param src_file file of orignating message
       * \param src_line line number of orignating message
       */
      virtual
      void
      message(c_string message, c_string src_file, int src_line) = 0;

      /*!
       * To be optionally implemented by a derived class; called
       * when a function cannot be loaded/found.
       * \param function_name name of function that could not be loaded/found
       */
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
     * Ctor.
     * \param label label with which to identify the APICallbackSet,
     *              string is copied.
     */
    explicit
    APICallbackSet(c_string label);

    virtual
    ~APICallbackSet();

    /*!
     * Return the label passed in the ctor.
     */
    c_string
    label(void) const;

    /*!
     * Sets the function that the system uses
     * to fetch the function pointers.
     * \param get_proc value to use, default is nullptr.
     */
    void
    get_proc_function(void* (*get_proc)(c_string));

    /*!
     * Sets the function that the system uses
     * to fetch the function pointers.
     * \param datum client data pointer passed to function
     * \param get_proc value to use, default is nullptr.
     */
    void
    get_proc_function(void *datum,
                      void* (*get_proc)(void*, c_string));

    /*!
     * Fetches a function using the function fetcher
     * passed to get_proc_function().
     * \param function name of function to fetch
     */
    void*
    get_proc(c_string function);

    /*!
     * To be called by an implementation before issuing a function
     * call.
     */
    void
    pre_call(c_string call_string_values,
             c_string call_string_src,
             c_string function_name,
             void *function_ptr,
             c_string src_file, int src_line);

    /*!
     * To be called by an impementation after issuing a function
     * call.
     */
    void
    post_call(c_string call_string_values,
              c_string call_string_src,
              c_string function_name,
              c_string error_string,
              void *function_ptr,
              c_string src_file, int src_line);

    /*!
     * To be called by an implementation when attempting to
     * call a function whose function pointer is null
     */
    void
    call_unloadable_function(c_string fname);

    /*!
     * To be called by an implementation to emit a
     * message to each of the callbacks.
     */
    void
    message(c_string message, c_string src_file, int src_line);

  private:
    void *m_d;
  };
}
