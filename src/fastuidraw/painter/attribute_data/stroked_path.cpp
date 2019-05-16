/*!
 * \file stroked_path.cpp
 * \brief file stroked_path.cpp
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

#include <vector>
#include <complex>
#include <algorithm>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/partitioned_tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/attribute_data/stroked_path.hpp>
#include <fastuidraw/painter/attribute_data/stroked_caps_joins.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data_filler.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>
#include <fastuidraw/painter/shader/painter_stroke_shader.hpp>
#include <private/util_private.hpp>
#include <private/util_private_ostream.hpp>
#include <private/bounding_box.hpp>
#include <private/path_util_private.hpp>
#include <private/point_attribute_data_merger.hpp>
#include <private/clip.hpp>

namespace
{
  inline
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPoint::offset_type_t pt,
            uint32_t depth)
  {
    using namespace fastuidraw;
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);

    /* clamp depth to maximum allowed value */
    depth = t_min(depth,
                  FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(StrokedPoint::depth_num_bits));

    return pack_bits(StrokedPoint::offset_type_bit0,
                     StrokedPoint::offset_type_num_bits, pp)
      | pack_bits(StrokedPoint::boundary_bit, 1u, bb)
      | pack_bits(StrokedPoint::depth_bit0,
                  StrokedPoint::depth_num_bits, depth);
  }

  class SingleSubEdge
  {
  public:
    SingleSubEdge(const fastuidraw::PartitionedTessellatedPath::segment &seg,
                  const fastuidraw::PartitionedTessellatedPath::segment *prev);

    fastuidraw::vec2
    unit_vector_into_segment(void) const
    {
      return fastuidraw::vec2(m_begin_normal.y(), -m_begin_normal.x());
    }

    fastuidraw::vec2
    unit_vector_leaving_segment(void) const
    {
      return fastuidraw::vec2(m_end_normal.y(), -m_end_normal.x());
    }

    bool m_from_line_segment;

    fastuidraw::vec2 m_pt0, m_pt1;
    float m_distance_from_edge_start;
    float m_distance_from_contour_start;
    float m_edge_length;
    float m_contour_length;

    float m_sub_edge_length;
    fastuidraw::vec2 m_begin_normal, m_end_normal;

    bool m_has_bevel;
    float m_bevel_lambda;
    fastuidraw::vec2 m_bevel_normal;
    bool m_use_arc_for_bevel;

    //only make sense when m_from_line_segment is true
    fastuidraw::vec2 m_delta;

    //only makes sense when m_from_line_segment is false
    fastuidraw::vec2 m_center;
    fastuidraw::range_type<float> m_arc_angle;
    float m_radius;

    // only needed when arc-stroking where an edge begins or ends.
    bool m_has_start_dashed_capper;
    bool m_has_end_dashed_capper;

  private:
    static
    float
    compute_lambda(const fastuidraw::vec2 &n0, const fastuidraw::vec2 &n1)
    {
      fastuidraw::vec2 v1(n1.y(), -n1.x());
      float d;

      d = fastuidraw::dot(v1, n0);
      return (d > 0.0f) ? -1.0f : 1.0f;
    }
  };


  class ScratchSpacePrivate
  {
  public:
    fastuidraw::PartitionedTessellatedPath::ScratchSpace m_scratch;
    std::vector<unsigned int> m_partioned_subset_choices;
  };

  class SubsetPrivate:fastuidraw::noncopyable
  {
  public:
    ~SubsetPrivate();

    void
    select_subsets(unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   fastuidraw::c_array<unsigned int> dst,
                   unsigned int &current);

    void
    make_ready(void);

    const fastuidraw::Path&
    bounding_path(void) const
    {
      return m_bounding_path;
    }

    const fastuidraw::Rect&
    bounding_box(void) const
    {
      return m_subset.bounding_box();
    }

    const fastuidraw::PainterAttributeData&
    painter_data(void) const
    {
      FASTUIDRAWassert(m_painter_data != nullptr);
      return *m_painter_data;
    }

    bool
    has_children(void) const
    {
      FASTUIDRAWassert((m_children[0] == nullptr) == !m_subset.has_children());
      FASTUIDRAWassert((m_children[1] == nullptr) == !m_subset.has_children());
      return m_children[0] != nullptr;
    }

    SubsetPrivate*
    child(unsigned int I)
    {
      return m_children[I];
    }

    unsigned int
    ID(void) const
    {
      return m_subset.ID();
    }

    fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::segment_chain>
    segment_chains(void) const
    {
      return m_subset.segment_chains();
    }

    static
    SubsetPrivate*
    create_root_subset(bool has_arcs,
                       const fastuidraw::PartitionedTessellatedPath &P,
                       std::vector<SubsetPrivate*> &out_values);

  private:
    /* creation of SubsetPrivate has that it takes ownership of data
     * it might delete the object or save it for later use.
     */
    SubsetPrivate(bool has_arcs,
                  fastuidraw::PartitionedTessellatedPath::Subset src,
                  std::vector<SubsetPrivate*> &out_values);

    void
    make_ready_from_children(void);

    void
    make_ready_from_subset(void);

    void
    ready_sizes_from_children(void);

    fastuidraw::vecN<SubsetPrivate*, 2> m_children;
    fastuidraw::Path m_bounding_path;
    fastuidraw::PainterAttributeData *m_painter_data;
    unsigned int m_num_attributes, m_num_indices;
    bool m_sizes_ready, m_ready, m_has_arcs;

    fastuidraw::PartitionedTessellatedPath::Subset m_subset;
  };

  class EdgeAttributeFillerBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    EdgeAttributeFillerBase(fastuidraw::PartitionedTessellatedPath::Subset src):
      m_src(src),
      m_ready(false)
    {}

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const
    {
      ready_data();
      num_attributes = m_attributes.size();
      num_indices = m_indices.size();
      num_attribute_chunks = num_index_chunks = number_z_ranges = 1;
    }

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;
  private:
    void
    ready_data(void) const;

    void
    process_segment_chain(unsigned int &depth,
                          const fastuidraw::PartitionedTessellatedPath::segment_chain &chain) const;

    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const = 0;

    fastuidraw::PartitionedTessellatedPath::Subset m_src;

    mutable bool m_ready;
    mutable std::vector<fastuidraw::PainterAttribute> m_attributes;
    mutable std::vector<fastuidraw::PainterIndex> m_indices;
    mutable unsigned int m_vertex_count, m_index_count;
    mutable unsigned int m_depth_count;
  };

  class LineEdgeAttributeFiller:public EdgeAttributeFillerBase
  {
  public:
    explicit
    LineEdgeAttributeFiller(fastuidraw::PartitionedTessellatedPath::Subset src):
      EdgeAttributeFillerBase(src)
    {}

  private:
    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                       std::vector<fastuidraw::PainterAttribute> &attribute_data,
                       std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_bevel(bool is_inner_bevel,
                     const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;
  };

  class ArcEdgeAttributeFiller:public EdgeAttributeFillerBase
  {
  public:
    explicit
    ArcEdgeAttributeFiller(fastuidraw::PartitionedTessellatedPath::Subset src):
      EdgeAttributeFillerBase(src)
    {}

  private:
    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                       std::vector<fastuidraw::PainterAttribute> &attribute_data,
                       std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_arc_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                      std::vector<fastuidraw::PainterAttribute> &attribute_data,
                      std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_bevel(bool is_inner_bevel,
                     const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_arc_bevel(bool is_inner_bevel,
                    const SingleSubEdge &sub_edge, unsigned int &depth,
                    std::vector<fastuidraw::PainterAttribute> &attribute_data,
                    std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_dashed_capper(bool for_start_dashed_capper,
                        const SingleSubEdge &sub_edge, unsigned int &depth,
                        std::vector<fastuidraw::PainterAttribute> &attribute_data,
                        std::vector<fastuidraw::PainterIndex> &index_data) const;
  };

  class StrokedPathPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    StrokedPathPrivate(const fastuidraw::TessellatedPath &P);
    ~StrokedPathPrivate();

    bool m_has_arcs;
    fastuidraw::reference_counted_ptr<const fastuidraw::PartitionedTessellatedPath> m_path_partioned;
    fastuidraw::StrokedCapsJoins m_caps_joins;
    SubsetPrivate* m_root;
    std::vector<SubsetPrivate*> m_subsets;
  };

}

////////////////////////////////////////
// SingleSubEdge methods
SingleSubEdge::
SingleSubEdge(const fastuidraw::PartitionedTessellatedPath::segment &seg,
              const fastuidraw::PartitionedTessellatedPath::segment *prev):
  m_from_line_segment(seg.m_type == fastuidraw::TessellatedPath::line_segment),
  m_pt0(seg.m_start_pt),
  m_pt1(seg.m_end_pt),
  m_distance_from_edge_start(seg.m_distance_from_edge_start),
  m_distance_from_contour_start(seg.m_distance_from_contour_start),
  m_edge_length(seg.m_edge_length),
  m_contour_length(seg.m_contour_length),
  m_sub_edge_length(seg.m_length),
  m_begin_normal(-seg.m_enter_segment_unit_vector.y(),
                 seg.m_enter_segment_unit_vector.x()),
  m_end_normal(-seg.m_leaving_segment_unit_vector.y(),
               seg.m_leaving_segment_unit_vector.x()),
  m_delta(m_pt1 - m_pt0),
  m_center(seg.m_center),
  m_arc_angle(seg.m_arc_angle),
  m_radius(seg.m_radius),
  m_has_start_dashed_capper(!m_from_line_segment && seg.m_first_segment_of_edge),
  m_has_end_dashed_capper(!m_from_line_segment && seg.m_last_segment_of_edge)
{
  /* If the dot product between the normal is negative, then
   * chances are there is a cusp; we do NOT want a bevel (or
   * arc) join then.
   */
  if (seg.m_continuation_with_predecessor
      || !prev
      || fastuidraw::dot(prev->m_leaving_segment_unit_vector,
                         seg.m_enter_segment_unit_vector) < 0.0f)
    {
      m_bevel_lambda = 0.0f;
      m_has_bevel = false;
      m_use_arc_for_bevel = false;
      m_bevel_normal = fastuidraw::vec2(0.0f, 0.0f);
    }
  else
    {
      m_bevel_normal = fastuidraw::vec2(-prev->m_leaving_segment_unit_vector.y(),
                                        +prev->m_leaving_segment_unit_vector.x());
      m_bevel_lambda = compute_lambda(m_bevel_normal, m_begin_normal);
      m_has_bevel = true;

      /* make them meet at an arc-join if one of them is as arc-segment */
      m_use_arc_for_bevel = prev->m_type == fastuidraw::TessellatedPath::arc_segment
        || seg.m_type == fastuidraw::TessellatedPath::arc_segment;
    }
}

///////////////////////////////////////////////
// SubsetPrivate methods
SubsetPrivate*
SubsetPrivate::
create_root_subset(bool has_arcs,
                   const fastuidraw::PartitionedTessellatedPath &P,
                   std::vector<SubsetPrivate*> &out_values)
{
  SubsetPrivate *return_value;

  return_value = FASTUIDRAWnew SubsetPrivate(has_arcs, P.root_subset(), out_values);
  return return_value;
}

SubsetPrivate::
SubsetPrivate(bool has_arcs,
              fastuidraw::PartitionedTessellatedPath::Subset src,
              std::vector<SubsetPrivate*> &out_values):
  m_children(nullptr, nullptr),
  m_painter_data(nullptr),
  m_num_attributes(0),
  m_num_indices(0),
  m_sizes_ready(false),
  m_ready(false),
  m_has_arcs(has_arcs),
  m_subset(src)
{
  using namespace fastuidraw;

  out_values[ID()] = this;
  if (m_subset.has_children())
    {
      vecN<fastuidraw::PartitionedTessellatedPath::Subset, 2> children;
      children = m_subset.children();
      m_children[0] = FASTUIDRAWnew SubsetPrivate(m_has_arcs, children[0], out_values);
      m_children[1] = FASTUIDRAWnew SubsetPrivate(m_has_arcs, children[1], out_values);
    }

  const fastuidraw::vec2 &m(bounding_box().m_min_point);
  const fastuidraw::vec2 &M(bounding_box().m_max_point);
  m_bounding_path << vec2(m.x(), m.y())
                  << vec2(m.x(), M.y())
                  << vec2(M.x(), M.y())
                  << vec2(M.x(), m.y())
                  << Path::contour_close();
}

SubsetPrivate::
~SubsetPrivate()
{
  if (m_painter_data)
    {
      FASTUIDRAWdelete(m_painter_data);
    }

  if (m_children[0])
    {
      FASTUIDRAWassert(m_children[1]);

      FASTUIDRAWdelete(m_children[0]);
      FASTUIDRAWdelete(m_children[1]);
    }
}

void
SubsetPrivate::
make_ready(void)
{
  if (!m_ready)
    {
      if (has_children())
        {
          make_ready_from_children();
        }
      else
        {
          make_ready_from_subset();
          FASTUIDRAWassert(m_painter_data != nullptr);
        }
    }
  FASTUIDRAWassert(m_ready);
}

void
SubsetPrivate::
make_ready_from_children(void)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  FASTUIDRAWassert(m_subset.has_children());
  FASTUIDRAWassert(m_painter_data == nullptr);

  m_children[0]->make_ready();
  m_children[1]->make_ready();
  m_ready = true;

  if (m_children[0]->m_painter_data == nullptr
      || m_children[1]->m_painter_data == nullptr)
    {
      return;
    }

  /* Do NOT allow merging if the depth requirements
   * summed between the two children is too great.
   */
  const unsigned int arc_max_depth(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(ArcStrokedPoint::depth_num_bits));
  const unsigned int linear_max_depth(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(StrokedPoint::depth_num_bits));
  unsigned int needed_zrange;
  bool mergable;

  needed_zrange = m_children[0]->painter_data().z_range(0).difference()
    + m_children[1]->painter_data().z_range(0).difference();

  mergable = (m_has_arcs && needed_zrange < arc_max_depth)
    || (!m_has_arcs && needed_zrange < linear_max_depth);

  if (!mergable)
    {
      return;
    }

  m_painter_data = FASTUIDRAWnew PainterAttributeData();
  if (m_has_arcs)
    {
      detail::PointAttributeDataMerger<ArcStrokedPoint> merger(m_children[0]->painter_data(),
                                                               m_children[1]->painter_data());
      m_painter_data->set_data(merger);
    }
  else
    {
      detail::PointAttributeDataMerger<StrokedPoint> merger(m_children[0]->painter_data(),
                                                            m_children[1]->painter_data());
      m_painter_data->set_data(merger);
    }

  if (!m_sizes_ready)
    {
      ready_sizes_from_children();
    }
  FASTUIDRAWassert(m_num_attributes == m_painter_data->attribute_data_chunk(0).size());
  FASTUIDRAWassert(m_num_indices == m_painter_data->index_data_chunk(0).size());
}

