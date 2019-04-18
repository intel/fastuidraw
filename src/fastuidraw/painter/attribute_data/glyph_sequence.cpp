/*!
 * \file glyph_sequence.cpp
 * \brief file glyph_sequence.cpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/painter/attribute_data/glyph_sequence.hpp>
#include <vector>
#include <map>
#include <algorithm>
#include <private/util_private.hpp>
#include <private/bounding_box.hpp>
#include <private/clip.hpp>

namespace
{
  enum
    {
      SplittingSize = 300,
      MaxDepth = 10,
    };

  class ScratchSpacePrivate
  {
  public:
    std::vector<fastuidraw::vec3> m_adjusted_clip_eqs;
    fastuidraw::c_array<const fastuidraw::vec2> m_clipped_rect;

    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clip_scratch_vec2s;
  };

  class GlyphSequencePrivate;
  class GlyphAttributesIndices
  {
  public:
    GlyphAttributesIndices(void)
    {}

    GlyphAttributesIndices(const GlyphAttributesIndices &obj)
    {
      FASTUIDRAWunused(obj);
      FASTUIDRAWassert(m_attribs.empty());
      FASTUIDRAWassert(m_indices.empty());
    }

    void
    set_values(fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
               fastuidraw::c_array<const fastuidraw::vec2> positions,
               float render_format_size,
               enum fastuidraw::PainterEnums::screen_orientation orientation,
               enum fastuidraw::PainterEnums::glyph_layout_type layout);

    fastuidraw::c_array<const fastuidraw::PainterAttribute>
    attributes(void) const
    {
      return fastuidraw::make_c_array(m_attribs);
    }

    fastuidraw::c_array<const fastuidraw::PainterIndex>
    indices(void) const
    {
      return fastuidraw::make_c_array(m_indices);
    }

  private:
    std::vector<fastuidraw::PainterAttribute> m_attribs;
    std::vector<fastuidraw::PainterIndex> m_indices;
  };

  class PerAddedGlyph
  {
  public:
    fastuidraw::BoundingBox<float> m_bounding_box;
    fastuidraw::GlyphMetrics m_metrics;
    fastuidraw::vec2 m_position;
    unsigned int m_index;
  };

  class Splitter
  {
  public:
    enum place_element_t
      {
        place_in_child0 = 0,
        place_in_child1 = 1,
        place_in_parent = 2,
      };

    enum split_type_t
      {
        split_in_x_coordinate = 0,
        split_in_y_coordinate = 1,
        does_not_split = 2,
      };

    Splitter(void):
      m_splitting_coordinate(does_not_split)
    {}

    /* out_values indexed by place_element_t */
    enum split_type_t
    split(const std::vector<unsigned int> &input,
          const fastuidraw::vec2 &mid,
          const GlyphSequencePrivate *added_glyphs_src,
          fastuidraw::vecN<std::vector<unsigned int>, 3> &out_values);

    enum place_element_t
    place_element(const PerAddedGlyph &G) const;

    bool
    splits(void) const
    {
      return m_splitting_coordinate != does_not_split;
    }
  private:
    enum split_type_t m_splitting_coordinate;
    float m_splitting_value;
  };

  class GlyphSubsetPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    GlyphSubsetPrivate(GlyphSequencePrivate *p);

    ~GlyphSubsetPrivate();

    bool
    is_split(void) const
    {
      FASTUIDRAWassert((m_child[0] != nullptr) == m_splitter.splits());
      FASTUIDRAWassert((m_child[1] != nullptr) == m_splitter.splits());
      return m_splitter.splits();
    }

    const fastuidraw::BoundingBox<float>&
    bounding_box(void) const
    {
      return m_bounding_box;
    }

    void
    add_glyph(const PerAddedGlyph &G);

    unsigned int
    select(ScratchSpacePrivate &scratch,
       fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
       const fastuidraw::float3x3 &clip_matrix_local,
       fastuidraw::c_array<unsigned int> dst);

    void
    select_all(fastuidraw::c_array<unsigned int> dst,
           unsigned int &current) const;

    const GlyphAttributesIndices&
    attributes_indices(fastuidraw::GlyphRenderer R);

    fastuidraw::c_array<const unsigned int>
    glyph_elements(void) const
    {
      return fastuidraw::make_c_array(m_glyph_list);
    }

    unsigned int
    ID(void) const
    {
      return m_ID;
    }

    const fastuidraw::Path&
    path(void);

  private:
    GlyphSubsetPrivate(GlyphSubsetPrivate *parent,
                       std::vector<unsigned int> &glyph_list, //steals the data
                       const fastuidraw::BoundingBox<float> &bb);
    void
    split(void);

    void
    add_glyph_when_split(const PerAddedGlyph &G);

    void
    add_glyph_when_not_split(const PerAddedGlyph &G);

    void
    select_implement(ScratchSpacePrivate &scratch,
             fastuidraw::c_array<unsigned int> dst,
             unsigned int &current);

    GlyphSequencePrivate *m_owner;
    unsigned int m_gen, m_ID;
    std::vector<unsigned int> m_glyph_list;
    fastuidraw::BoundingBox<float> m_bounding_box;
    std::map<fastuidraw::GlyphRenderer, GlyphAttributesIndices> m_data;
    fastuidraw::Path *m_path;

    Splitter m_splitter;
    fastuidraw::vecN<GlyphSubsetPrivate*, 2> m_child;
    unsigned int m_glyph_atlas_clear_count;
  };

  class GlyphSequencePrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    GlyphSequencePrivate(float format_size,
                         enum fastuidraw::PainterEnums::screen_orientation orientation,
                         const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> &cache,
                         enum fastuidraw::PainterEnums::glyph_layout_type layout):
      m_format_size(format_size),
      m_orientation(orientation),
      m_layout(layout),
      m_cache(cache),
      m_root(nullptr)
    {
      FASTUIDRAWassert(cache);
    }

    ~GlyphSequencePrivate(void)
    {
      if (m_root)
        {
          FASTUIDRAWdelete(m_root);
        }
    }

    float
    format_size(void) const
    {
      return m_format_size;
    }

    enum fastuidraw::PainterEnums::screen_orientation
    orientation(void) const
    {
      return m_orientation;
    }

    const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache>&
    cache(void) const
    {
      return m_cache;
    }

    enum fastuidraw::PainterEnums::glyph_layout_type
    layout(void) const
    {
      return m_layout;
    }

    unsigned int
    number_added_glyphs(void) const
    {
      return m_added_glyphs.size();
    }

    const PerAddedGlyph&
    added_glyph(unsigned int I) const
    {
      FASTUIDRAWassert(I < m_added_glyphs.size());
      FASTUIDRAWassert(m_added_glyphs[I].m_index == I);
      return m_added_glyphs[I];
    }

    void
    add_glyphs(fastuidraw::c_array<const fastuidraw::GlyphSource> sources,
               fastuidraw::c_array<const fastuidraw::vec2> positions);

    unsigned int
    give_subset_ID(GlyphSubsetPrivate *p)
    {
      unsigned int return_value(m_subsets.size());
      m_subsets.push_back(p);
      return return_value;
    }

    unsigned int
    number_subsets(void)
    {
      make_subsets_ready();
      return m_subsets.size();
    }

    GlyphSubsetPrivate*
    fetch_subset(unsigned int I)
    {
      make_subsets_ready();
      FASTUIDRAWassert(I < m_subsets.size());
      FASTUIDRAWassert(m_subsets[I]->ID() == I);
      return m_subsets[I];
    }

    bool
    empty(void) const
    {
      return m_added_glyphs.empty();
    }

    GlyphSubsetPrivate*
    root(void)
    {
      make_subsets_ready();
      return m_root;
    }

  private:
    void
    make_subsets_ready(void);

    float m_format_size;
    enum fastuidraw::PainterEnums::screen_orientation m_orientation;
    enum fastuidraw::PainterEnums::glyph_layout_type m_layout;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_cache;

    std::vector<PerAddedGlyph> m_added_glyphs;
    GlyphSubsetPrivate *m_root;
    std::vector<GlyphSubsetPrivate*> m_subsets;
  };
}

