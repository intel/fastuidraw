#pragma once

#include <ostream>
#include <string>
#include <fastuidraw/util/reference_counted.hpp>

class StreamHolder:
  public fastuidraw::reference_counted<StreamHolder>::concurrent
{
public:
  StreamHolder(const std::string &filename);
  ~StreamHolder();

  std::ostream&
  stream(void)
  {
    return *m_stream;
  }
private:
  std::ostream *m_stream;
  bool m_delete_stream;
};
