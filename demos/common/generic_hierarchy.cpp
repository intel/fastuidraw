#include "generic_hierarchy.hpp"
#include "ostream_utility.hpp"

namespace
{
enum
  {
    splitting_size = 20,
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

  unsigned int
  query(const fastuidraw::vec2 &p,
        BoundingBox<float> *out_bb)
  {
    if (m_bbox.intersects(p))
      {
        return query_implement(p, out_bb);
      }
    else
      {
        return GenericHierarchy::not_found;
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
  add_implement(const BoundingBox<float> &bbox,
                unsigned int reference) = 0;

  virtual
  void
  query_implement(const BoundingBox<float> &bbox,
                  std::vector<unsigned int> *output) = 0;

  virtual
  unsigned int
  query_implement(const fastuidraw::vec2 &p,
                  BoundingBox<float> *out_bb) = 0;

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
  add_implement(const BoundingBox<float> &bbox,
                unsigned int reference);

  virtual
  void
  query_implement(const BoundingBox<float> &bbox,
                  std::vector<unsigned int> *output);

  virtual
  unsigned int
  query_implement(const fastuidraw::vec2 &p,
                  BoundingBox<float> *out_bb);

  fastuidraw::vecN<TreeBase*, 2> m_children;
  std::vector<Element> m_elements;
};

class Leaf:public TreeBase
{
public:
  explicit
  Leaf(const BoundingBox<float> &bbox);

  Leaf(const BoundingBox<float> &bbox,
       const std::vector<Element> &elements);

  ~Leaf();

private:
  virtual
  TreeBase*
  add_implement(const BoundingBox<float> &bbox,
                unsigned int reference);

  virtual
  void
  query_implement(const BoundingBox<float> &bbox,
                  std::vector<unsigned int> *output);

  virtual
  unsigned int
  query_implement(const fastuidraw::vec2 &p,
                BoundingBox<float> *out_bb);

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
  fastuidraw::vecN<bool, 2> child_takes;

  for (unsigned int i = 0; i < 2; ++i)
    {
      child_takes[i] = m_children[i]->bounding_box().intersects(bbox);
    }

  if (child_takes[0] && child_takes[1])
    {
      Element E(bbox, reference);
      m_elements.push_back(E);
    }
  else
    {
      unsigned int IDX;
      TreeBase *p;

      IDX = child_takes[0] ? 0u : 1u;
      p = m_children[IDX]->add(bbox, reference);
      if (p != m_children[IDX])
        {
          FASTUIDRAWdelete(m_children[IDX]);
          m_children[IDX] = p;
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
  for (const auto &E : m_elements)
    {
      if (E.m_box.intersects(bbox))
        {
          output->push_back(E.m_reference);
        }
    }
}

unsigned int
Node::
query_implement(const fastuidraw::vec2 &p,
                BoundingBox<float> *out_bb)
{
  int R;

  R = m_children[0]->query(p, out_bb);
  if (R != GenericHierarchy::not_found)
    {
      return R;
    }
  R = m_children[1]->query(p, out_bb);
  if (R != GenericHierarchy::not_found)
    {
      return R;
    }

  for (const auto &E : m_elements)
    {
      if (E.m_box.intersects(p))
        {
          *out_bb = E.m_box;
          return E.m_reference;
        }
    }
  return GenericHierarchy::not_found;
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

unsigned int
Leaf::
query_implement(const fastuidraw::vec2 &p,
                BoundingBox<float> *out_bb)
{
  for (const auto &E : m_elements)
    {
      if (E.m_box.intersects(p))
        {
          *out_bb = E.m_box;
          return E.m_reference;
        }
    }
  return GenericHierarchy::not_found;
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

      unsigned int allow_split;
      const float max_overlap(1.5f);
      allow_split = static_cast<unsigned int>(max_overlap * static_cast<float>(m_elements.size()));

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
// GenericHierarchy methods
GenericHierarchy::
GenericHierarchy(const BoundingBox<float> &bbox)
{
  m_root = FASTUIDRAWnew Leaf(bbox);
}

GenericHierarchy::
~GenericHierarchy()
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  FASTUIDRAWdelete(d);
}

void
GenericHierarchy::
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
GenericHierarchy::
query(const BoundingBox<float> &bbox, std::vector<unsigned int> *output)
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  d->query(bbox, output);
}

unsigned int
GenericHierarchy::
query(const fastuidraw::vec2 &p, BoundingBox<float> *out_bb)
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  return d->query(p, out_bb);
}