void
SubsetPrivate::
make_ready_from_subset(void)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(m_children[0] == nullptr);
  FASTUIDRAWassert(m_children[1] == nullptr);
  FASTUIDRAWassert(m_painter_data == nullptr);
  FASTUIDRAWassert(!m_subset.has_children());

  m_ready = true;
  m_painter_data = FASTUIDRAWnew PainterAttributeData();
  if (m_has_arcs)
    {
      m_painter_data->set_data(ArcEdgeAttributeFiller(m_subset));
    }
  else
    {
      m_painter_data->set_data(LineEdgeAttributeFiller(m_subset));
    }

  m_num_attributes = m_painter_data->attribute_data_chunk(0).size();
  m_num_indices = m_painter_data->index_data_chunk(0).size();
  m_sizes_ready = true;
}

void
SubsetPrivate::
ready_sizes_from_children(void)
{
  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  FASTUIDRAWassert(!m_sizes_ready);

  m_sizes_ready = true;
  FASTUIDRAWassert(m_children[0]->m_sizes_ready);
  FASTUIDRAWassert(m_children[1]->m_sizes_ready);
  m_num_attributes = m_children[0]->m_num_attributes + m_children[1]->m_num_attributes;
  m_num_indices = m_children[0]->m_num_indices + m_children[1]->m_num_indices;
}

