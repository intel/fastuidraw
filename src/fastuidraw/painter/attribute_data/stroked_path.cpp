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
  template<typename T>
  class OrderingEntry:public T
  {
  public:
    OrderingEntry(const T &src, unsigned int chunk):
      T(src),
      m_chunk(chunk),
      m_depth(0)
    {}

    unsigned int m_chunk;
    unsigned int m_depth;
  };

  template<typename T>
  class GenericOrdering
  {
  public:
    fastuidraw::c_array<const OrderingEntry<T> >
    elements(void) const
    {
      return fastuidraw::make_c_array(m_elements);
    }

    unsigned int
    size(void) const
    {
      return m_elements.size();
    }

    const OrderingEntry<T>&
    operator[](unsigned int N) const
    {
      FASTUIDRAWassert(N < m_elements.size());
      return m_elements[N];
    }

    void
    add_element(const T &src, unsigned int &chunk)
    {
      OrderingEntry<T> C(src, chunk);
      m_elements.push_back(C);
      ++chunk;
    }

    void
    post_process(fastuidraw::range_type<unsigned int> range,
                 unsigned int &depth);

  private:
    std::vector<OrderingEntry<T> > m_elements;
  };

  typedef fastuidraw::PartitionedTessellatedPath::cap PerCapData;
  typedef fastuidraw::PartitionedTessellatedPath::join PerJoinData;

  typedef GenericOrdering<PerCapData> CapOrdering;
  typedef GenericOrdering<PerJoinData> JoinOrdering;

  class CapJoinChunkDepthTracking
  {
  public:
    CapJoinChunkDepthTracking(void):
      m_join_chunk_cnt(0),
      m_cap_chunk_cnt(0)
    {}

    unsigned int m_join_chunk_cnt;
    JoinOrdering m_join_ordering;
    unsigned int m_cap_chunk_cnt;
    CapOrdering m_cap_ordering;
  };

  class RangeAndChunk
  {
  public:
    /* what range of Joins/Caps hit by the chunk. */
    fastuidraw::range_type<unsigned int> m_elements;

    /* depth range of Joins/Caps hit */
    fastuidraw::range_type<unsigned int> m_depth_range;

    /* what the chunk is */
    int m_chunk;

    bool
    non_empty(void) const
    {
      return m_depth_range.m_end > m_depth_range.m_begin;
    }
  };

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

  class SubsetSelectionPrivate
  {
  public:
    fastuidraw::PartitionedTessellatedPath::SubsetSelection m_selection;
    std::vector<unsigned int> m_subset_ids;
    fastuidraw::c_array<const unsigned int> m_join_subset_ids;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> m_source;
  };

  class SubsetPrivate:fastuidraw::noncopyable
  {
  public:
    ~SubsetPrivate();

    void
    select_subsets(unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   std::vector<unsigned int> &dst);

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

    const SubsetPrivate*
    child(unsigned int I) const
    {
      return m_children[I];
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

    fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::join>
    join_values(void) const
    {
      return m_subset.joins();
    }

    fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::cap>
    cap_values(void) const
    {
      return m_subset.caps();
    }

    const RangeAndChunk&
    joins(void) const
    {
      return m_joins;
    }

    const RangeAndChunk&
    caps(void) const
    {
      return m_caps;
    }

    int
    join_chunk(void) const
    {
      return (m_joins.non_empty()) ?
        m_joins.m_chunk :
        -1;
    }

    int
    cap_chunk(void) const
    {
      return (m_caps.non_empty()) ?
        m_caps.m_chunk :
        -1;
    }

    static
    SubsetPrivate*
    create_root_subset(bool has_arcs, CapJoinChunkDepthTracking &tracking,
                       const fastuidraw::PartitionedTessellatedPath &P,
                       std::vector<SubsetPrivate*> &out_values);

  private:
    class PostProcessVariables
    {
    public:
      PostProcessVariables(void):
        m_join_depth(0),
        m_cap_depth(0)
      {}

      unsigned int m_join_depth;
      unsigned int m_cap_depth;
    };

    /* creation of SubsetPrivate has that it takes ownership of data
     * it might delete the object or save it for later use.
     */
    SubsetPrivate(bool has_arcs, CapJoinChunkDepthTracking &tracking,
                  fastuidraw::PartitionedTessellatedPath::Subset src,
                  std::vector<SubsetPrivate*> &out_values);
    void
    post_process(PostProcessVariables &variables,
                 CapJoinChunkDepthTracking &tracking);

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

    RangeAndChunk m_joins, m_caps;

    fastuidraw::PartitionedTessellatedPath::Subset m_subset;
  };

  class JoinCreatorBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    JoinCreatorBase(const CapJoinChunkDepthTracking &P, const SubsetPrivate *st);

    virtual
    ~JoinCreatorBase() {}

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const override;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const override;

  protected:
    void
    post_ctor_initalize(void);

  private:
    void
    fill_join(unsigned int join_id, const PerJoinData &join_data,
              unsigned int chunk, unsigned int depth,
              fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &vertex_offset, unsigned int &index_offset,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts,
              fastuidraw::c_array<fastuidraw::range_type<int> > join_vertex_ranges,
              fastuidraw::c_array<fastuidraw::range_type<int> > join_index_ranges) const;

    static
    void
    set_chunks(const SubsetPrivate *st,
               fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
               fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
               fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
               fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
               fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
               fastuidraw::c_array<int> index_adjusts);

    static
    void
    process_chunk(const RangeAndChunk &ch,
                  fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
                  fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
                  fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
                  fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
                  fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
                  fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                  fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                  fastuidraw::c_array<int> index_adjusts);

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const = 0;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const = 0;

    const JoinOrdering &m_ordering;
    const SubsetPrivate *m_st;
    unsigned int m_num_verts, m_num_indices, m_num_chunks, m_num_joins;
    bool m_post_ctor_initalized_called;
  };

  class ArcRoundedJoinCreator:public JoinCreatorBase
  {
  public:
    ArcRoundedJoinCreator(const CapJoinChunkDepthTracking &P, const SubsetPrivate *st):
      JoinCreatorBase(P, st)
    {
      m_per_join_data.reserve(P.m_join_ordering.size());
      post_ctor_initalize();
    }

  private:
    class PerArcRoundedJoin
    {
    public:
      explicit
      PerArcRoundedJoin(const PerJoinData &J)
      {
        using namespace fastuidraw::ArcStrokedPointPacking;
        pack_join_size(J, &m_vertex_count, &m_index_count);
      }

      /* number of vertices and indices needed */
      unsigned int m_vertex_count, m_index_count;
    };

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const override
    {
      PerArcRoundedJoin J(join);

      FASTUIDRAWassert(join_id == m_per_join_data.size());
      FASTUIDRAWunused(join_id);
      m_per_join_data.push_back(J);

      vert_count += J.m_vertex_count;
      index_count += J.m_index_count;
    }

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const override
    {
      using namespace fastuidraw::ArcStrokedPointPacking;
      pack_join(join, depth,
                pts.sub_array(vertex_offset, m_per_join_data[join_id].m_vertex_count),
                indices.sub_array(index_offset, m_per_join_data[join_id].m_index_count),
                vertex_offset);
      vertex_offset += m_per_join_data[join_id].m_vertex_count;
      index_offset += m_per_join_data[join_id].m_index_count;
    }

    mutable std::vector<PerArcRoundedJoin> m_per_join_data;
  };

  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const CapJoinChunkDepthTracking &P,
                       const SubsetPrivate *st,
                       float thresh):
      JoinCreatorBase(P, st),
      m_thresh(thresh)
    {
      m_per_join_data.reserve(P.m_join_ordering.size());
      post_ctor_initalize();
    }

  private:
    class PerRoundedJoin
    {
    public:
      PerRoundedJoin(const PerJoinData &J, float thresh)
      {
        using namespace fastuidraw::StrokedPointPacking;
        pack_rounded_join_size(J, thresh, &m_num_verts, &m_num_indices);
      }

      unsigned int m_num_verts, m_num_indices;
    };

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const override
    {
      FASTUIDRAWunused(join_id);
      PerRoundedJoin J(join, m_thresh);

      FASTUIDRAWassert(join_id == m_per_join_data.size());
      FASTUIDRAWunused(join_id);
      m_per_join_data.push_back(J);

      vert_count += J.m_num_verts;
      index_count += J.m_num_indices;
    }

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const override
    {
      FASTUIDRAWassert(join_id < m_per_join_data.size());

      pts = pts.sub_array(vertex_offset, m_per_join_data[join_id].m_num_verts);
      indices = indices.sub_array(index_offset, m_per_join_data[join_id].m_num_indices);
      fastuidraw::StrokedPointPacking::pack_join(fastuidraw::PainterEnums::rounded_joins,
                                                 join, depth, pts, indices, vertex_offset);

      vertex_offset += m_per_join_data[join_id].m_num_verts;
      index_offset += m_per_join_data[join_id].m_num_indices;
    }

    float m_thresh;
    mutable std::vector<PerRoundedJoin> m_per_join_data;
  };

  template<enum fastuidraw::PainterEnums::join_style join_style>
  class GenericJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    GenericJoinCreator(const CapJoinChunkDepthTracking &P, const SubsetPrivate *st):
      JoinCreatorBase(P, st)
    {
      post_ctor_initalize();
    }

  private:
    typedef fastuidraw::StrokedPointPacking::packing_size<enum fastuidraw::PainterEnums::join_style, join_style> packing_size;

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const override
    {
      FASTUIDRAWunused(join_id);
      FASTUIDRAWunused(join);
      vert_count += packing_size::number_attributes;
      index_count += packing_size::number_indices;
    }

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &J,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const override
    {
      FASTUIDRAWunused(join_id);

      pts = pts.sub_array(vertex_offset, packing_size::number_attributes);
      indices = indices.sub_array(index_offset, packing_size::number_indices);

      fastuidraw::StrokedPointPacking::pack_join(join_style, J, depth, pts, indices, vertex_offset);
      vertex_offset += packing_size::number_attributes;
      index_offset += packing_size::number_indices;
    }
  };

  typedef GenericJoinCreator<fastuidraw::PainterEnums::bevel_joins> BevelJoinCreator;
  typedef GenericJoinCreator<fastuidraw::PainterEnums::miter_clip_joins> MiterClipJoinCreator;
  typedef GenericJoinCreator<fastuidraw::PainterEnums::miter_bevel_joins> MiterBevelJoinCreator;
  typedef GenericJoinCreator<fastuidraw::PainterEnums::miter_joins> MiterJoinCreator;

  class PointIndexCapSize
  {
  public:
    PointIndexCapSize(void):
      m_verts(0),
      m_indices(0)
    {}

    unsigned int m_verts, m_indices;
  };

  class CapCreatorBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    CapCreatorBase(const CapJoinChunkDepthTracking &P,
                   const SubsetPrivate *st,
                   PointIndexCapSize sz);

    virtual
    ~CapCreatorBase()
    {}

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const override;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const override;

  private:
    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const = 0;

    static
    void
    set_chunks(const SubsetPrivate *st,
               fastuidraw::c_array<const fastuidraw::range_type<int> > cap_vertex_ranges,
               fastuidraw::c_array<const fastuidraw::range_type<int> > cap_index_ranges,
               fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
               fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
               fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
               fastuidraw::c_array<int> index_adjusts);

    const CapOrdering &m_ordering;
    const SubsetPrivate *m_st;

    unsigned int m_num_chunks;
    PointIndexCapSize m_size;
  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const CapJoinChunkDepthTracking &P,
                      const SubsetPrivate *st,
                      float thresh):
      CapCreatorBase(P, st, compute_size(P, thresh))
    {
      using namespace fastuidraw;
      StrokedPointPacking::pack_rounded_cap_size(thresh, &m_num_verts_per_cap, &m_num_indices_per_cap);
    }

  private:
    static
    PointIndexCapSize
    compute_size(const CapJoinChunkDepthTracking &P, float thresh)
    {
      using namespace fastuidraw;

      PointIndexCapSize return_value;
      unsigned int num_caps, num_verts_per_cap, num_indices_per_cap;

      StrokedPointPacking::pack_rounded_cap_size(thresh, &num_verts_per_cap, &num_indices_per_cap);
      num_caps = P.m_cap_ordering.size();
      return_value.m_verts = num_verts_per_cap * num_caps;
      return_value.m_indices = num_indices_per_cap * num_caps;

      return return_value;
    }

    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const override
    {
      using namespace fastuidraw;

      pts = pts.sub_array(vertex_offset, m_num_verts_per_cap);
      indices = indices.sub_array(index_offset, m_num_indices_per_cap);
      StrokedPointPacking::pack_cap(StrokedPointPacking::rounded_cap, C, depth, pts, indices, vertex_offset);
      vertex_offset += m_num_verts_per_cap;
      index_offset += m_num_indices_per_cap;
    }

    unsigned int m_num_verts_per_cap, m_num_indices_per_cap;
  };

  template<enum fastuidraw::StrokedPointPacking::cap_type_t cap_type>
  class GenericCapCreator:public CapCreatorBase
  {
  public:
    explicit
    GenericCapCreator(const CapJoinChunkDepthTracking &P,
                      const SubsetPrivate *st):
      CapCreatorBase(P, st, compute_size(P))
    {}

  private:
    typedef fastuidraw::StrokedPointPacking::packing_size<enum fastuidraw::StrokedPointPacking::cap_type_t, cap_type> packing_size;

    static
    PointIndexCapSize
    compute_size(const CapJoinChunkDepthTracking &P)
    {
      PointIndexCapSize return_value;
      unsigned int num_caps;

      num_caps = P.m_cap_ordering.size();
      return_value.m_verts = packing_size::number_attributes * num_caps;
      return_value.m_indices = packing_size::number_indices * num_caps;

      return return_value;
    }

    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const override
    {
       pts = pts.sub_array(vertex_offset, packing_size::number_attributes);
       indices = indices.sub_array(index_offset, packing_size::number_indices);

       pack_cap(cap_type, C, depth, pts, indices, vertex_offset);
       vertex_offset += packing_size::number_attributes;
       index_offset += packing_size::number_indices;
    }
  };

  typedef GenericCapCreator<fastuidraw::StrokedPointPacking::square_cap> SquareCapCreator;
  typedef GenericCapCreator<fastuidraw::StrokedPointPacking::adjustable_cap> AdjustableCapCreator;

  class ArcRoundedCapCreator:public CapCreatorBase
  {
  public:
    ArcRoundedCapCreator(const CapJoinChunkDepthTracking &P,
                         const SubsetPrivate *st):
      CapCreatorBase(P, st, compute_size(P))
    {}

  private:
    static
    PointIndexCapSize
    compute_size(const CapJoinChunkDepthTracking &P)
    {
      using namespace fastuidraw;

      unsigned int num_caps;
      PointIndexCapSize return_value;

      num_caps = P.m_cap_ordering.size();
      return_value.m_verts = ArcStrokedPointPacking::num_attributes_per_cap * num_caps;
      return_value.m_indices = ArcStrokedPointPacking::num_indices_per_cap * num_caps;

      return return_value;
    }

    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const override
    {
      using namespace fastuidraw;

      pts = pts.sub_array(vertex_offset, ArcStrokedPointPacking::num_attributes_per_cap);
      indices = indices.sub_array(index_offset, ArcStrokedPointPacking::num_indices_per_cap);
      ArcStrokedPointPacking::pack_cap(C, depth, pts, indices, vertex_offset);
      vertex_offset += ArcStrokedPointPacking::num_attributes_per_cap;
      index_offset += ArcStrokedPointPacking::num_indices_per_cap;
    }
  };

  class ThreshWithData
  {
  public:
    ThreshWithData(void):
      m_data(nullptr),
      m_thresh(-1)
    {}

    ThreshWithData(fastuidraw::PainterAttributeData *d, float t):
      m_data(d), m_thresh(t)
    {}

    static
    bool
    reverse_compare_against_thresh(const ThreshWithData &lhs, float rhs)
    {
      return lhs.m_thresh > rhs;
    }

    fastuidraw::PainterAttributeData *m_data;
    float m_thresh;
  };

  template<typename T>
  class PreparedAttributeData
  {
  public:
    PreparedAttributeData(void):
      m_ready(false)
    {}

    /* must be called before the first call to data().
     */
    void
    mark_as_empty(void)
    {
      m_ready = true;
    }

    const fastuidraw::PainterAttributeData&
    data(const CapJoinChunkDepthTracking &P, const SubsetPrivate *st)
    {
      if (!m_ready)
        {
          m_data.set_data(T(P, st));
          m_ready = true;
        }
      return m_data;
    }

  private:
    fastuidraw::PainterAttributeData m_data;
    bool m_ready;
  };

  class LineEdgeAttributeFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    LineEdgeAttributeFiller(fastuidraw::PartitionedTessellatedPath::Subset src):
      m_chains(src.segment_chains())
    {
      using namespace fastuidraw::StrokedPointPacking;
      pack_segment_chain_size(m_chains,
                              &m_depth_size,
                              &m_num_attributes,
                              &m_num_indices);
    }

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const override
    {
      num_attributes = m_num_attributes;
      num_indices = m_num_indices;
      num_attribute_chunks = num_index_chunks = number_z_ranges = 1;
    }

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const override
    {
      using namespace fastuidraw::StrokedPointPacking;

      FASTUIDRAWassert(attribute_data.size() == m_num_attributes);
      FASTUIDRAWassert(index_data.size() == m_num_indices);

      attribute_chunks[0] = attribute_data;
      index_chunks[0] = index_data;
      zranges[0].m_begin = 0;
      zranges[0].m_end = m_depth_size;
      index_adjusts[0] = 0;
      pack_segment_chain(m_chains, zranges[0].m_begin,
                         attribute_data, index_data, index_adjusts[0]);
    }

  private:
    fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::segment_chain> m_chains;
    unsigned int m_depth_size;
    unsigned int m_num_attributes;
    unsigned int m_num_indices;
  };

  class ArcEdgeAttributeFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    ArcEdgeAttributeFiller(fastuidraw::PartitionedTessellatedPath::Subset src):
      m_chains(src.segment_chains())
    {
      using namespace fastuidraw::ArcStrokedPointPacking;
      pack_segment_chain_size(m_chains,
                              &m_depth_size,
                              &m_num_attributes,
                              &m_num_indices);
    }

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const override
    {
      num_attributes = m_num_attributes;
      num_indices = m_num_indices;
      num_attribute_chunks = num_index_chunks = number_z_ranges = 1;
    }

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const override
    {
      using namespace fastuidraw::ArcStrokedPointPacking;

      FASTUIDRAWassert(attribute_data.size() == m_num_attributes);
      FASTUIDRAWassert(index_data.size() == m_num_indices);

      attribute_chunks[0] = attribute_data;
      index_chunks[0] = index_data;
      zranges[0].m_begin = 0;
      zranges[0].m_end = m_depth_size;
      index_adjusts[0] = 0;
      pack_segment_chain(m_chains, zranges[0].m_begin,
                         attribute_data, index_data, index_adjusts[0]);
    }

  private:
    fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::segment_chain> m_chains;
    unsigned int m_depth_size;
    unsigned int m_num_attributes;
    unsigned int m_num_indices;
  };

  class StrokedPathPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    StrokedPathPrivate(const fastuidraw::TessellatedPath &P);
    ~StrokedPathPrivate();

    template<typename T>
    const fastuidraw::PainterAttributeData&
    fetch_create(float thresh,
                 std::vector<ThreshWithData> &values);

    bool m_has_arcs;
    fastuidraw::reference_counted_ptr<const fastuidraw::PartitionedTessellatedPath> m_path_partioned;
    SubsetPrivate* m_root;
    std::vector<SubsetPrivate*> m_subsets;

    CapJoinChunkDepthTracking m_cap_join_tracking;
    PreparedAttributeData<BevelJoinCreator> m_bevel_joins;
    PreparedAttributeData<MiterClipJoinCreator> m_miter_clip_joins;
    PreparedAttributeData<MiterJoinCreator> m_miter_joins;
    PreparedAttributeData<MiterBevelJoinCreator> m_miter_bevel_joins;
    PreparedAttributeData<ArcRoundedJoinCreator> m_arc_rounded_joins;
    PreparedAttributeData<SquareCapCreator> m_square_caps;
    PreparedAttributeData<AdjustableCapCreator> m_adjustable_caps;
    PreparedAttributeData<ArcRoundedCapCreator> m_arc_rounded_caps;
    std::vector<ThreshWithData> m_rounded_joins;
    std::vector<ThreshWithData> m_rounded_caps;
  };

}

