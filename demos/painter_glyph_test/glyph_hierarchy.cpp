#include "glyph_hierarchy.hpp"
#include "ostream_utility.hpp"

namespace
{
enum
  {
    splitting_size = 20,
    allow_split = 30
  };

class Element
{
public:
  Element(const BoundingBox<float> &b, unsigned int r):
    m_box(b),
    m_reference(r)
  {}

  BoundingBox<float> m_box;
  unsigned int m_reference;
};

class TreeBase:fastuidraw::noncopyable
{
public:
  explicit
  TreeBase(const BoundingBox<float> &bbox):
    m_bbox(bbox)
  {}

  virtual
  ~TreeBase()
  {}

  TreeBase*
  add(const BoundingBox<float> &bbox, unsigned int reference)
  {
    if (bbox.intersects(m_bbox))
      {
        return add_implement(bbox, reference);
      }
    else
      {
        return this;
      }
  }

  void
  query(const BoundingBox<float> &bbox,
        std::vector<unsigned int> *output)
  {
    if(bbox.intersects(m_bbox))
      {
        query_implement(bbox, output);
      }
  }

  const BoundingBox<float>&
  bounding_box(void)
  {
    return m_bbox;
  }

private:
  virtual
  TreeBase*
  add_implement(const BoundingBox<float> &bbox, unsigned int reference) = 0;

  virtual
  void
  query_implement(const BoundingBox<float> &bbox, std::vector<unsigned int> *output) = 0;

  BoundingBox<float> m_bbox;
};

class Node:public TreeBase
{
public:
  Node(const BoundingBox<float> &bbox,
       const BoundingBox<float> &bbox0,
       const std::vector<Element> &elements0,
       const BoundingBox<float> &bbox1,
       const std::vector<Element> &elements1);

  ~Node();

private:
  virtual
  TreeBase*
  add_implement(const BoundingBox<float> &bbox, unsigned int reference);

  virtual
  void
  query_implement(const BoundingBox<float> &bbox, std::vector<unsigned int> *output);

  fastuidraw::vecN<TreeBase*, 2> m_children;
};

class Leaf:public TreeBase
{
public:
  explicit
  Leaf(const BoundingBox<float> &bbox);

  Leaf(const BoundingBox<float> &bbox, const std::vector<Element> &elements);

  ~Leaf();

private:
  virtual
  TreeBase*
  add_implement(const BoundingBox<float> &bbox, unsigned int reference);

  virtual
  void
  query_implement(const BoundingBox<float> &bbox, std::vector<unsigned int> *output);

  std::vector<Element> m_elements;
};

}

///////////////////////////////////
// Node methods
Node::
Node(const BoundingBox<float> &bbox,
     const BoundingBox<float> &bbox0,
     const std::vector<Element> &elements0,
     const BoundingBox<float> &bbox1,
     const std::vector<Element> &elements1):
  TreeBase(bbox)
{
  m_children[0] = FASTUIDRAWnew Leaf(bbox0, elements0);
  m_children[1] = FASTUIDRAWnew Leaf(bbox1, elements1);
}

Node::
~Node()
{
  FASTUIDRAWdelete(m_children[0]);
  FASTUIDRAWdelete(m_children[1]);
}

TreeBase*
Node::
add_implement(const BoundingBox<float> &bbox, unsigned int reference)
{
  for (unsigned int i = 0; i < 2; ++i)
    {
      TreeBase *p;
      p = m_children[i]->add(bbox, reference);
      if (p != m_children[i])
        {
          FASTUIDRAWdelete(m_children[i]);
          m_children[i] = p;
        }
    }

  return this;
}

void
Node::
query_implement(const BoundingBox<float> &bbox,
      std::vector<unsigned int> *output)
{
  m_children[0]->query(bbox, output);
  m_children[1]->query(bbox, output);
}

///////////////////////////////////////////
// Leaf methods
Leaf::
Leaf(const BoundingBox<float> &bbox, const std::vector<Element> &elements):
  TreeBase(bbox),
  m_elements(elements)
{}

Leaf::
Leaf(const BoundingBox<float> &bbox):
  TreeBase(bbox)
{}

Leaf::
~Leaf()
{}

void
Leaf::
query_implement(const BoundingBox<float> &bbox,
                std::vector<unsigned int> *output)
{
  for (const auto &E : m_elements)
    {
      if (E.m_box.intersects(bbox))
        {
          output->push_back(E.m_reference);
        }
    }
}

TreeBase*
Leaf::
add_implement(const BoundingBox<float> &bbox, unsigned int reference)
{
  m_elements.push_back(Element(bbox, reference));
  if (m_elements.size() > splitting_size)
    {
      fastuidraw::vecN<std::vector<Element>, 2> split_x, split_y;
      fastuidraw::vecN<BoundingBox<float>, 2> split_x_bb, split_y_bb;
      unsigned int split_x_size(0), split_y_size(0);

      split_x_bb = bounding_box().split(0);
      split_y_bb = bounding_box().split(1);

      for (const Element &E : m_elements)
        {
          for (unsigned int i = 0; i < 2; ++i)
            {
              if (split_x_bb[i].intersects(E.m_box))
                {
                  split_x[i].push_back(E);
                  ++split_x_size;
                }
              if (split_y_bb[i].intersects(E.m_box))
                {
                  split_y[i].push_back(E);
                  ++split_y_size;
                }
            }

          FASTUIDRAWassert(split_x_bb[0].intersects(E.m_box) || split_x_bb[1].intersects(E.m_box));
          FASTUIDRAWassert(split_y_bb[0].intersects(E.m_box) || split_y_bb[1].intersects(E.m_box));
        }

      if (fastuidraw::t_min(split_x_size, split_y_size) <= allow_split)
        {
          if (split_x_size < split_y_size)
            {
              return FASTUIDRAWnew Node(bounding_box(),
                                        split_x_bb[0], split_x[0],
                                        split_x_bb[1], split_x[1]);
            }
          else
            {
              return FASTUIDRAWnew Node(bounding_box(),
                                        split_y_bb[0], split_y[0],
                                        split_y_bb[1], split_y[1]);
            }
        }
    }
  return this;
}

///////////////////////////////////
// GlyphHierarchy methods
GlyphHierarchy::
GlyphHierarchy(const BoundingBox<float> &bbox)
{
  m_root = FASTUIDRAWnew Leaf(bbox);
}

GlyphHierarchy::
~GlyphHierarchy()
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  FASTUIDRAWdelete(d);
}

void
GlyphHierarchy::
add(const BoundingBox<float> &bbox, unsigned int reference)
{
  TreeBase *d, *p;

  d = static_cast<TreeBase*>(m_root);
  p = d->add(bbox, reference);
  if (p != d)
    {
      FASTUIDRAWdelete(d);
      m_root = p;
    }
}

void
GlyphHierarchy::
query(const BoundingBox<float> &bbox, std::vector<unsigned int> *output)
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  d->query(bbox, output);
}