void
SubsetPrivate::
select_subsets(unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               fastuidraw::c_array<unsigned int> dst,
               unsigned int &current)
{

  if (!m_sizes_ready && !has_children())
    {
      /* We need to make this one ready because it will be selected. */
      make_ready_from_subset();
      FASTUIDRAWassert(m_painter_data != nullptr);
      FASTUIDRAWassert(m_sizes_ready);
    }

  if (m_sizes_ready
      && m_num_attributes <= max_attribute_cnt
      && m_num_indices <= max_index_cnt)
    {
      make_ready();

      /* if this Subset has two children but is unable
       * to merge them, then m_painter_data will still
       * be nullptr after make_ready().
       */
      if (m_painter_data)
        {
          dst[current] = ID();
          ++current;
          return;
        }
    }

  if (has_children())
    {
      m_children[0]->select_subsets(max_attribute_cnt, max_index_cnt, dst, current);
      m_children[1]->select_subsets(max_attribute_cnt, max_index_cnt, dst, current);
      if (!m_sizes_ready)
        {
          ready_sizes_from_children();
        }
    }
  else
    {
      FASTUIDRAWassert(m_sizes_ready);
      FASTUIDRAWassert(!"Childless StrokedPath::Subset has too many attributes or indices");
    }
}

////////////////////////////////////////
// EdgeAttributeFillerBase methods
void
EdgeAttributeFillerBase::
process_segment_chain(unsigned int &d,
                      const fastuidraw::PartitionedTessellatedPath::segment_chain &chain) const
{
  const fastuidraw::PartitionedTessellatedPath::segment *prev(chain.m_prev_to_start);
  for (const fastuidraw::PartitionedTessellatedPath::segment &S : chain.m_segments)
    {
      SingleSubEdge sub_edge(S, prev);
      process_sub_edge(sub_edge, d, m_attributes, m_indices);
      prev = &S;
    }
}

