/*!
 * \file fastuidraw_memory.cpp
 * \brief file fastuidraw_memory.cpp
 *
 * Adapted from: WRATHNew.cpp and WRATHmemory.cpp of WRATH:
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


#include <map>
#include <iostream>
#include <iomanip>
#include <iosfwd>
#include <cstdlib>
#include <sstream>

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include "../private/util_private.hpp"

#ifdef FASTUIDRAW_DEBUG

namespace
{
  void
  bad_malloc_message(size_t size, const char *file, int line)
  {
    std::cerr << "Allocation at [" << file << "," << line << "] of "
              << size << " bytes failed\n";
  }

  class address_set_type:
    private std::map<const void*, std::pair<const char*,int> >
  {
  public:
    address_set_type(void)
    {}

    virtual
    ~address_set_type()
    {
      if(!empty())
        {
          print(std::cerr);
        }
    }

    bool
    present(const void *ptr);

    void
    track(const void *ptr, const char* file, int line);

    void
    untrack(const void *ptr, const char* file, int line);

    void
    print(std::ostream &ostr);

  private:
    fastuidraw::mutex m_mutex;
  };

  address_set_type&
  address_set(void)
  {
    static address_set_type retval;
    return retval;
  }

}

///////////////////////////////////////////////
// address_set_type methods
bool
address_set_type::
present(const void *ptr)
{
  bool return_value;

  m_mutex.lock();
  return_value = (find(ptr) != end());
  m_mutex.unlock();
  return return_value;
}

void
address_set_type::
track(const void *ptr, const char* file, int line)
{
  m_mutex.lock();
  insert(value_type(ptr, mapped_type(file, line)));
  m_mutex.unlock();
}

void
address_set_type::
untrack(const void *ptr, const char *file, int line)
{
  bool found;
  iterator iter;

  m_mutex.lock();

  iter = find(ptr);
  if(iter != end())
    {
      found = true;
      erase(iter);
    }
  else
    {
      found = false;
    }

  m_mutex.unlock();

  if(!found)
    {
      std::cerr << "Deletion from [" << file << ", "  << line
                << "] of untracked @" << ptr << "\n"
                << std::flush;
    }
}

void
address_set_type::
print(std::ostream &ostr)
{
  m_mutex.lock();

  for(const_iterator iter = begin(); iter != end(); ++iter)
    {
      ostr << const_cast<void*>(iter->first) << "[" << iter->second.first
           << "," << iter->second.second << "]\n";
    }

  m_mutex.unlock();
}

#endif

//////////////////////////////////////////////////////
// fastuidraw::memory methods
void
fastuidraw::memory::
object_deletion_message(const void *ptr, const char *file, int line)
{
  #ifdef FASTUIDRAW_DEBUG
    {
      if(!ptr)
        {
          return;
        }
      address_set().untrack(ptr, file, line);
    }
  #else
    {
      FASTUIDRAWunused(file);
      FASTUIDRAWunused(line);
      FASTUIDRAWunused(ptr);
    }
  #endif
}

void*
fastuidraw::memory::
malloc_implement(size_t size, const char *file, int line)
{
  void *return_value;

  if(size == 0)
    {
      return nullptr;
    }

  return_value = std::malloc(size);

  #ifdef FASTUIDRAW_DEBUG
    {
      if(!return_value)
        {
          bad_malloc_message(size, file, line);
        }
      else
        {
          address_set().track(return_value, file, line);
        }
    }
  #else
    {
      FASTUIDRAWunused(file);
      FASTUIDRAWunused(line);
    }
  #endif

  return return_value;
}

void*
fastuidraw::memory::
calloc_implement(size_t nmemb, size_t size, const char *file, int line)
{
  void *return_value;

  if(nmemb == 0 || size == 0)
    {
      return nullptr;
    }

  return_value = std::calloc(nmemb, size);

  #ifdef FASTUIDRAW_DEBUG
    {
      if(!return_value)
        {
          bad_malloc_message(size * nmemb, file, line);
        }
      else
        {
          address_set().track(return_value, file, line);
        }
    }
  #else
    {
      FASTUIDRAWunused(file);
      FASTUIDRAWunused(line);
    }
  #endif

  return return_value;
}

void*
fastuidraw::memory::
realloc_implement(void *ptr, size_t size, const char *file, int line)
{
  void *return_value;

  if(!ptr)
    {
      return malloc_implement(size, file, line);
    }

  if(size == 0)
    {
      free_implement(ptr, file, line);
      return nullptr;
    }

  #ifdef FASTUIDRAW_DEBUG
    {
      if(!address_set().present(ptr))
        {
          std::cerr << "Realloc from [" << file << ", "  << line
                    << "] of untracked @" << ptr << "\n"
                    << std::flush;
        }
    }
  #else
    {
      FASTUIDRAWunused(file);
      FASTUIDRAWunused(line);
    }
  #endif

  return_value = std::realloc(ptr, size);

  #ifdef FASTUIDRAW_DEBUG
    {
      if(return_value != ptr)
        {
          address_set().untrack(ptr, file, line);
          address_set().track(return_value, file, line);
        }
    }
  #endif

  return return_value;
}

void
fastuidraw::memory::
free_implement(void *ptr, const char *file, int line)
{
  #ifdef FASTUIDRAW_DEBUG
    {
      if(ptr)
        {
          address_set().untrack(ptr, file, line);
        }
    }
  #else
    {
      FASTUIDRAWunused(file);
      FASTUIDRAWunused(line);
    }
  #endif

  std::free(ptr);
}

void*
operator new(std::size_t n, const char *file, int line) throw ()
{
  return fastuidraw::memory::malloc_implement(n, file, line);
}

void*
operator new[](std::size_t n, const char *file, int line) throw ()
{
  return fastuidraw::memory::malloc_implement(n, file, line);
}

void
operator delete(void *ptr, const char *file, int line) throw()
{
  fastuidraw::memory::free_implement(ptr, file, line);
}

void
operator delete[](void *ptr, const char *file, int line) throw()
{
  fastuidraw::memory::free_implement(ptr, file, line);
}