///////////////////////////
// GenericOrdering methods
template<typename T>
void
GenericOrdering<T>::
post_process(fastuidraw::range_type<unsigned int> range,
             unsigned int &depth)
{
  unsigned int R, d;

  /* the depth values of the caps must come in reverse
   * so that higher depth values occlude later elements
   */
  R = range.difference();
  d = depth + R - 1;
  for(unsigned int i = range.m_begin; i < range.m_end; ++i, --d)
    {
      FASTUIDRAWassert(i < m_elements.size());
      m_elements[i].m_depth = d;
    }
  depth += R;
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
create_root_subset(bool has_arcs, CapJoinChunkDepthTracking &tracking,
                   const fastuidraw::PartitionedTessellatedPath &P,
                   std::vector<SubsetPrivate*> &out_values)
{
  SubsetPrivate *return_value;
  PostProcessVariables variables;

  return_value = FASTUIDRAWnew SubsetPrivate(has_arcs, tracking, P.root_subset(), out_values);
  return_value->post_process(variables, tracking);
  return return_value;
}

void
SubsetPrivate::
post_process(PostProcessVariables &variables,
             CapJoinChunkDepthTracking &tracking)
{
  /* We want the depth to go in the reverse order as the
   * draw order. The Draw order is child(0), child(1)
   * Thus, we first handle depth child(1) and then child(0).
   */
  m_joins.m_depth_range.m_begin = variables.m_join_depth;
  m_caps.m_depth_range.m_begin = variables.m_cap_depth;

  if (has_children())
    {
      FASTUIDRAWassert(m_children[0] != nullptr);
      FASTUIDRAWassert(m_children[1] != nullptr);

      m_children[1]->post_process(variables, tracking);
      m_children[0]->post_process(variables, tracking);
    }
  else
    {
      FASTUIDRAWassert(m_children[0] == nullptr);
      FASTUIDRAWassert(m_children[1] == nullptr);

      tracking.m_join_ordering.post_process(m_joins.m_elements, variables.m_join_depth);
      tracking.m_cap_ordering.post_process(m_caps.m_elements, variables.m_cap_depth);
    }

  m_joins.m_depth_range.m_end = variables.m_join_depth;
  m_caps.m_depth_range.m_end = variables.m_cap_depth;
}

SubsetPrivate::
SubsetPrivate(bool has_arcs, CapJoinChunkDepthTracking &tracking,
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
  m_joins.m_elements.m_begin = tracking.m_join_ordering.size();
  m_caps.m_elements.m_begin = tracking.m_cap_ordering.size();

  if (m_subset.has_children())
    {
      vecN<fastuidraw::PartitionedTessellatedPath::Subset, 2> children;
      children = m_subset.children();
      m_children[0] = FASTUIDRAWnew SubsetPrivate(m_has_arcs, tracking, children[0], out_values);
      m_children[1] = FASTUIDRAWnew SubsetPrivate(m_has_arcs, tracking, children[1], out_values);
    }
  else
    {
      for (const auto &J : src.joins())
        {
          tracking.m_join_ordering.add_element(J, tracking.m_join_chunk_cnt);
        }
      for (const auto &C : src.caps())
        {
          tracking.m_cap_ordering.add_element(C, tracking.m_cap_chunk_cnt);
        }
    }

  m_joins.m_elements.m_end = tracking.m_join_ordering.size();
  m_caps.m_elements.m_end = tracking.m_cap_ordering.size();
  m_joins.m_chunk = tracking.m_join_chunk_cnt++;
  m_caps.m_chunk = tracking.m_cap_chunk_cnt++;

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
               std::vector<unsigned int> &dst)
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
          dst.push_back(ID());
          return;
        }
    }

  if (has_children())
    {
      m_children[0]->select_subsets(max_attribute_cnt, max_index_cnt, dst);
      m_children[1]->select_subsets(max_attribute_cnt, max_index_cnt, dst);
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

/////////////////////////////////////////////////
// JoinCreatorBase methods
JoinCreatorBase::
JoinCreatorBase(const CapJoinChunkDepthTracking &P, const SubsetPrivate *st):
  m_ordering(P.m_join_ordering),
  m_st(st),
  m_num_verts(0),
  m_num_indices(0),
  m_num_chunks(P.m_join_chunk_cnt),
  m_num_joins(0),
  m_post_ctor_initalized_called(false)
{}

void
JoinCreatorBase::
post_ctor_initalize(void)
{
  FASTUIDRAWassert(!m_post_ctor_initalized_called);
  m_post_ctor_initalized_called = true;

  for(const PerJoinData &J : m_ordering.elements())
    {
      add_join(m_num_joins, J, m_num_verts, m_num_indices);
      ++m_num_joins;
    }
}

void
JoinCreatorBase::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  FASTUIDRAWassert(m_post_ctor_initalized_called);
  num_attributes = m_num_verts;
  num_indices = m_num_indices;
  number_z_ranges = num_attribute_chunks = num_index_chunks = m_num_chunks;
}