void
EdgeAttributeFillerBase::
ready_data(void) const
{
  if (m_ready)
    {
      return;
    }

  m_ready = true;

  /* To get it so that the depth values are in non-increasing order
   * with respect to the triangles, we will reverse the triangle
   * other AFTER filling the data.
   */
  unsigned int d(0);
  for (fastuidraw::PartitionedTessellatedPath::segment_chain chain : m_src.segment_chains())
    {
      process_segment_chain(d, chain);
    }
  m_vertex_count = m_attributes.size();
  m_index_count = m_indices.size();
  m_depth_count = d;

  /* now reverse the triangles in m_indices. */
  std::reverse(m_indices.begin(), m_indices.end());
}

void
EdgeAttributeFillerBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  using namespace fastuidraw;

  index_adjusts[0] = 0;
  zranges[0].m_begin = 0;
  zranges[0].m_end = m_depth_count;
  attribute_chunks[0] = attribute_data;
  index_chunks[0] = index_data;

  std::copy(m_attributes.begin(), m_attributes.end(), attribute_data.begin());
  std::copy(m_indices.begin(), m_indices.end(), index_data.begin());
}

///////////////////////////////////////////////
// LineEdgeAttributeFiller methods
void
LineEdgeAttributeFiller::
build_line_bevel(bool is_inner_bevel,
                 const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  vecN<StrokedPoint, 3> pts;
  float sgn;
  unsigned int vert_offset(attribute_data.size());

  if (is_inner_bevel)
    {
      sgn = -sub_edge.m_bevel_lambda;
    }
  else
    {
      sgn = sub_edge.m_bevel_lambda;
    }

  indices.push_back(vert_offset + 0);
  indices.push_back(vert_offset + 1);
  indices.push_back(vert_offset + 2);

  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_contour_length = sub_edge.m_contour_length;
      pts[k].m_auxiliary_offset = vec2(0.0f, 0.0f);
    }

  pts[0].m_pre_offset = vec2(0.0f, 0.0f);
  pts[0].m_packed_data = pack_data(0, StrokedPoint::offset_sub_edge, depth)
    | StrokedPoint::bevel_edge_mask;

  pts[1].m_pre_offset = sgn * sub_edge.m_bevel_normal;
  pts[1].m_packed_data = pack_data(1, StrokedPoint::offset_sub_edge, depth)
    | StrokedPoint::bevel_edge_mask;

  pts[2].m_pre_offset = sgn * sub_edge.m_begin_normal;
  pts[2].m_packed_data = pack_data(1, StrokedPoint::offset_sub_edge, depth)
    | StrokedPoint::bevel_edge_mask;

  for(unsigned int i = 0; i < 3; ++i)
    {
      PainterAttribute A;

      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  if (is_inner_bevel)
    {
      ++depth;
    }
}

