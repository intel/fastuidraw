/*!
 * \file painter_attribute_data_filler_path_fill.cpp
 * \brief file painter_attribute_data_filler_path_fill.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#include <algorithm>
#include <fastuidraw/painter/painter_attribute_data_filler_path_fill.hpp>

namespace
{
  class PathFillerPrivate
  {
  public:
    PathFillerPrivate(const fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath> &p):
      m_path(p)
    {}
    fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath> m_path;
  };

  class WindingSelectorChunk:public fastuidraw::WindingSelectorChunkBase
  {
  public:
    static
    unsigned int
    static_chunk_from_winding_number(int winding_number);

    static
    bool
    static_winding_number_from_chunk(unsigned int chunk, int &winding_number);

    static
    int
    static_winding_number_from_chunk(unsigned int chunk)
    {
      bool b;
      int r;

      b = static_winding_number_from_chunk(chunk, r);
      assert(b);
      FASTUIDRAWunused(b);
      assert(static_chunk_from_winding_number(r) == chunk);

      return r;
    }

    virtual
    bool
    common_attribute_data(void) const
    {
      return true;
    }

    virtual
    unsigned int
    chunk_from_fill_rule(enum fastuidraw::PainterEnums::fill_rule_t fill_rule) const
    {
      return fill_rule;
    }

    virtual
    unsigned int
    chunk_from_winding_number(int winding_number) const
    {
      return static_chunk_from_winding_number(winding_number);
    }

    virtual
    bool
    winding_number_from_chunk(unsigned int chunk, int &winding_number) const
    {
      return static_winding_number_from_chunk(chunk, winding_number);
    }
  };

  fastuidraw::PainterAttribute
  generate_attribute(const fastuidraw::vec2 &src)
  {
    fastuidraw::PainterAttribute dst;

    dst.m_attrib0 = fastuidraw::pack_vec4(src.x(), src.y(), 0.0f, 0.0f);
    dst.m_attrib1 = fastuidraw::uvec4(0u, 0u, 0u, 0u);
    dst.m_attrib2 = fastuidraw::uvec4(0u, 0u, 0u, 0u);

    return dst;
  }
}

////////////////////////////////////////////
// WindingSelectorChunk methods
unsigned int
WindingSelectorChunk::
static_chunk_from_winding_number(int winding_number)
{
  /* basic idea:
     - start counting at fill_rule_data_count
     - ordering is: 1, -1, 2, -2, ...
  */
  int value, sg;

  if(winding_number == 0)
    {
      return fastuidraw::PainterEnums::complement_nonzero_fill_rule;
    }

  value = std::abs(winding_number);
  sg = (winding_number < 0) ? 1 : 0;
  return fastuidraw::PainterEnums::fill_rule_data_count + sg + 2 * (value - 1);
}

bool
WindingSelectorChunk::
static_winding_number_from_chunk(unsigned int chunk, int &winding_number)
{
  if(chunk == fastuidraw::PainterEnums::complement_nonzero_fill_rule)
    {
      winding_number = 0;
      return true;
    }

  if(chunk >= fastuidraw::PainterEnums::fill_rule_data_count)
    {
      int idx, abs_winding;

      idx = chunk - fastuidraw::PainterEnums::fill_rule_data_count;
      abs_winding = 1 + idx / 2;
      winding_number = (idx & 1) ? -abs_winding : abs_winding;
      return true;
    }

  return false;
}

//////////////////////////////////////////////////////////
// fastuidraw::PainterAttributeDataFillerPathFill methods
fastuidraw::PainterAttributeDataFillerPathFill::
PainterAttributeDataFillerPathFill(const reference_counted_ptr<const FilledPath> &path)
{
  m_d = FASTUIDRAWnew PathFillerPrivate(path);
}

