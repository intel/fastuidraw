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
  @{
 */

#include <cstdlib>
#include <fastuidraw/util/checked_delete.hpp>
#include <fastuidraw/util/fastuidraw_memory_private.hpp>

/*!\def FASTUIDRAWnew
  For debug build of FastUIDraw, allocations with FASTUIDRAWnew are tracked and
  at program exit a list of those objects not deleted by FASTUIDRAWdelete
  are printed with the file and line number of the allocation. For release builds
  of FastUIDraw, FASTUIDRAWnew maps to new.
 */
#define FASTUIDRAWnew \
  ::new(__FILE__, __LINE__)

/*!\def FASTUIDRAWdelete
  Use FASTUIDRAWdelete for objects allocated with FASTUIDRAWnew.
  For release builds of FastUIDraw, maps to fastuidraw::checked_delete(p).
  \param ptr address of object to delete, value must be a return
             value of FASTUIDRAWnew
 */
#define FASTUIDRAWdelete(ptr) \
  do {                                                                  \
    fastuidraw::memory::object_deletion_message(ptr, __FILE__, __LINE__); \
    fastuidraw::checked_delete(ptr); } while(0)

/*!\def FASTUIDRAWdelete_array
  Use FASTUIDRAWdelete_array for arrays of objects allocated with FASTUIDRAWnew.
  For release builds of FastUIDraw, maps to fastuidraw::checked_array_delete().
  \param ptr address of array of objects to delete, value must be a return
             value of FASTUIDRAWnew
 */
#define FASTUIDRAWdelete_array(ptr) \
  do {                                                                  \
    fastuidraw::memory::object_deletion_message(ptr, __FILE__, __LINE__); \
    fastuidraw::checked_array_delete(ptr); } while(0)

/*!\def FASTUIDRAWmalloc
  For debug build of FastUIDraw, allocations with FASTUIDRAWmalloc are tracked and
  at program exit a list of those objects not deleted by FASTUIDRAWfree
  are printed with the file and line number of the allocation. For release builds
  of FastUIDraw, maps to std::malloc.
 */
#define FASTUIDRAWmalloc(size) \
  fastuidraw::memory::malloc_implement(size, __FILE__, __LINE__)

/*!\def FASTUIDRAWcalloc
  For debug build of FastUIDraw, allocations with FASTUIDRAWcalloc are tracked and
  at program exit a list of those objects not deleted by FASTUIDRAWfree
  are printed with the file and line number of the allocation. For release builds
  of FastUIDraw, maps to std::calloc.
  \param nmemb number of elements to allocate
  \param size size of each element in bytes
 */
#define FASTUIDRAWcalloc(nmemb, size) \
  fastuidraw::memory::calloc_implement(nmemb, size, __FILE__, __LINE__)

/*!\def FASTUIDRAWrealloc
  For debug build of FastUIDraw, allocations with FASTUIDRAWrealloc are tracked and
  at program exit a list of those objects not deleted by FASTUIDRAWfree
  are printed with the file and line number of the allocation. For release builds
  of FastUIDraw, maps to std::realloc.
  \param ptr pointer at whcih to rellocate
  \param size new size
 */
#define FASTUIDRAWrealloc(ptr, size) \
  fastuidraw::memory::realloc_implement(ptr, size, __FILE__, __LINE__)

/*!\def FASTUIDRAWfree
  Use FASTUIDRAWfree for objects allocated with FASTUIDRAWmalloc,
  FASTUIDRAWrealloc and FASTUIDRAWcalloc. For release builds of
  FastUIDraw, maps to std::free.
  \param ptr address of object to free
 */
#define FASTUIDRAWfree(ptr) \
  fastuidraw::memory::free_implement(ptr, __FILE__, __LINE__)

/*! @} */