void
LineEdgeAttributeFiller::
build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                   std::vector<fastuidraw::PainterAttribute> &attribute_data,
                   std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  vecN<StrokedPoint, 6> pts;

  /* The quad is:
   * (p, n, delta,  1),
   * (p,-n, delta,  1),
   * (p, 0,     0,  0),
   * (p_next,  n, -delta, 1),
   * (p_next, -n, -delta, 1),
   * (p_next,  0, 0)
   *
   * Notice that we are encoding if it is
   * start or end of edge from the sign of
   * m_on_boundary.
   */
  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_contour_length = sub_edge.m_contour_length;
      pts[k].m_pre_offset = normal_sign[k] * sub_edge.m_begin_normal;
      pts[k].m_auxiliary_offset = sub_edge.m_delta;
      pts[k].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, depth);

      pts[k + 3].m_position = sub_edge.m_pt1;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_edge_length = sub_edge.m_edge_length;
      pts[k + 3].m_contour_length = sub_edge.m_contour_length;
      pts[k + 3].m_pre_offset = normal_sign[k] * sub_edge.m_end_normal;
      pts[k + 3].m_auxiliary_offset = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, depth)
        | StrokedPoint::end_sub_edge_mask;
    }

  unsigned int vert_offset(attribute_data.size());
  const unsigned int tris[12] =
    {
      0, 2, 5,
      0, 5, 3,
      2, 1, 4,
      2, 4, 5
    };

  for(unsigned int i = 0; i < 12; ++i)
    {
      indices.push_back(vert_offset + tris[i]);
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      PainterAttribute A;

      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  ++depth;
}

void
LineEdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  FASTUIDRAWassert(sub_edge.m_from_line_segment);
  FASTUIDRAWassert(!sub_edge.m_has_start_dashed_capper);
  FASTUIDRAWassert(!sub_edge.m_has_end_dashed_capper);

  if (sub_edge.m_has_bevel)
    {
      build_line_bevel(false,
                       sub_edge, depth,
                       attribute_data, indices);
    }
  build_line_segment(sub_edge, depth, attribute_data, indices);

  if (sub_edge.m_has_bevel)
    {
      build_line_bevel(true,
                       sub_edge, depth, attribute_data, indices);
    }
}

