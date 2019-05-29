/*!
 * \file data_buffer.cpp
 * \brief file data_buffer.cpp
 *
 * Copyright 2018 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <vector>
#include <fstream>
#include <cstring>
#include <fastuidraw/util/data_buffer.hpp>
#include <private/util_private.hpp>

namespace
{
  typedef std::vector<uint8_t> DataBufferBackingStorePrivate;
}

fastuidraw::DataBufferBackingStore::
DataBufferBackingStore(unsigned int num_bytes, uint8_t v)
{
  m_d = FASTUIDRAWnew DataBufferBackingStorePrivate(num_bytes, v);
}

fastuidraw::DataBufferBackingStore::
DataBufferBackingStore(c_array<const uint8_t> init_data)
{
  DataBufferBackingStorePrivate *d;

  d = FASTUIDRAWnew DataBufferBackingStorePrivate();
  m_d = d;

  d->resize(init_data.size());
  if (!d->empty())
    {
      DataBufferBackingStorePrivate &p(*d);
      std::memcpy(&p[0], init_data.c_ptr(), init_data.size());
    }
}

fastuidraw::DataBufferBackingStore::
DataBufferBackingStore(c_string filename)
{
  DataBufferBackingStorePrivate *d;

  d = FASTUIDRAWnew DataBufferBackingStorePrivate();
  m_d = d;

  std::ifstream file(filename, std::ios::binary);
  if (file)
    {
      std::ifstream::pos_type sz;

      file.seekg(0, std::ios::end);
      sz = file.tellg();
      d->resize(sz);

      c_array<char> dst;
      dst = make_c_array(*d).reinterpret_pointer<char>();

      file.seekg(0, std::ios::beg);
      file.read(dst.c_ptr(), dst.size());
    }
}

fastuidraw::DataBufferBackingStore::
~DataBufferBackingStore()
{
  DataBufferBackingStorePrivate *d;
  d = static_cast<DataBufferBackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::c_array<uint8_t>
fastuidraw::DataBufferBackingStore::
data(void)
{
  DataBufferBackingStorePrivate *d;
  d = static_cast<DataBufferBackingStorePrivate*>(m_d);
  return make_c_array(*d);
}
