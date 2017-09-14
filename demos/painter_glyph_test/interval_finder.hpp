#pragma once

#include <vector>
#include <map>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>

template<typename T, unsigned int SplitThreshhold = 4>
class IntervalFinder:fastuidraw::noncopyable
{
public:
  IntervalFinder(float begin, float end);
  ~IntervalFinder();

  void
  add_entry(fastuidraw::range_type<float> interval,
            T value);

  void
  find_entries(float x, std::vector<T> *dst) const;

private:
  enum
    {
      child0_count,
      child1_count,
    };

  typedef std::pair<fastuidraw::range_type<float>, T> entry;

  bool
  have_children(void) const
  {
    bool b0(m_children[0]);
    bool b1(m_children[1]);
    FASTUIDRAWassert(b0 == b1);
    return m_children[0];
  }

  void
  make_children(void);

  fastuidraw::range_type<float> m_region;
  float m_midpoint;
  fastuidraw::uvec2 m_counts;
  std::vector<entry> m_entries;
  fastuidraw::vecN<IntervalFinder*, 2> m_children;
};

template<typename T, unsigned int SplitThreshhold>
IntervalFinder<T, SplitThreshhold>::
IntervalFinder(float begin, float end):
  m_region(begin, end),
  m_midpoint(0.5f * (begin + end)),
  m_children(nullptr, nullptr),
  m_counts(0, 0)
{
  if(m_region.m_begin > m_region.m_end)
    {
      std::swap(m_region.m_begin, m_region.m_end);
    }
}

template<typename T, unsigned int SplitThreshhold>
IntervalFinder<T, SplitThreshhold>::
~IntervalFinder()
{
  if(have_children())
    {
      FASTUIDRAWdelete(m_children[0]);
      FASTUIDRAWdelete(m_children[1]);
    }
}

template<typename T, unsigned int SplitThreshhold>
void
IntervalFinder<T, SplitThreshhold>::
add_entry(fastuidraw::range_type<float> interval,
          T value)
{
  if(interval.m_begin > interval.m_end)
    {
      std::swap(interval.m_begin, interval.m_end);
    }

  if(!have_children())
    {
      m_entries.push_back(entry(interval, value));
      if(interval.m_end < m_midpoint)
        {
          ++m_counts[child0_count];
        }
      else if(interval.m_begin > m_midpoint)
        {
          ++m_counts[child1_count];
        }

      if(m_counts[child0_count] > SplitThreshhold
         || m_counts[child1_count] > SplitThreshhold)
        {
          make_children();
        }
    }
  else
    {
      if(interval.m_end < m_midpoint)
        {
          m_children[0]->add_entry(interval, value);
        }
      else if(interval.m_begin > m_midpoint)
        {
          m_children[1]->add_entry(interval, value);
        }
      else
        {
          m_entries.push_back(entry(interval, value));
        }
    }
}

template<typename T, unsigned int SplitThreshhold>
void
IntervalFinder<T, SplitThreshhold>::
find_entries(float x, std::vector<T> *dst) const
{
  if (x < m_region.m_begin || x > m_region.m_end)
    {
      return;
    }

  for(const auto &e : m_entries)
    {
      if(x >= e.first.m_begin && x <= e.first.m_end)
        {
          dst->push_back(e.second);
        }
    }

  if(have_children())
    {
      m_children[0]->find_entries(x, dst);
      m_children[1]->find_entries(x, dst);
    }
}

template<typename T, unsigned int SplitThreshhold>
void
IntervalFinder<T, SplitThreshhold>::
make_children(void)
{
  std::vector<entry> tmp;

  FASTUIDRAWassert(!have_children());
  tmp.swap(m_entries);

  m_children[0] = FASTUIDRAWnew IntervalFinder(m_region.m_begin, m_midpoint);
  m_children[1] = FASTUIDRAWnew IntervalFinder(m_midpoint, m_region.m_end);

  for(const auto &e : tmp)
    {
      if(e.first.m_end < m_midpoint)
        {
          m_children[0]->m_entries.push_back(e);
        }
      else if(e.first.m_begin > m_midpoint)
        {
          m_children[1]->m_entries.push_back(e);
        }
      else
        {
          m_entries.push_back(e);
        }
    }
}