void
JoinCreatorBase::
fill_join(unsigned int join_id,
          const PerJoinData &join_data,
          unsigned int chunk, unsigned int depth,
          fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &vertex_offset, unsigned int &index_offset,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts,
          fastuidraw::c_array<fastuidraw::range_type<int> > join_vertex_ranges,
          fastuidraw::c_array<fastuidraw::range_type<int> > join_index_ranges) const
{
  unsigned int v(vertex_offset), i(index_offset);

  FASTUIDRAWassert(join_id < m_num_joins);

  fill_join_implement(join_id, join_data, pts, depth, indices, vertex_offset, index_offset);

  join_vertex_ranges[join_id].m_begin = v;
  join_vertex_ranges[join_id].m_end = vertex_offset;

  join_index_ranges[join_id].m_begin = i;
  join_index_ranges[join_id].m_end = index_offset;

  attribute_chunks[chunk] = pts.sub_array(v, vertex_offset - v);
  index_chunks[chunk] = indices.sub_array(i, index_offset - i);
  index_adjusts[chunk] = -int(v);
  zranges[chunk] = fastuidraw::range_type<int>(depth, depth + 1);
}

void
JoinCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0), index_offset(0), join_id(0);
  std::vector<fastuidraw::range_type<int> > jvr(m_num_joins), jir(m_num_joins);
  fastuidraw::c_array<fastuidraw::range_type<int> > join_vertex_ranges, join_index_ranges;

  join_vertex_ranges = fastuidraw::make_c_array(jvr);
  join_index_ranges = fastuidraw::make_c_array(jir);

  FASTUIDRAWassert(attribute_data.size() == m_num_verts);
  FASTUIDRAWassert(index_data.size() == m_num_indices);
  FASTUIDRAWassert(attribute_chunks.size() == m_num_chunks);
  FASTUIDRAWassert(index_chunks.size() == m_num_chunks);
  FASTUIDRAWassert(zranges.size() == m_num_chunks);
  FASTUIDRAWassert(index_adjusts.size() == m_num_chunks);

  for(const OrderingEntry<PerJoinData> &J : m_ordering.elements())
    {
      fill_join(join_id, J, J.m_chunk, J.m_depth,
                attribute_data, index_data,
                vertex_offset, index_offset,
                attribute_chunks, index_chunks,
                zranges, index_adjusts,
                join_vertex_ranges,
                join_index_ranges);
      ++join_id;
    }

  FASTUIDRAWassert(vertex_offset == m_num_verts);
  FASTUIDRAWassert(index_offset == m_num_indices);
  set_chunks(m_st,
             join_vertex_ranges, join_index_ranges,
             attribute_data, index_data,
             attribute_chunks, index_chunks,
             zranges, index_adjusts);
}