fastuidraw::PainterAttributeDataFillerPathFill::
~PainterAttributeDataFillerPathFill()
{
  PathFillerPrivate *d;
  d = reinterpret_cast<PathFillerPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath>&
fastuidraw::PainterAttributeDataFillerPathFill::
path(void) const
{
  PathFillerPrivate *d;
  d = reinterpret_cast<PathFillerPrivate*>(m_d);
  assert(d != NULL);
  return d->m_path;
}

void
fastuidraw::PainterAttributeDataFillerPathFill::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_increments) const
{
  const FilledPath *p(path().get());

  number_z_increments = 0;
  if(p == NULL || p->winding_numbers().empty())
    {
      number_attributes = 0;
      number_indices = 0;
      number_attribute_chunks = 0;
      number_index_chunks = 0;
      return;
    }

  number_attributes = p->points().size();
  number_attribute_chunks = 1;

  number_indices = p->odd_winding_indices().size()
    + p->nonzero_winding_indices().size()
    + p->even_winding_indices().size()
    + p->zero_winding_indices().size();
  for(const_c_array<int>::iterator iter = p->winding_numbers().begin(),
        end = p->winding_numbers().end(); iter != end; ++iter)
    {
      if(*iter != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          number_indices += p->indices(*iter).size();
        }
    }

  /* now get how big the index_chunks really needs to be
   */
  int smallest_winding(p->winding_numbers().front());
  int largest_winding(p->winding_numbers().back());
  unsigned int largest_winding_idx(WindingSelectorChunk::static_chunk_from_winding_number(largest_winding));
  unsigned int smallest_winding_idx(WindingSelectorChunk::static_chunk_from_winding_number(smallest_winding));
  number_index_chunks = 1 + std::max(largest_winding_idx, smallest_winding_idx);
}

void
fastuidraw::PainterAttributeDataFillerPathFill::
fill_data(c_array<PainterAttribute> attributes,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attrib_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements,
          c_array<int> index_adjusts) const
{
  const FilledPath *p(path().get());
  if(p == NULL || p->winding_numbers().empty())
    {
      return;
    }

  assert(attributes.size() == p->points().size());
  assert(attrib_chunks.size() == 1);
  assert(zincrements.empty());
  FASTUIDRAWunused(zincrements);

  /* generate attribute data
   */
  std::transform(p->points().begin(), p->points().end(),
                 attributes.begin(), generate_attribute);
  attrib_chunks[0] = attributes;

  unsigned int current(0);

#define GRAB_MACRO(enum_name, function_name) do {               \
    c_array<PainterIndex> dst;                                  \
    dst = index_data.sub_array(current, p->function_name().size()); \
    std::copy(p->function_name().begin(),                           \
              p->function_name().end(), dst.begin());           \
    index_chunks[PainterEnums::enum_name] = dst;                \
    index_adjusts[PainterEnums::enum_name] = 0;                 \
    current += dst.size();                                      \
  } while(0)

  GRAB_MACRO(odd_even_fill_rule, odd_winding_indices);
  GRAB_MACRO(nonzero_fill_rule, nonzero_winding_indices);
  GRAB_MACRO(complement_odd_even_fill_rule, even_winding_indices);
  GRAB_MACRO(complement_nonzero_fill_rule, zero_winding_indices);

#undef GRAB_MACRO

  for(const_c_array<int>::iterator iter = p->winding_numbers().begin(),
        end = p->winding_numbers().end(); iter != end; ++iter)
    {
      if(*iter != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          c_array<PainterIndex> dst;
          const_c_array<unsigned int> src;
          unsigned int idx;

          idx = WindingSelectorChunk::static_chunk_from_winding_number(*iter);
          assert(*iter == WindingSelectorChunk::static_winding_number_from_chunk(idx));

          src = p->indices(*iter);
          dst = index_data.sub_array(current, src.size());
          assert(dst.size() == src.size());

          std::copy(src.begin(), src.end(), dst.begin());

          index_chunks[idx] = dst;
          index_adjusts[idx] = 0;
          current += dst.size();
        }
    }
}

fastuidraw::reference_counted_ptr<fastuidraw::WindingSelectorChunkBase>
fastuidraw::PainterAttributeDataFillerPathFill::
chunk_selector(void)
{
  return FASTUIDRAWnew WindingSelectorChunk();
}
