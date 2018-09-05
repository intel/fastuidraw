#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <vector>

#include "bounding_box.hpp"

#pragma once

class GlyphHierarchy:fastuidraw::noncopyable
{
public:
  enum
    {
      glyph_not_found = ~0u
    };

  explicit
  GlyphHierarchy(const BoundingBox<float> &bbox);

  ~GlyphHierarchy();

  void
  add(const BoundingBox<float> &bbox, unsigned int reference);

  void
  query(const BoundingBox<float> &bbox, std::vector<unsigned int> *output);

  unsigned int
  query(const fastuidraw::vec2 &p, BoundingBox<float> *out_bb);

private:
  void *m_root;
};