void
JoinCreatorBase::
set_chunks(const SubsetPrivate *st,
           fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
           fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
           fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
           fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
           fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
           fastuidraw::c_array<int> index_adjusts)
{
  process_chunk(st->joins(),
                join_vertex_ranges, join_index_ranges,
                attribute_data, index_data,
                attribute_chunks, index_chunks,
                zranges, index_adjusts);

  if (st->has_children())
    {
      set_chunks(st->child(0),
                 join_vertex_ranges, join_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);

      set_chunks(st->child(1),
                 join_vertex_ranges, join_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);
    }
}

void
JoinCreatorBase::
process_chunk(const RangeAndChunk &ch,
              fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
              fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
              fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts)
{
  unsigned int K;
  fastuidraw::range_type<int> vr, ir;

  if (ch.m_elements.m_begin < ch.m_elements.m_end)
    {
      vr.m_begin = join_vertex_ranges[ch.m_elements.m_begin].m_begin;
      vr.m_end = join_vertex_ranges[ch.m_elements.m_end - 1].m_end;

      ir.m_begin = join_index_ranges[ch.m_elements.m_begin].m_begin;
      ir.m_end = join_index_ranges[ch.m_elements.m_end - 1].m_end;
    }
  else
    {
      vr.m_begin = vr.m_end = 0;
      ir.m_begin = ir.m_end = 0;
    }

  K = ch.m_chunk;
  attribute_chunks[K] = attribute_data.sub_array(vr);
  index_chunks[K] = index_data.sub_array(ir);
  zranges[K] = fastuidraw::range_type<int>(ch.m_depth_range.m_begin, ch.m_depth_range.m_end);
  index_adjusts[K] = -int(vr.m_begin);
}