/////////////////////////////////////
// GlyphAttributesIndices methods
void
GlyphAttributesIndices::
set_values(fastuidraw::c_array<const fastuidraw::Glyph> glyphs,
           fastuidraw::c_array<const fastuidraw::vec2> positions,
           float render_format_size,
           enum fastuidraw::PainterEnums::screen_orientation orientation,
           enum fastuidraw::PainterEnums::glyph_layout_type layout)
{
  using namespace fastuidraw;

  unsigned int num_attrs(0), num_indices(0);

  for (Glyph G : glyphs)
    {
      if (G.valid()
          && G.render_size().x() > 0
          && G.render_size().y() > 0)
        {
          num_attrs += 4;
          num_indices += 6;
        }
    }

  m_attribs.resize(num_attrs);
  m_indices.resize(num_indices);

  for (unsigned int g = 0, attr = 0, idx = 0; g < positions.size(); ++g)
    {
      Glyph G(glyphs[g]);

      if (G.valid()
          && G.render_size().x() > 0
          && G.render_size().y() > 0)
        {
          float scale;

          scale = render_format_size / G.metrics().units_per_EM();
          G.pack_glyph(attr, make_c_array(m_attribs),
                       idx, make_c_array(m_indices),
                       positions[g], scale,
                       orientation, layout);

          attr += 4;
          idx += 6;
        }
    }
}

