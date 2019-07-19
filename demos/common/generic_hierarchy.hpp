#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <vector>

#include "bounding_box.hpp"

#ifndef FASTUIDRAW_DEMO_GENERIC_HIERARCHY_HPP
#define FASTUIDRAW_DEMO_GENERIC_HIERARCHY_HPP

class GenericHierarchy:fastuidraw::noncopyable
{
public:
  enum
    {
      not_found = ~0u
    };

  explicit
  GenericHierarchy(const BoundingBox<float> &bbox);

  ~GenericHierarchy();

  void
  add(const BoundingBox<float> &bbox, unsigned int reference);

  void
  query(const BoundingBox<float> &bbox, std::vector<unsigned int> *output);

  unsigned int
  query(const fastuidraw::vec2 &p, BoundingBox<float> *out_bb);

private:
  void *m_root;
};

#endif