///////////////////////////////////////////////
// CapCreatorBase methods
CapCreatorBase::
CapCreatorBase(const CapJoinChunkDepthTracking &P,
               const SubsetPrivate *st,
               PointIndexCapSize sz):
  m_ordering(P.m_cap_ordering),
  m_st(st),
  m_num_chunks(P.m_cap_chunk_cnt),
  m_size(sz)
{
}

void
CapCreatorBase::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  num_attributes = m_size.m_verts;
  num_indices = m_size.m_indices;
  number_z_ranges = num_attribute_chunks = num_index_chunks = m_num_chunks;
}

void
CapCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0u), index_offset(0u), cap_id(0u);
  std::vector<fastuidraw::range_type<int> > cvr(m_num_chunks), cir(m_num_chunks);

  for(const OrderingEntry<PerCapData> &C : m_ordering.elements())
    {
      unsigned int v(vertex_offset), i(index_offset);

      add_cap(C, C.m_depth, attribute_data, index_data, vertex_offset, index_offset);

      cvr[cap_id].m_begin = v;
      cvr[cap_id].m_end = vertex_offset;

      cir[cap_id].m_begin = i;
      cir[cap_id].m_end = index_offset;

      attribute_chunks[C.m_chunk] = attribute_data.sub_array(cvr[cap_id]);
      index_chunks[C.m_chunk] = index_data.sub_array(cir[cap_id]);
      zranges[C.m_chunk] = fastuidraw::range_type<int>(C.m_depth, C.m_depth + 1);
      index_adjusts[C.m_chunk] = -int(v);

      ++cap_id;
    }

  FASTUIDRAWassert(vertex_offset == m_size.m_verts);
  FASTUIDRAWassert(index_offset == m_size.m_indices);

  set_chunks(m_st,
             fastuidraw::make_c_array(cvr),
             fastuidraw::make_c_array(cir),
             attribute_data, index_data,
             attribute_chunks, index_chunks,
             zranges, index_adjusts);
}