//////////////////////////////////
// Splitter methods
enum Splitter::place_element_t
Splitter::
place_element(const PerAddedGlyph &G) const
{
  FASTUIDRAWassert(splits());

  bool hits_child0, hits_child1, hits_mid;
  float min_v(G.m_bounding_box.min_point()[m_splitting_coordinate]);
  float max_v(G.m_bounding_box.max_point()[m_splitting_coordinate]);

  hits_mid = (min_v < m_splitting_value) && (m_splitting_value < max_v);
  hits_child0 = (m_splitting_value > min_v) || hits_mid;
  hits_child1 = (m_splitting_value < max_v) || hits_mid;

  FASTUIDRAWassert(hits_child0 || hits_child1);
  if (hits_child0 && hits_child1)
    {
      return place_in_parent;
    }
  else if (hits_child0)
    {
      return place_in_child0;
    }
  else
    {
      return place_in_child1;
    }
}

enum Splitter::split_type_t
Splitter::
split(const std::vector<unsigned int> &input,
      const fastuidraw::vec2 &mid,
      const GlyphSequencePrivate *added_glyphs_src,
      fastuidraw::vecN<std::vector<unsigned int>, 3> &out_values)
{
  using namespace fastuidraw;

  vecN<Splitter, 2> splitV;
  vecN<unsigned int, 2> in_pre_counts(0u, 0u);
  vecN<unsigned int, 2> in_post_counts(0u, 0u);
  vecN<unsigned int, 2> in_both_counts(0u, 0u);

  FASTUIDRAWassert(!splits());
  FASTUIDRAWassert(std::is_sorted(input.begin(), input.end()));

  splitV[0].m_splitting_coordinate = split_in_x_coordinate;
  splitV[0].m_splitting_value = mid.x();

  splitV[1].m_splitting_coordinate = split_in_y_coordinate;
  splitV[1].m_splitting_value = mid.y();

  for (unsigned int I : input)
    {
      const PerAddedGlyph &G(added_glyphs_src->added_glyph(I));
      for (int c = 0; c < 2; ++c)
        {
          enum place_element_t P;
          bool in_child0, in_child1;

          P = splitV[c].place_element(G);
          in_child0 = (P == place_in_child0) || (P == place_in_parent);
          in_child1 = (P == place_in_child1) || (P == place_in_parent);

          if (in_child0)
            {
              ++in_pre_counts[c];
            }

          if (in_child1)
            {
              ++in_post_counts[c];
            }

          if (in_child0 && in_child1)
            {
              ++in_both_counts[c];
            }
        }
    }

  enum split_type_t choice;
  choice = (in_both_counts[0] < in_both_counts[1]) ?
    split_in_x_coordinate : split_in_y_coordinate;

  if (in_both_counts[choice] == input.size())
    {
      return does_not_split;
    }

  m_splitting_coordinate = choice;
  m_splitting_value = mid[choice];

  for (unsigned int I : input)
    {
      const PerAddedGlyph &G(added_glyphs_src->added_glyph(I));
      enum place_element_t P;

      P = place_element(G);
      out_values[P].push_back(I);
    }

  FASTUIDRAWassert(std::is_sorted(out_values[0].begin(), out_values[0].end()));
  FASTUIDRAWassert(std::is_sorted(out_values[1].begin(), out_values[1].end()));
  FASTUIDRAWassert(std::is_sorted(out_values[2].begin(), out_values[2].end()));
  return m_splitting_coordinate;
}

