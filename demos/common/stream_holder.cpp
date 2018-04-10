#include <iostream>
#include <fstream>
#include "stream_holder.hpp"

///////////////////////////
// StreamHolder methods
StreamHolder::
StreamHolder(const std::string &filename):
  m_delete_stream(false)
{
  if (filename == "stderr")
    {
      m_stream = &std::cerr;
    }
  else if (filename == "stdout")
    {
      m_stream = &std::cout;
    }
  else
    {
      m_delete_stream = true;
      m_stream = FASTUIDRAWnew std::ofstream(filename.c_str());
    }
}

StreamHolder::
~StreamHolder()
{
  if (m_delete_stream)
    {
      FASTUIDRAWdelete(m_stream);
    }
}
