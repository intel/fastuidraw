/*!
 * \file fastuidraw_memory_private.hpp
 * \brief file fastuidraw_memory_private.hpp
 *
 * Copyright 2016 by Intel.
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


/* Private functions needed for macros in fastuidraw_memory.hpp
 */

#pragma once

#include <cstddef>

/*!
 * Internal routine used by FASTUIDRAWnew, do not use directly.
 */
void*
operator new(std::size_t n, const char *file, int line) throw ();

/*!
 * Internal routine used by FASTUIDRAWnew, do not use directly.
 */
void*
operator new[](std::size_t n, const char *file, int line) throw ();

/*!
 * Internal routine used by FASTUIDRAWnew, do not use directly.
 */
void
operator delete(void *ptr, const char *file, int line) throw();

/*!
 * Internal routine used by FASTUIDRAWnew, do not use directly.
 */
void
operator delete[](void *ptr, const char *file, int line) throw();

namespace fastuidraw
{

namespace memory
{
  /*!
   * Private function used by macro FASTUIDRAWdelete, do NOT call.
   */
  void
  check_object_exists(const void *ptr, const char *file, int line);

  /*!
   * Private function used by macro FASTUIDRAWdelete, do NOT call.
   */
  template<typename T>
  void
  call_dtor(T *p)
  {
    p->~T();
  }

  /*!
   * Private function used by macro FASTUIDRAWmalloc, do NOT call.
   */
  void*
  malloc_implement(size_t size, const char *file, int line);

  /*!
   * Private function used by macro FASTUIDRAWcalloc, do NOT call.
   */
  void*
  calloc_implement(size_t nmemb, size_t size, const char *file, int line);

  /*!
   * Private function used by macro FASTUIDRAWrealloc, do NOT call.
   */
  void*
  realloc_implement(void *ptr, size_t size, const char *file, int line);

  /*!
   * Private function used by macro FASTUIDRAWfree, do NOT call.
   */
  void
  free_implement(void *ptr, const char *file, int line);

} //namespace memory

} //namespace fastuidraw