///////////////////////////////////
// GlyphSubsetPrivate methods
GlyphSubsetPrivate::
GlyphSubsetPrivate(GlyphSequencePrivate *p):
  m_owner(p),
  m_gen(0),
  m_ID(m_owner->give_subset_ID(this)),
  m_glyph_list(p->number_added_glyphs()),
  m_path(nullptr),
  m_child(nullptr, nullptr),
  m_glyph_atlas_clear_count(0)
{
  unsigned int num(m_glyph_list.size());
  for (unsigned int i = 0; i < num; ++i)
    {
      m_glyph_list[i] = i;
      m_bounding_box.union_box(p->added_glyph(i).m_bounding_box);
    }
  if (m_gen < MaxDepth && m_glyph_list.size() > SplittingSize)
    {
      split();
    }
}

GlyphSubsetPrivate::
GlyphSubsetPrivate(GlyphSubsetPrivate *parent,
                   std::vector<unsigned int> &glyph_list, //steals the data
                   const fastuidraw::BoundingBox<float> &bb):
  m_owner(parent->m_owner),
  m_gen(1 + parent->m_gen),
  m_ID(m_owner->give_subset_ID(this)),
  m_bounding_box(bb),
  m_path(nullptr),
  m_child(nullptr, nullptr),
  m_glyph_atlas_clear_count(0)
{
  std::swap(m_glyph_list, glyph_list);
  if (m_gen < MaxDepth && m_glyph_list.size() > SplittingSize)
    {
      split();
    }
}


GlyphSubsetPrivate::
~GlyphSubsetPrivate()
{
  if (is_split())
    {
      FASTUIDRAWdelete(m_child[0]);
      FASTUIDRAWdelete(m_child[1]);
    }

  if (m_path)
    {
      FASTUIDRAWdelete(m_path);
    }
}

const fastuidraw::Path&
GlyphSubsetPrivate::
path(void)
{
  if (!m_path)
    {
      m_path = FASTUIDRAWnew fastuidraw::Path();
      if (!m_bounding_box.empty())
        {
          fastuidraw::vec2 a(m_bounding_box.min_point());
          fastuidraw::vec2 b(m_bounding_box.max_point());

          *m_path << fastuidraw::vec2(a.x(), a.y())
                  << fastuidraw::vec2(a.x(), b.y())
                  << fastuidraw::vec2(b.x(), b.y())
                  << fastuidraw::vec2(b.x(), a.y())
                  << fastuidraw::Path::contour_close();
        }
    }
  return *m_path;
}

void
GlyphSubsetPrivate::
add_glyph(const PerAddedGlyph &G)
{
  if (G.m_bounding_box.empty())
    {
      return;
    }

  if (m_bounding_box.union_box(G.m_bounding_box) && !m_path)
    {
      FASTUIDRAWdelete(m_path);
    }
  if (is_split())
    {
      add_glyph_when_split(G);
    }
  else
    {
      add_glyph_when_not_split(G);
    }
}

void
GlyphSubsetPrivate::
add_glyph_when_split(const PerAddedGlyph &G)
{
  enum Splitter::place_element_t P;

  FASTUIDRAWassert(is_split());
  P = m_splitter.place_element(G);
  if (P == Splitter::place_in_parent)
    {
      m_data.clear();
      m_glyph_list.push_back(G.m_index);
    }
  else
    {
      m_child[P]->add_glyph(G);
    }
}