////////////////////////////////////
// ArcEdgeAttributeFiller methods
void
ArcEdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  if (sub_edge.m_has_bevel)
    {
      if (sub_edge.m_use_arc_for_bevel)
        {
          build_arc_bevel(false, sub_edge, depth, attribute_data, indices);
        }
      else
        {
          build_line_bevel(false, sub_edge, depth, attribute_data, indices);
        }
    }

  if (sub_edge.m_has_start_dashed_capper)
    {
      build_dashed_capper(true, sub_edge, depth, attribute_data, indices);
    }

  if (sub_edge.m_from_line_segment)
    {
      build_line_segment(sub_edge, depth, attribute_data, indices);
    }
  else
    {
      build_arc_segment(sub_edge, depth, attribute_data, indices);
    }

  if (sub_edge.m_has_bevel)
    {
      if (sub_edge.m_use_arc_for_bevel)
        {
          build_arc_bevel(true, sub_edge, depth, attribute_data, indices);
        }
      else
        {
          build_line_bevel(true, sub_edge, depth, attribute_data, indices);
        }
    }

  if (sub_edge.m_has_end_dashed_capper)
    {
      build_dashed_capper(false, sub_edge, depth, attribute_data, indices);
    }
}

void
ArcEdgeAttributeFiller::
build_dashed_capper(bool for_start_dashed_capper,
                    const SingleSubEdge &sub_edge, unsigned int &depth,
                    std::vector<fastuidraw::PainterAttribute> &attribute_data,
                    std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  unsigned int vert_offset(attribute_data.size());
  const PainterIndex tris[12] =
    {
      0, 3, 4,
      0, 4, 1,
      1, 4, 5,
      1, 5, 2,
    };

  for (unsigned int i = 0; i < 12; ++i)
    {
      indices.push_back(vert_offset + tris[i]);
    }

  vec2 tangent, normal, pos;
  uint32_t packed_data, packed_data_mid;
  float d;

  packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point_dashed_capper, depth);
  packed_data_mid = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point_dashed_capper, depth);
  if (for_start_dashed_capper)
    {
      normal = sub_edge.m_begin_normal;
      tangent = -sub_edge.unit_vector_into_segment();
      pos = sub_edge.m_pt0;
      d = 0.0f;
    }
  else
    {
      normal = sub_edge.m_end_normal;
      tangent = sub_edge.unit_vector_leaving_segment();
      pos = sub_edge.m_pt1;
      packed_data |= ArcStrokedPoint::end_segment_mask;
      packed_data_mid |= ArcStrokedPoint::end_segment_mask;
      d = sub_edge.m_sub_edge_length;
    }

  ArcStrokedPoint pt;
  PainterAttribute A;

  pt.m_position = pos;
  pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + d;
  pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + d;
  pt.m_edge_length = sub_edge.m_edge_length;
  pt.m_contour_length = sub_edge.m_contour_length;
  pt.m_data = tangent;

  pt.m_packed_data = packed_data;
  pt.m_offset_direction = normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data_mid;
  pt.m_offset_direction = vec2(0.0f, 0.0f);
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data;
  pt.m_offset_direction = -normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data_mid | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = vec2(0.0f, 0.0f);
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = -normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  ++depth;
}

void
ArcEdgeAttributeFiller::
build_arc_bevel(bool is_inner_bevel,
                const SingleSubEdge &sub_edge, unsigned int &depth,
                std::vector<fastuidraw::PainterAttribute> &attribute_data,
                std::vector<fastuidraw::PainterIndex> &indices) const
{
  fastuidraw::vec2 n_start, n_end;
  fastuidraw::ArcStrokedPoint pt;

  if (is_inner_bevel)
    {
      n_start = -sub_edge.m_bevel_lambda * sub_edge.m_begin_normal;
      n_end = -sub_edge.m_bevel_lambda * sub_edge.m_bevel_normal;
    }
  else
    {
      n_start = sub_edge.m_bevel_lambda * sub_edge.m_bevel_normal;
      n_end = sub_edge.m_bevel_lambda * sub_edge.m_begin_normal;
    }

  pt.m_position = sub_edge.m_pt0;
  pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
  pt.m_edge_length = sub_edge.m_edge_length;
  pt.m_contour_length = sub_edge.m_contour_length;

  fastuidraw::detail::pack_arc_join(pt, 1, n_start, n_end, depth,
                                    attribute_data, indices, false);
  ++depth;
}

void
ArcEdgeAttributeFiller::
build_line_bevel(bool is_inner_bevel,
                 const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  vecN<ArcStrokedPoint, 3> pts;
  float sgn;

  if (is_inner_bevel)
    {
      sgn = -sub_edge.m_bevel_lambda;
    }
  else
    {
      sgn = sub_edge.m_bevel_lambda;
    }

  unsigned int vert_offset(attribute_data.size());
  for (unsigned int i = 0; i < 3; ++i)
    {
      indices.push_back(vert_offset + i);
    }

  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_contour_length = sub_edge.m_contour_length;
      pts[k].m_data = vec2(0.0f, 0.0f);
    }

  pts[0].m_offset_direction = vec2(0.0f, 0.0f);
  pts[0].m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
    | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_line_segment, depth);

  pts[1].m_offset_direction = sgn * sub_edge.m_bevel_normal;
  pts[1].m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_line_segment, depth);

  pts[2].m_offset_direction = sgn * sub_edge.m_begin_normal;
  pts[2].m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_line_segment, depth);

  for(unsigned int i = 0; i < 3; ++i)
    {
      PainterAttribute A;
      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  if (is_inner_bevel)
    {
      ++depth;
    }
}

