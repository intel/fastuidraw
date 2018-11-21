/*!
 * \file fastuidraw_memory.hpp
 * \brief file fastuidraw_memory.hpp
 *
 * Adapted from: WRATHNew.hpp and WRATHmemory.hpp of WRATH:
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

/*!\addtogroup Utility
 * @{
 */

#include <cstdlib>
#include <fastuidraw/util/fastuidraw_memory_private.hpp>

/*!\def FASTUIDRAWnew
 * When creating FastUIDraw objects, one must use FASTUIDRAWnew instead of new
 * to create objects. For debug build of FastUIDraw, allocations with FASTUIDRAWnew
 * are tracked and at program exit a list of those objects not deleted by
 * FASTUIDRAWdelete are printed with the file and line number of the allocation.
 * For release builds of FastUIDraw, allocations are not tracked and std::malloc
 * is used to allocate memory. Do NOT use FASTUIDRAWnew for creating arrays
 * (i.e. p = new type[N]) as FASTUIDRAWdelete does not handle array deletion.
 */
#define FASTUIDRAWnew \
  ::new(__FILE__, __LINE__)

/*!\def FASTUIDRAWdelete
 * Use \ref FASTUIDRAWdelete to delete objects that were allocated with \ref
 * FASTUIDRAWnew. For debug builds of FastUIDraw, if the memory was not tracked
 * an error message is emitted.
 * \param ptr address of object to delete, value must be a return value
 *            from FASTUIDRAWnew
 */
#define FASTUIDRAWdelete(ptr) \
  do {                                                                  \
    fastuidraw::memory::check_object_exists(ptr, __FILE__, __LINE__);   \
    fastuidraw::memory::call_dtor(ptr);                                 \
    fastuidraw::memory::free_implement(ptr, __FILE__, __LINE__);        \
  } while(0)

/*!\def FASTUIDRAWmalloc
 * For debug build of FastUIDraw, allocations with \ref FASTUIDRAWmalloc are tracked and
 * at program exit a list of those objects not deleted by \ref FASTUIDRAWfree
 * are printed with the file and line number of the allocation. For release builds
 * of FastUIDraw, maps to std::malloc.
 */
#define FASTUIDRAWmalloc(size) \
  fastuidraw::memory::malloc_implement(size, __FILE__, __LINE__)

/*!\def FASTUIDRAWcalloc
 * For debug build of FastUIDraw, allocations with \ref FASTUIDRAWcalloc are tracked and
 * at program exit a list of those objects not deleted by \ref FASTUIDRAWfree
 * are printed with the file and line number of the allocation. For release builds
 * of FastUIDraw, maps to std::calloc.
 * \param nmemb number of elements to allocate
 * \param size size of each element in bytes
 */
#define FASTUIDRAWcalloc(nmemb, size) \
  fastuidraw::memory::calloc_implement(nmemb, size, __FILE__, __LINE__)

/*!\def FASTUIDRAWrealloc
 * For debug build of FastUIDraw, allocations with \ref FASTUIDRAWrealloc are tracked and
 * at program exit a list of those objects not deleted by \ref FASTUIDRAWfree
 * are printed with the file and line number of the allocation. For release builds
 * of FastUIDraw, maps to std::realloc.
 * \param ptr pointer at which to rellocate
 * \param size new size
 */
#define FASTUIDRAWrealloc(ptr, size) \
  fastuidraw::memory::realloc_implement(ptr, size, __FILE__, __LINE__)

/*!\def FASTUIDRAWfree
 * Use FASTUIDRAWfree for objects allocated with FASTUIDRAWmalloc,
 * FASTUIDRAWrealloc and FASTUIDRAWcalloc. For release builds of
 * FastUIDraw, maps to std::free.
 * \param ptr address of object to free
 */
#define FASTUIDRAWfree(ptr) \
  fastuidraw::memory::free_implement(ptr, __FILE__, __LINE__)

/*! @} */