void
GlyphSubsetPrivate::
add_glyph_when_not_split(const PerAddedGlyph &G)
{
  FASTUIDRAWassert(!is_split());

  m_data.clear();
  m_glyph_list.push_back(G.m_index);
  if (m_gen < MaxDepth && m_glyph_list.size() > SplittingSize)
    {
      split();
    }
}

void
GlyphSubsetPrivate::
split(void)
{
  using namespace fastuidraw;

  vecN<std::vector<unsigned int>, 3> glyphs;
  vecN<BoundingBox<float>, 2> child_boxes;
  enum Splitter::split_type_t P;
  vec2 mid;

  FASTUIDRAWassert(!is_split());
  FASTUIDRAWassert(!m_bounding_box.empty());

  mid = 0.5f * (m_bounding_box.min_point() + m_bounding_box.max_point());
  P = m_splitter.split(m_glyph_list, mid, m_owner, glyphs);

  if (P == Splitter::does_not_split)
    {
      return;
    }

  if (P == Splitter::split_in_x_coordinate)
    {
      child_boxes = m_bounding_box.split_x();
    }
  else
    {
      FASTUIDRAWassert(P == Splitter::split_in_y_coordinate);
      child_boxes = m_bounding_box.split_y();
    }

  m_data.clear();
  std::swap(m_glyph_list, glyphs[Splitter::place_in_parent]);

  m_child[0] = FASTUIDRAWnew GlyphSubsetPrivate(this, glyphs[Splitter::place_in_child0], child_boxes[0]);
  m_child[1] = FASTUIDRAWnew GlyphSubsetPrivate(this, glyphs[Splitter::place_in_child1], child_boxes[1]);
}

unsigned int
GlyphSubsetPrivate::
select(ScratchSpacePrivate &scratch,
       fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
       const fastuidraw::float3x3 &clip_matrix_local,
       fastuidraw::c_array<unsigned int> dst)
{
  unsigned int return_value(0u);

  scratch.m_adjusted_clip_eqs.resize(clip_equations.size());
  for(unsigned int i = 0; i < clip_equations.size(); ++i)
    {
      /* transform clip equations from clip coordinates to
       * local coordinates.
       */
      scratch.m_adjusted_clip_eqs[i] = clip_equations[i] * clip_matrix_local;
    }

  select_implement(scratch, dst, return_value);
  return return_value;
}

void
GlyphSubsetPrivate::
select_implement(ScratchSpacePrivate &scratch,
                 fastuidraw::c_array<unsigned int> dst,
                 unsigned int &current)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  vecN<vec2, 4> bb;
  bool unclipped;

  m_bounding_box.inflated_polygon(bb, 0.0f);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, &scratch.m_clipped_rect,
                                  scratch.m_clip_scratch_vec2s);

  //completely clipped
  if (scratch.m_clipped_rect.empty())
    {
      return;
    }

  /* TODO: compute and check the bounding box of the elements
   * of m_glyph_list against the clip equations.
   */
  if (!m_glyph_list.empty())
    {
      dst[current++] = m_ID;
    }

  if (!is_split())
    {
      return;
    }

  if (unclipped)
    {
      m_child[0]->select_all(dst, current);
      m_child[1]->select_all(dst, current);
    }
  else
    {
      m_child[0]->select_implement(scratch, dst, current);
      m_child[1]->select_implement(scratch, dst, current);
    }
}

void
GlyphSubsetPrivate::
select_all(fastuidraw::c_array<unsigned int> dst, unsigned int &current) const
{
  if (!m_glyph_list.empty())
    {
      dst[current++] = m_ID;
    }

  if (is_split())
    {
      m_child[0]->select_all(dst, current);
      m_child[1]->select_all(dst, current);
    }
}