void
ArcEdgeAttributeFiller::
build_arc_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                  std::vector<fastuidraw::PainterAttribute> &attribute_data,
                  std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  ArcStrokedPoint begin_pt, end_pt;
  vec2 begin_radial, end_radial;
  PainterAttribute A;
  unsigned int vert_offset(attribute_data.size());

  const unsigned int tris[24] =
    {
      4, 6, 7,
      4, 7, 5,
      2, 4, 5,
      2, 5, 3,
      8, 0, 1,
      9, 2, 3,

      0,  1, 10,
      1, 10, 11
    };

  for (unsigned int i = 0; i < 24; ++i)
    {
      indices.push_back(tris[i] + vert_offset);
    }

  begin_pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
  begin_pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
  begin_pt.m_edge_length = sub_edge.m_edge_length;
  begin_pt.m_contour_length = sub_edge.m_contour_length;
  begin_pt.radius() = sub_edge.m_radius;
  begin_pt.arc_angle() = sub_edge.m_arc_angle.m_end - sub_edge.m_arc_angle.m_begin;

  end_pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
  end_pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
  end_pt.m_edge_length = sub_edge.m_edge_length;
  end_pt.m_contour_length = sub_edge.m_contour_length;
  end_pt.radius() = sub_edge.m_radius;
  end_pt.arc_angle() = sub_edge.m_arc_angle.m_end - sub_edge.m_arc_angle.m_begin;

  begin_radial = vec2(t_cos(sub_edge.m_arc_angle.m_begin), t_sin(sub_edge.m_arc_angle.m_begin));
  end_radial = vec2(t_cos(sub_edge.m_arc_angle.m_end), t_sin(sub_edge.m_arc_angle.m_end));

  /* inner stroking boundary points (points 0, 1) */
  begin_pt.m_packed_data = ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* the points that are on the arc (points 2, 3) */
  begin_pt.m_packed_data = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* outer stroking boundary points (points 4, 5) */
  begin_pt.m_packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* beyond outer stroking boundary points (points 6, 7) */
  begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* points that move to origin when stroking radius is larger than arc radius (points 8, 9) */
  begin_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
    | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
    | ArcStrokedPoint::end_segment_mask
    | ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* beyond inner stroking boundary (points 10, 11) */
  begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
    | ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  ++depth;
}

void
ArcEdgeAttributeFiller::
build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                   std::vector<fastuidraw::PainterAttribute> &attribute_data,
                   std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  vecN<ArcStrokedPoint, 6> pts;

  /* The quad is:
   * (p, n, delta,  1),
   * (p,-n, delta,  1),
   * (p, 0,     0,  0),
   * (p_next,  n, -delta, 1),
   * (p_next, -n, -delta, 1),
   * (p_next,  0, 0)
   *
   * Notice that we are encoding if it is
   * start or end of edge from the sign of
   * m_on_boundary.
   */
  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_contour_length = sub_edge.m_contour_length;
      pts[k].m_offset_direction = normal_sign[k] * sub_edge.m_begin_normal;
      pts[k].m_data = sub_edge.m_delta;
      pts[k].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                         ArcStrokedPoint::offset_line_segment,
                                                         depth);

      pts[k + 3].m_position = sub_edge.m_pt1;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_edge_length = sub_edge.m_edge_length;
      pts[k + 3].m_contour_length = sub_edge.m_contour_length;
      pts[k + 3].m_offset_direction = normal_sign[k] * sub_edge.m_end_normal;
      pts[k + 3].m_data = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                             ArcStrokedPoint::offset_line_segment,
                                                             depth)
        | ArcStrokedPoint::end_segment_mask;
    }

  const unsigned int tris[12] =
    {
      0, 2, 5,
      0, 5, 3,
      2, 1, 4,
      2, 4, 5
    };

  unsigned int vert_offset(attribute_data.size());
  for(unsigned int i = 0; i < 12; ++i)
    {
      indices.push_back(vert_offset + tris[i]);
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      PainterAttribute A;

      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  ++depth;
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P):
  m_has_arcs(P.has_arcs()),
  m_caps_joins(P.partitioned()),
  m_root(nullptr)
{
  if (!P.segment_data().empty())
    {
      m_path_partioned = &P.partitioned();
      m_subsets.resize(m_path_partioned->number_subsets());
      m_root = SubsetPrivate::create_root_subset(m_has_arcs, *m_path_partioned, m_subsets);
    }
}