void
CapCreatorBase::
set_chunks(const SubsetPrivate *st,
           fastuidraw::c_array<const fastuidraw::range_type<int> > cap_vertex_ranges,
           fastuidraw::c_array<const fastuidraw::range_type<int> > cap_index_ranges,
           fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
           fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
           fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
           fastuidraw::c_array<int> index_adjusts)
{
  const RangeAndChunk &ch(st->caps());
  fastuidraw::range_type<int> vr, ir;
  unsigned int K;

  if (ch.m_elements.m_begin < ch.m_elements.m_end)
    {
      vr.m_begin = cap_vertex_ranges[ch.m_elements.m_begin].m_begin;
      vr.m_end = cap_vertex_ranges[ch.m_elements.m_end - 1].m_end;

      ir.m_begin = cap_index_ranges[ch.m_elements.m_begin].m_begin;
      ir.m_end = cap_index_ranges[ch.m_elements.m_end - 1].m_end;

      FASTUIDRAWassert(ch.m_depth_range.m_begin < ch.m_depth_range.m_end);
    }
  else
    {
      vr.m_begin = vr.m_end = 0;
      ir.m_begin = ir.m_end = 0;
      FASTUIDRAWassert(ch.m_depth_range.m_begin == ch.m_depth_range.m_end);
    }

  K = ch.m_chunk;
  attribute_chunks[K] = attribute_data.sub_array(vr);
  index_chunks[K] = index_data.sub_array(ir);
  zranges[K] = fastuidraw::range_type<int>(ch.m_depth_range.m_begin, ch.m_depth_range.m_end);
  index_adjusts[K] = -int(vr.m_begin);

  if (st->has_children())
    {
      set_chunks(st->child(0),
                 cap_vertex_ranges, cap_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);

      set_chunks(st->child(1),
                 cap_vertex_ranges, cap_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);
    }
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P):
  m_has_arcs(P.has_arcs()),
  m_root(nullptr)
{
  m_path_partioned = &P.partitioned();
  m_subsets.resize(m_path_partioned->number_subsets());
  m_root = SubsetPrivate::create_root_subset(m_has_arcs, m_cap_join_tracking, *m_path_partioned, m_subsets);
}

