#include <vector>
#include <fstream>
#include <fastuidraw/util/data_buffer.hpp>
#include "../private/util_private.hpp"

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
DataBufferBackingStore(const char *filename)
{
  DataBufferBackingStorePrivate *d;

  d = FASTUIDRAWnew DataBufferBackingStorePrivate();
  m_d = d;

  std::ifstream file(filename, std::ios::binary);
  if(file)
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