StrokedPathPrivate::
~StrokedPathPrivate()
{
  if (m_root)
    {
      FASTUIDRAWdelete(m_root);
    }
}

//////////////////////////////////////////////
// fastuidraw::StrokedPath::ScratchSpace methods
fastuidraw::StrokedPath::ScratchSpace::
ScratchSpace(void)
{
  m_d = FASTUIDRAWnew ScratchSpacePrivate();
}

fastuidraw::StrokedPath::ScratchSpace::
~ScratchSpace(void)
{
  ScratchSpacePrivate *d;
  d = static_cast<ScratchSpacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

/////////////////////////////////
// fastuidraw::StrokedPath::Subset methods
fastuidraw::StrokedPath::Subset::
Subset(void *d):
  m_d(d)
{
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::Subset::
painter_data(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->painter_data();
}

const fastuidraw::Path&
fastuidraw::StrokedPath::Subset::
bounding_path(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->bounding_path();
}

const fastuidraw::Rect&
fastuidraw::StrokedPath::Subset::
bounding_box(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->bounding_box();
}

fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::segment_chain>
fastuidraw::StrokedPath::Subset::
segment_chains(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->segment_chains();
}

unsigned int
fastuidraw::StrokedPath::Subset::
ID(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->ID();
}

bool
fastuidraw::StrokedPath::Subset::
has_children(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->has_children();
}

fastuidraw::vecN<fastuidraw::StrokedPath::Subset, 2>
fastuidraw::StrokedPath::Subset::
children(void) const
{
  SubsetPrivate *d, *p0(nullptr), *p1(nullptr);

  d = static_cast<SubsetPrivate*>(m_d);
  if (d && d->has_children())
    {
      p0 = d->child(0);
      p1 = d->child(1);
    }

  Subset s0(p0), s1(p1);
  return vecN<Subset, 2>(s0, s1);
}

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  m_d = FASTUIDRAWnew StrokedPathPrivate(P);
}

fastuidraw::StrokedPath::
~StrokedPath()
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

bool
fastuidraw::StrokedPath::
has_arcs(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_has_arcs;
}

unsigned int
fastuidraw::StrokedPath::
number_subsets(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_subsets.size();
}

fastuidraw::StrokedPath::Subset
fastuidraw::StrokedPath::
subset(unsigned int I) const
{
  StrokedPathPrivate *d;
  SubsetPrivate *p;

  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWassert(I < d->m_subsets.size());
  p = d->m_subsets[I];
  p->make_ready();

  return Subset(p);
}

unsigned int
fastuidraw::StrokedPath::
select_subsets(ScratchSpace &scratch_space,
               c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               const vec2 &one_pixel_width,
               c_array<const float> geometry_inflation,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               c_array<unsigned int> dst) const
{
  StrokedPathPrivate *d;
  ScratchSpacePrivate *scratch;
  unsigned int return_value(0);

  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWassert(dst.size() >= d->m_subsets.size());

  if (d->m_root)
    {
      unsigned int N;
      c_array<unsigned int> partitioned_subsets;

      scratch = static_cast<ScratchSpacePrivate*>(scratch_space.m_d);
      scratch->m_partioned_subset_choices.resize(d->m_path_partioned->number_subsets());
      partitioned_subsets = make_c_array(scratch->m_partioned_subset_choices);

      N = d->m_path_partioned->select_subsets(scratch->m_scratch,
                                              clip_equations,
                                              clip_matrix_local,
                                              one_pixel_width,
                                              geometry_inflation,
                                              partitioned_subsets);

      partitioned_subsets = partitioned_subsets.sub_array(0, N);
      for (unsigned int S : partitioned_subsets)
        {
          d->m_subsets[S]->select_subsets(max_attribute_cnt,
                                          max_index_cnt,
                                          dst, return_value);
        }
    }

  return return_value;
}

unsigned int
fastuidraw::StrokedPath::
select_subsets_no_culling(unsigned int max_attribute_cnt,
                          unsigned int max_index_cnt,
                          c_array<unsigned int> dst) const
{
  StrokedPathPrivate *d;
  unsigned int return_value(0);

  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWassert(dst.size() >= d->m_subsets.size());

  if (d->m_root)
    {
      d->m_root->select_subsets(max_attribute_cnt,
                                max_index_cnt,
                                dst, return_value);
    }
  else
    {
      return_value = 0;
    }

  return return_value;
}

fastuidraw::StrokedPath::Subset
fastuidraw::StrokedPath::
root_subset(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return Subset(d->m_root);
}

const fastuidraw::StrokedCapsJoins&
fastuidraw::StrokedPath::
caps_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_caps_joins;
}

const fastuidraw::PartitionedTessellatedPath&
fastuidraw::StrokedPath::
partitioned_path(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return *d->m_path_partioned;
}