StrokedPathPrivate::
~StrokedPathPrivate()
{
  if (m_root)
    {
      FASTUIDRAWdelete(m_root);
    }

  for(unsigned int i = 0, endi = m_rounded_joins.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_rounded_joins[i].m_data);
    }

  for(unsigned int i = 0, endi = m_rounded_caps.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_rounded_caps[i].m_data);
    }
}

template<typename T>
const fastuidraw::PainterAttributeData&
StrokedPathPrivate::
fetch_create(float thresh, std::vector<ThreshWithData> &values)
{
  if (values.empty())
    {
      fastuidraw::PainterAttributeData *newD;
      newD = FASTUIDRAWnew fastuidraw::PainterAttributeData();
      newD->set_data(T(m_cap_join_tracking, m_root, 1.0f));
      values.push_back(ThreshWithData(newD, 1.0f));
    }

  /* we set a hard tolerance of 1e-6. Should we
   * set it as a ratio of the bounding box of
   * the underlying tessellated path?
   */
  thresh = fastuidraw::t_max(thresh, float(1e-6));
  if (values.back().m_thresh <= thresh)
    {
      std::vector<ThreshWithData>::const_iterator iter;
      iter = std::lower_bound(values.begin(), values.end(), thresh,
                              ThreshWithData::reverse_compare_against_thresh);
      FASTUIDRAWassert(iter != values.end());
      FASTUIDRAWassert(iter->m_thresh <= thresh);
      FASTUIDRAWassert(iter->m_data != nullptr);
      return *iter->m_data;
    }
  else
    {
      float t;
      t = values.back().m_thresh;
      while(t > thresh)
        {
          fastuidraw::PainterAttributeData *newD;

          t *= 0.5f;
          newD = FASTUIDRAWnew fastuidraw::PainterAttributeData();
          newD->set_data(T(m_cap_join_tracking, m_root, t));
          values.push_back(ThreshWithData(newD, t));
        }
      return *values.back().m_data;
    }
}