const GlyphAttributesIndices&
GlyphSubsetPrivate::
attributes_indices(fastuidraw::GlyphRenderer R)
{
  using namespace fastuidraw;

  std::map<GlyphRenderer, GlyphAttributesIndices>::const_iterator iter;
  if (!m_data.empty() && m_glyph_atlas_clear_count != m_owner->cache()->number_times_atlas_cleared())
    {
      m_glyph_atlas_clear_count = m_owner->cache()->number_times_atlas_cleared();
      m_data.clear();
    }

  iter = m_data.find(R);
  if (iter != m_data.end())
    {
      return iter->second;
    }

  GlyphAttributesIndices &dst(m_data[R]);

  std::vector<GlyphMetrics> tmp_metrics_store(m_glyph_list.size());
  c_array<GlyphMetrics> tmp_metrics(make_c_array(tmp_metrics_store));
  std::vector<vec2> tmp_positions_store(m_glyph_list.size());
  c_array<vec2> tmp_positions(make_c_array(tmp_positions_store));

  for (unsigned int i = 0, endi = m_glyph_list.size(); i < endi; ++i)
    {
      unsigned int I;

      I = m_glyph_list[i];
      tmp_metrics[i] = m_owner->added_glyph(I).m_metrics;
      tmp_positions[i] = m_owner->added_glyph(I).m_position;
    }

  std::vector<Glyph> tmp_glyphs_store(m_glyph_list.size());
  c_array<Glyph> tmp_glyphs(make_c_array(tmp_glyphs_store));
  m_owner->cache()->fetch_glyphs(R, c_array<const GlyphMetrics>(tmp_metrics), tmp_glyphs, true);

  dst.set_values(tmp_glyphs,
                 tmp_positions,
                 m_owner->format_size(),
                 m_owner->orientation(),
                 m_owner->layout());

  return dst;
}

/////////////////////////////////
// GlyphSequencePrivate methods
void
GlyphSequencePrivate::
add_glyphs(fastuidraw::c_array<const fastuidraw::GlyphSource> sources,
           fastuidraw::c_array<const fastuidraw::vec2> positions)
{
  FASTUIDRAWassert(sources.size() == positions.size());

  if (sources.empty())
    {
      return;
    }

  std::vector<fastuidraw::GlyphMetrics> tmp(sources.size());
  unsigned int old_sz;

  old_sz = m_added_glyphs.size();
  m_added_glyphs.resize(old_sz + sources.size());
  m_cache->fetch_glyph_metrics(sources, fastuidraw::make_c_array(tmp));

  for (unsigned int i = 0; i < sources.size(); ++i)
    {
      fastuidraw::GlyphMetrics M(tmp[i]);

      m_added_glyphs[i + old_sz].m_index = i + old_sz;
      m_added_glyphs[i + old_sz].m_metrics = M;
      m_added_glyphs[i + old_sz].m_position = positions[i];

      if (M.valid())
        {
          float scale;
          fastuidraw::vec2 p_bl, p_tr, lo, glyph_size;

          scale = m_format_size / M.units_per_EM();
          glyph_size = scale * M.size();
          lo = (m_layout == fastuidraw::PainterEnums::glyph_layout_horizontal) ?
            M.horizontal_layout_offset() :
            M.vertical_layout_offset();

          if (m_orientation == fastuidraw::PainterEnums::y_increases_downwards)
            {
              p_bl.x() = scale * lo.x();
              p_tr.x() = p_bl.x() + glyph_size.x();

              p_bl.y() = -scale * lo.y();
              p_tr.y() = p_bl.y() - glyph_size.y();
            }
          else
            {
              p_bl = scale * lo;
              p_tr = p_bl + glyph_size;
            }
          m_added_glyphs[i + old_sz].m_bounding_box.union_point(positions[i] + p_bl);
          m_added_glyphs[i + old_sz].m_bounding_box.union_point(positions[i] + p_tr);
          if (m_root)
            {
              m_root->add_glyph(m_added_glyphs[i + old_sz]);
            }
        }
    }
}

void
GlyphSequencePrivate::
make_subsets_ready(void)
{
  if (!m_root)
    {
      m_root = FASTUIDRAWnew GlyphSubsetPrivate(this);
    }
}

///////////////////////////////////////////////////
// fastuidraw::GlyphSequence::Subset methods
fastuidraw::GlyphSequence::Subset::
Subset(void *d):
  m_d(d)
{}

void
fastuidraw::GlyphSequence::Subset::
attributes_and_indices(GlyphRenderer render,
                       c_array<const PainterAttribute> *out_attributes,
                       c_array<const PainterIndex> *out_indices)
{
  GlyphSubsetPrivate *d;

  d = static_cast<GlyphSubsetPrivate*>(m_d);
  const GlyphAttributesIndices &values(d->attributes_indices(render));

  *out_attributes = values.attributes();
  *out_indices = values.indices();
}

