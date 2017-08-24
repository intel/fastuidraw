#include <vector>
#include <fstream>
#include <fastuidraw/util/data_buffer.hpp>
#include "../private/util_private.hpp"

namespace
{
  typedef std::vector<uint8_t> DataBufferPrivate;
}

fastuidraw::DataBuffer::
DataBuffer(unsigned int num_bytes)
{
  m_d = FASTUIDRAWnew DataBufferPrivate(num_bytes);
}

fastuidraw::DataBuffer::
DataBuffer(const char *filename)
{
  DataBufferPrivate *d;

  d = FASTUIDRAWnew DataBufferPrivate();
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

fastuidraw::DataBuffer::
~DataBuffer()
{
  DataBufferPrivate *d;
  d = static_cast<DataBufferPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::c_array<uint8_t>
fastuidraw::DataBuffer::
data(void)
{
  DataBufferPrivate *d;
  d = static_cast<DataBufferPrivate*>(m_d);
  return make_c_array(*d);
}

fastuidraw::const_c_array<uint8_t>
fastuidraw::DataBuffer::
data(void) const
{
  const DataBufferPrivate *d;
  d = static_cast<DataBufferPrivate*>(m_d);
  return make_c_array(*d);
}