//////////////////////////////////////////////
// fastuidraw::StrokedPath::SubsetSelection methods
fastuidraw::StrokedPath::SubsetSelection::
SubsetSelection(void)
{
  m_d = FASTUIDRAWnew SubsetSelectionPrivate();
}

fastuidraw::StrokedPath::SubsetSelection::
~SubsetSelection(void)
{
  SubsetSelectionPrivate *d;
  d = static_cast<SubsetSelectionPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedPath::SubsetSelection::
subset_ids(void) const
{
  SubsetSelectionPrivate *d;
  d = static_cast<SubsetSelectionPrivate*>(m_d);
  return make_c_array(d->m_subset_ids);
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedPath::SubsetSelection::
join_subset_ids(void) const
{
  SubsetSelectionPrivate *d;
  d = static_cast<SubsetSelectionPrivate*>(m_d);
  return d->m_join_subset_ids;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath>&
fastuidraw::StrokedPath::SubsetSelection::
source(void) const
{
  SubsetSelectionPrivate *d;
  d = static_cast<SubsetSelectionPrivate*>(m_d);
  return d->m_source;
}

void
fastuidraw::StrokedPath::SubsetSelection::
clear(const reference_counted_ptr<const StrokedPath> &src)
{
  SubsetSelectionPrivate *d;
  d = static_cast<SubsetSelectionPrivate*>(m_d);

  d->m_source = src;
  d->m_subset_ids.clear();
  d->m_join_subset_ids = c_array<const unsigned int>();

  if (src)
    {
      StrokedPathPrivate *path_d;

      path_d = static_cast<StrokedPathPrivate*>(src->m_d);
      d->m_selection.clear(path_d->m_path_partioned);
    }
  else
    {
      d->m_selection.clear();
    }
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

fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::join>
fastuidraw::StrokedPath::Subset::
joins(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->join_values();
}

fastuidraw::c_array<const fastuidraw::PartitionedTessellatedPath::cap>
fastuidraw::StrokedPath::Subset::
caps(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->cap_values();
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

int
fastuidraw::StrokedPath::Subset::
cap_chunk(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->cap_chunk();
}

int
fastuidraw::StrokedPath::Subset::
join_chunk(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  FASTUIDRAWassert(d);
  return d->join_chunk();
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

void
fastuidraw::StrokedPath::
select_subsets(c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               const vec2 &one_pixel_width,
               c_array<const float> geometry_inflation,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               bool select_miter_joins,
               SubsetSelection &dst) const
{
  StrokedPathPrivate *d;
  SubsetSelectionPrivate *dst_d;

  d = static_cast<StrokedPathPrivate*>(m_d);
  dst_d = static_cast<SubsetSelectionPrivate*>(dst.m_d);

  dst_d->m_source = this;
  dst_d->m_subset_ids.clear();
  dst_d->m_join_subset_ids = c_array<const unsigned int>();
  if (d->m_root)
    {
      d->m_path_partioned->select_subsets(clip_equations,
                                          clip_matrix_local,
                                          one_pixel_width,
                                          geometry_inflation,
                                          select_miter_joins,
                                          dst_d->m_selection);

      for (unsigned int I : dst_d->m_selection.subset_ids())
        {
          d->m_subsets[I]->select_subsets(max_attribute_cnt,
                                          max_index_cnt,
                                          dst_d->m_subset_ids);
        }

      if (select_miter_joins)
        {
          dst_d->m_join_subset_ids = dst_d->m_selection.join_subset_ids();
        }
      else
        {
          dst_d->m_join_subset_ids = make_c_array(dst_d->m_subset_ids);
        }
    }
}

void
fastuidraw::StrokedPath::
select_subsets_no_culling(unsigned int max_attribute_cnt,
                          unsigned int max_index_cnt,
                          SubsetSelection &dst) const
{
  StrokedPathPrivate *d;
  SubsetSelectionPrivate *dst_d;

  d = static_cast<StrokedPathPrivate*>(m_d);
  dst_d = static_cast<SubsetSelectionPrivate*>(dst.m_d);

  dst_d->m_source = this;
  dst_d->m_subset_ids.clear();
  if (d->m_root)
    {
      d->m_root->select_subsets(max_attribute_cnt,
                                max_index_cnt,
                                dst_d->m_subset_ids);
    }
  dst_d->m_join_subset_ids = make_c_array(dst_d->m_subset_ids);
}

fastuidraw::StrokedPath::Subset
fastuidraw::StrokedPath::
root_subset(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return Subset(d->m_root);
}

const fastuidraw::PartitionedTessellatedPath&
fastuidraw::StrokedPath::
partitioned_path(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return *d->m_path_partioned;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
square_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_square_caps.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
adjustable_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_adjustable_caps.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_bevel_joins.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_clip_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_clip_joins.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_bevel_joins.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_joins.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
arc_rounded_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_arc_rounded_joins.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
arc_rounded_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_arc_rounded_caps.data(d->m_cap_join_tracking, d->m_root);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_joins(float thresh) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);

  return d->fetch_create<RoundedJoinCreator>(thresh, d->m_rounded_joins);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_caps(float thresh) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->fetch_create<RoundedCapCreator>(thresh, d->m_rounded_caps);
}