fastuidraw::c_array<const unsigned int>
fastuidraw::GlyphSequence::Subset::
glyphs(void)
{
  GlyphSubsetPrivate *d;

  d = static_cast<GlyphSubsetPrivate*>(m_d);
  return d->glyph_elements();
}

bool
fastuidraw::GlyphSequence::Subset::
bounding_box(Rect *out_bb_box)
{
  GlyphSubsetPrivate *d;

  d = static_cast<GlyphSubsetPrivate*>(m_d);
  const fastuidraw::BoundingBox<float> &box(d->bounding_box());

  if (box.empty())
    {
      out_bb_box->m_min_point = fastuidraw::vec2(0.0f, 0.0f);
      out_bb_box->m_max_point = fastuidraw::vec2(0.0f, 0.0f);
      return false;
    }
  else
    {
      out_bb_box->m_min_point = box.min_point();
      out_bb_box->m_max_point = box.max_point();
      return true;
    }
}

const fastuidraw::Path&
fastuidraw::GlyphSequence::Subset::
path(void)
{
  GlyphSubsetPrivate *d;

  d = static_cast<GlyphSubsetPrivate*>(m_d);
  return d->path();
}

//////////////////////////////////////////////////
// fastuidraw::GlyphSequence::ScratchSpace methods
fastuidraw::GlyphSequence::ScratchSpace::
ScratchSpace(void)
{
  m_d = FASTUIDRAWnew ScratchSpacePrivate();
}

fastuidraw::GlyphSequence::ScratchSpace::
~ScratchSpace()
{
  ScratchSpacePrivate *d;
  d = static_cast<ScratchSpacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

////////////////////////////////////////
// fastuidraw::GlyphSequence methods
fastuidraw::GlyphSequence::
GlyphSequence(float format_size,
              enum PainterEnums::screen_orientation orientation,
              const reference_counted_ptr<GlyphCache> &cache,
              enum PainterEnums::glyph_layout_type layout)
{
  m_d = FASTUIDRAWnew GlyphSequencePrivate(format_size, orientation, cache, layout);
}

fastuidraw::GlyphSequence::
~GlyphSequence()
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

float
fastuidraw::GlyphSequence::
format_size(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->format_size();
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache>&
fastuidraw::GlyphSequence::
glyph_cache(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->cache();
}

enum fastuidraw::PainterEnums::screen_orientation
fastuidraw::GlyphSequence::
orientation(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->orientation();
}

enum fastuidraw::PainterEnums::glyph_layout_type
fastuidraw::GlyphSequence::
layout(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->layout();
}

void
fastuidraw::GlyphSequence::
add_glyphs(c_array<const GlyphSource> sources,
           c_array<const vec2> positions)
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  d->add_glyphs(sources, positions);
}

unsigned int
fastuidraw::GlyphSequence::
number_glyphs(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->number_added_glyphs();
}

void
fastuidraw::GlyphSequence::
added_glyph(unsigned int I,
            GlyphMetrics *out_glyph_metrics,
            vec2 *out_position) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);

  const PerAddedGlyph &G(d->added_glyph(I));
  *out_glyph_metrics = G.m_metrics;
  *out_position = G.m_position;
}

unsigned int
fastuidraw::GlyphSequence::
number_subsets(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->number_subsets();
}

fastuidraw::GlyphSequence::Subset
fastuidraw::GlyphSequence::
subset(unsigned int I) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return Subset(d->fetch_subset(I));
}

unsigned int
fastuidraw::GlyphSequence::
select_subsets(ScratchSpace &scratch_space,
               c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               c_array<unsigned int> dst) const
{
  GlyphSequencePrivate *d;

  d = static_cast<GlyphSequencePrivate*>(m_d);
  return (!d->empty()) ?
    d->root()->select(*static_cast<ScratchSpacePrivate*>(scratch_space.m_d),
                      clip_equations, clip_matrix_local, dst) :
    0u;
}
