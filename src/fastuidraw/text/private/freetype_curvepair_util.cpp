/*!
 * \file freetype_curvepair_util.cpp
 * \brief file freetype_curvepair_util.cpp
 *
 * Adapted from: WRATHTextureFontFreeType_CurveAnalytic.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 *
 * Contact: info@nomovok.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 *
 */

#include <map>
#include <functional>

#include <fastuidraw/text/glyph_render_data_curve_pair.hpp>
#include "freetype_util.hpp"
#include "freetype_curvepair_util.hpp"
#include "../../private/array2d.hpp"

/*
  Such a mess. Oh well. This entire code does two things:
   - Take as input the geometry of an FT_Outline and generate
     curve pair data. The main pain here are:
      * cubics are not directly supported; instead they are broken
        into quadratics.
      * if a bezier curve is too tiny(like smaller than a texel),
        it should be collapsed
     These two tasks are implemented in CollapsingContourEmitter

   - for each texel, compute what curve pair is active in the texel.
     This is implemented in the class IndexTextureData
 */

using namespace fastuidraw;

namespace
{
  enum rule_type
    {
      or_rule,
      and_rule
    };

  class MakeEvenFilter:public fastuidraw::detail::geometry_data_filter
  {
  public:
    virtual
    fastuidraw::ivec2
    apply_filter(const fastuidraw::ivec2 &in_pt, enum fastuidraw::detail::point_type::point_classification cl) const
    {
      if(cl==fastuidraw::detail::point_type::on_curve)
        {
          return fastuidraw::ivec2( in_pt.x() + (in_pt.x()&1),
                                   in_pt.y() + (in_pt.y()&1));
        }
      else
        {
          return in_pt;
        }
    }
  };

  class TaggedOutlineData;

  fastuidraw::vec2
  get_point(const fastuidraw::ivec2 &texel_bl, const fastuidraw::ivec2 &texel_tr,
            int side,
            const fastuidraw::detail::simple_line *L)
  {
    enum fastuidraw::detail::boundary_type v(static_cast<enum fastuidraw::detail::boundary_type>(side));
    fastuidraw::vec2 R;
    int fixed_coord;

    fixed_coord=fastuidraw::detail::fixed_coordinate(fastuidraw::detail::side_type(v));

    R[fixed_coord]=(fastuidraw::detail::is_min_side_type(v))?
      texel_bl[fixed_coord]:
      texel_tr[fixed_coord];

    R[1-fixed_coord]=L->m_value;

    return R;
  }

  float
  compute_area(fastuidraw::vec2 a, fastuidraw::vec2 b, fastuidraw::vec2 c)
  {
    b-=a;
    c-=a;
    return 0.5f*std::abs( b.x()*c.y() - b.y()*c.x());
  }


  fastuidraw::vecN<vec2, 2>
  get_corner_points(const fastuidraw::ivec2 &texel_bl, const fastuidraw::ivec2 &texel_tr,
                    int side0, int side1, const fastuidraw::vec2 &if_not_found)
  {
    fastuidraw::vecN<vec2, 2> R;
    enum fastuidraw::detail::boundary_type v0(static_cast<enum fastuidraw::detail::boundary_type>(side0));
    enum fastuidraw::detail::boundary_type v1(static_cast<enum fastuidraw::detail::boundary_type>(side1));

    if(v0==fastuidraw::detail::opposite_boundary(v1))
      {
        if(fastuidraw::detail::side_type(v0)==fastuidraw::detail::x_fixed)
          {
            R[0]=vec2(texel_bl.x(), texel_bl.y());
            R[1]=vec2(texel_tr.x(), texel_bl.y());
          }
        else
          {
            R[0]=vec2(texel_bl.x(), texel_bl.y());
            R[1]=vec2(texel_bl.x(), texel_tr.y());
          }
      }
    else
      {
        R[1]=if_not_found;

        if(side_type(v0)!=fastuidraw::detail::x_fixed)
          {
            std::swap(v0, v1);
          }

        /*
          v0 is either left or right
          and v1 is either below or above.
        */
        FASTUIDRAWassert(v0==fastuidraw::detail::left_boundary or v0==fastuidraw::detail::right_boundary);
        FASTUIDRAWassert(v1==fastuidraw::detail::below_boundary or v1==fastuidraw::detail::above_boundary);

        R[0].x()= (v0==fastuidraw::detail::left_boundary)?
          texel_bl.x():
          texel_tr.x();

        R[0].y()= (v0==fastuidraw::detail::below_boundary)?
          texel_bl.y():
          texel_tr.y();
      }
    return R;
  }

  class IndexTextureData
  {
  public:
    IndexTextureData(TaggedOutlineData &outline_data, fastuidraw::ivec2 bitmap_size,
                     c_array<uint16_t> pixel_data);

    void
    fill_index_data(void);

  private:
    typedef const fastuidraw::detail::simple_line* curve_cache_value_entry;
    typedef std::map<int, std::vector<curve_cache_value_entry> > curve_cache_value;
    typedef const fastuidraw::detail::BezierCurve *curve_cache_key;
    typedef std::map<curve_cache_key, curve_cache_value> curve_cache;

    uint16_t
    select_index(int x, int y);


    bool
    intersection_should_be_used(int side, const fastuidraw::detail::simple_line *intersection);


    enum return_code
    sub_select_index(uint16_t &pixel_value,
                     const curve_cache &curves,
                     int x, int y, int winding_value);


    uint16_t
    sub_select_index_hard_case(curve_cache &curves,
                               int x, int y,
                               const fastuidraw::ivec2 &texel_bl,
                               const fastuidraw::ivec2 &texel_tr);

    void
    remove_edge_huggers(curve_cache &curves,
                        const fastuidraw::ivec2 &texel_bl,
                        const fastuidraw::ivec2 &texel_tr);

    static
    bool
    curve_hugs_edge(const fastuidraw::detail::BezierCurve *curve,
                    const fastuidraw::ivec2 &texel_bl, const fastuidraw::ivec2 &texel_tr,
                    int thresshold);


    std::pair<float, const fastuidraw::detail::BezierCurve*>
    compute_feature_importance(curve_cache &cache,
                               curve_cache::iterator iter,
                               const fastuidraw::ivec2 &texel_bl, const fastuidraw::ivec2 &texel_tr,
                               float texel_area);

    fastuidraw::ivec2 m_bitmap_sz;
    const TaggedOutlineData &m_outline_data;
    c_array<uint16_t> m_index_pixels;
    std::vector<bool> m_reverse_components;
    array2d<fastuidraw::detail::analytic_return_type> m_intersection_data;
    array2d<int> m_winding_values;
  };



  class CollapsingContourEmitter:
    public fastuidraw::detail::ContourEmitterBase,
    public fastuidraw::detail::CoordinateConverter
  {
  public:

    CollapsingContourEmitter(float curvature_collapse,
                             const FT_Outline &outline,
                             const fastuidraw::detail::CoordinateConverter &conv):
      fastuidraw::detail::CoordinateConverter(conv),
      m_real_worker(outline, conv.scale_factor()),
      m_curvature_collapse(curvature_collapse)
    {}

    virtual
    void
    produce_contours(fastuidraw::detail::geometry_data data)
    {
      consumer_state S(this, data);
      m_real_worker.produce_contours(data);
    }

  private:

    class consumer_state
    {
    public:
      consumer_state(CollapsingContourEmitter *master,
                     fastuidraw::detail::geometry_data data);

      ~consumer_state()
      {
        m_consume_curves.disconnect();
        m_consume_contours.disconnect();
      }

    private:

      void
      consume_curve(fastuidraw::detail::BezierCurve *curve);

      void
      consume_contour(void);

      CollapsingContourEmitter *m_master;
      std::vector< std::pair<fastuidraw::detail::BezierCurve*, bool> > m_curves;
      std::vector< std::pair<fastuidraw::detail::BezierCurve*, int> > m_curves_to_emit;

      fastuidraw::detail::geometry_data m_data;
      signal_emit_curve::Connection m_consume_curves;
      signal_end_contour::Connection m_consume_contours;
    };

    static
    float
    compute_curvature(const fastuidraw::detail::BezierCurve *curve);

    fastuidraw::detail::ContourEmitterFromFT_Outline m_real_worker;
    float m_curvature_collapse;
  };


  class TaggedOutlineData:public fastuidraw::detail::OutlineData
  {
  public:
    TaggedOutlineData(CollapsingContourEmitter *collapsing_contour_emitter,
                      const fastuidraw::detail::geometry_data gmt);

    void
    fill_geometry_data(fastuidraw::c_array<fastuidraw::GlyphRenderDataCurvePair::entry> data);
  };

}

/////////////////////////////////////
// TaggedOutlineData methods
TaggedOutlineData::
TaggedOutlineData(CollapsingContourEmitter *collapsing_contour_emitter,
                  const fastuidraw::detail::geometry_data gmt):
  fastuidraw::detail::OutlineData(collapsing_contour_emitter,
                                  *collapsing_contour_emitter,
                                  gmt)
{}

void
TaggedOutlineData::
fill_geometry_data(fastuidraw::c_array<fastuidraw::GlyphRenderDataCurvePair::entry> data)
{
  std::vector<fastuidraw::vec2> pts;

  pts.reserve(5);
  for(unsigned int i=0, endi = number_curves(); i < endi; ++i)
    {
      const fastuidraw::detail::BezierCurve *c, *n;

      /* Step 1: get the geometry:
       */
      c = bezier_curve(i);
      n = next_neighbor(c);
      pts.clear();

      for(unsigned int k = 0; k < c->control_points().size(); ++k)
        {
          fastuidraw::vec2 p;

          p = bitmap_from_point(c->control_point(k), fastuidraw::detail::bitmap_begin);
          FASTUIDRAWassert(k < 3);
          pts.push_back(p);
        }
      for(unsigned int k = 1; k < n->control_points().size(); ++k)
        {
          fastuidraw::vec2 p;

          p = bitmap_from_point(n->control_point(k), fastuidraw::detail::bitmap_begin);
          FASTUIDRAWassert(k < 3);
          pts.push_back(p);
        }

      /* Step 2: generate an entry from it:
       */
      fastuidraw::GlyphRenderDataCurvePair::entry E(fastuidraw::make_c_array(pts), c->control_points().size());
      data[i] = fastuidraw::GlyphRenderDataCurvePair::entry(E);
    }


}



//////////////////////////////////////
// CollapsingContourEmitter methods
CollapsingContourEmitter::consumer_state::
consumer_state(CollapsingContourEmitter *master,
               fastuidraw::detail::geometry_data data):
  m_master(master),
  m_data(data),
  m_consume_curves(m_master->m_real_worker.connect_emit_curve(std::bind(&CollapsingContourEmitter::consumer_state::consume_curve, this, std::placeholders::_1))),
  m_consume_contours(m_master->m_real_worker.connect_emit_end_contour(std::bind(&CollapsingContourEmitter::consumer_state::consume_contour, this)))
{
}



void
CollapsingContourEmitter::consumer_state::
consume_curve(fastuidraw::detail::BezierCurve *curve)
{
  /*
    Step 1: detect if the start and end position
    of curve are within the same texel:
   */
  fastuidraw::ivec2 p0, p1;
  fastuidraw::ivec2 tp0, tp1;
  std::pair<fastuidraw::detail::BezierCurve*, bool> v(curve, false);

  p0=curve->pt0();
  p1=curve->pt1();

  tp0=m_master->texel(p0);
  tp1=m_master->texel(p1);


  v.second=(tp0==tp1);

  if(curve->degree()==3)
    {
      fastuidraw::vecN<fastuidraw::detail::BezierCurve*, 4> quads_4;
      fastuidraw::vecN<fastuidraw::detail::BezierCurve*, 4> quads_2;
      fastuidraw::vecN<fastuidraw::detail::BezierCurve*, 1> quads_1;
      c_array<fastuidraw::detail::BezierCurve*> quads;
      enum return_code R;
      bool split_as_4, split_as_2;
      int L1dist;

      FASTUIDRAWunused(R);

      /*
        "small" cubics, i.e. those whose end points
        are 2 or fewer texels apart are broken into
        1 or 2 quads rather than 4.
       */
      L1dist=(tp0-tp1).L1norm();
      split_as_4= (L1dist>6);
      split_as_2= (L1dist>3);

      if(split_as_4)
        {
          R=curve->approximate_cubic(m_data, quads_4);
          quads=quads_4;

          FASTUIDRAWassert(R==routine_success);
        }
      else if(split_as_2)
        {
          R=curve->approximate_cubic(m_data, quads_2);
          quads=quads_2;

          FASTUIDRAWassert(R==routine_success);
        }
      else
        {
          R=curve->approximate_cubic(m_data, quads_1);
          quads=quads_1;

          FASTUIDRAWassert(R==routine_success);
        }

      for(unsigned int i=0; i<quads.size(); ++i)
        {
          std::pair<fastuidraw::detail::BezierCurve*, bool> w(quads[i], false);
          fastuidraw::ivec2 wp0, wp1;
          fastuidraw::ivec2 wtp0, wtp1;

          wp0=quads[i]->pt0();
          wp1=quads[i]->pt1();
          wtp0=m_master->texel(wp0);
          wtp1=m_master->texel(wp1);
          w.second=(wtp0==wtp1);

          m_curves.push_back(w);
        }
      FASTUIDRAWdelete(curve);
    }
  else
    {
      m_curves.push_back(v);
    }
}



void
CollapsingContourEmitter::consumer_state::
consume_contour(void)
{

  for(unsigned int i=0; i<m_curves.size(); ++i)
    {
      if(!m_curves[i].second)
        {
          m_curves_to_emit.push_back( std::make_pair(m_curves[i].first, i));
        }
    }

  if(m_curves_to_emit.empty())
    {
      /*
        all curves within the same texel,
        thus we wil ignore the entire contour!
       */
      for(unsigned int i=0; i<m_curves.size(); ++i)
        {
          FASTUIDRAWdelete(m_curves[i].first);
        }
      m_curves.clear();
      return;
    }

  for(unsigned int C=0, endC=m_curves_to_emit.size(); C+1<endC; ++C)
    {
      /*
        loop through the curves that are to be destroyed...
       */
      fastuidraw::ivec2 pt(m_curves_to_emit[C].first->pt1());
      unsigned int number_skipped(0);

      for(int k=m_curves_to_emit[C].second+1; k<m_curves_to_emit[C+1].second; ++k, ++number_skipped)
        {
          FASTUIDRAWassert(m_curves[k].first!=nullptr);
          pt+=m_curves[k].first->pt1();

          FASTUIDRAWdelete(m_curves[k].first);
          m_curves[k].first=nullptr;
        }

      if(number_skipped>0)
        {
          pt/=(1+number_skipped);
          std::vector<uint16_t> indices;
          uint16_t new_pt_index;

          new_pt_index=m_data.push_back(pt, FT_CURVE_TAG_ON);

          indices=m_curves_to_emit[C].first->control_point_indices();
          indices.back()=new_pt_index;
          *m_curves_to_emit[C].first=fastuidraw::detail::BezierCurve(m_data, indices);

          indices=m_curves_to_emit[C+1].first->control_point_indices();
          indices.front()=new_pt_index;
          *m_curves_to_emit[C+1].first=fastuidraw::detail::BezierCurve(m_data, indices);
        }
    }

  if(!m_curves_to_emit.empty())
    {
      int number_skipped(0);
      fastuidraw::ivec2 pt(m_curves_to_emit.back().first->pt1());

      for(int k=m_curves_to_emit.back().second+1, end_k=m_curves.size(); k<end_k; ++k, ++number_skipped)
        {
          FASTUIDRAWassert(m_curves[k].first!=nullptr);
          pt+=m_curves[k].first->pt1();

          FASTUIDRAWdelete(m_curves[k].first);
          m_curves[k].first=nullptr;
        }

      for(int k=0; k<m_curves_to_emit.front().second; ++k, ++number_skipped)
        {
          FASTUIDRAWassert(m_curves[k].first!=nullptr);
          pt+=m_curves[k].first->pt1();

          FASTUIDRAWdelete(m_curves[k].first);
          m_curves[k].first=nullptr;
        }

      if(number_skipped>0)
        {
          pt/=(1+number_skipped);
          std::vector<uint16_t> indices;
          uint16_t new_pt_index;

          new_pt_index=m_data.push_back(pt, FT_CURVE_TAG_ON);

          indices=m_curves_to_emit.back().first->control_point_indices();
          indices.back()=new_pt_index;
          *m_curves_to_emit.back().first=fastuidraw::detail::BezierCurve(m_data, indices);

          indices=m_curves_to_emit.front().first->control_point_indices();
          indices.front()=new_pt_index;
          *m_curves_to_emit.front().first=fastuidraw::detail::BezierCurve(m_data, indices);
        }
    }

  for(unsigned int C=0, endC=m_curves_to_emit.size(); C<endC; ++C)
    {
      fastuidraw::detail::BezierCurve *ptr(m_curves_to_emit[C].first);

      if(ptr->degree()==2 and m_master->m_curvature_collapse>0.0f)
        {
          float curvature;

          curvature=compute_curvature(ptr);
          if(curvature < m_master->m_curvature_collapse)
            {
              std::vector<uint16_t> indices(2);

              indices[0]=ptr->control_point_indices().front();
              indices[1]=ptr->control_point_indices().back();

              *ptr=fastuidraw::detail::BezierCurve(m_data, indices);
            }
        }

      m_master->emit_curve(ptr);
    }

  m_curves.clear();
  m_curves_to_emit.clear();
  m_master->emit_end_contour();
}

//////////////////////////////////////////
// CollapsingContourEmitter methods
float
CollapsingContourEmitter::
compute_curvature(const fastuidraw::detail::BezierCurve *ptr)
{
  if(ptr->degree()!=2)
    {
      return 0.0f;
    }


  /*
    Curvature = integral_{t=0}^{t=1} K(t) || p_t(t) || dt

    p(t) = a0 + a1*t + a2*t*t

    K(t)= || p_t X p_tt || / || p_t ||^ 3

    then

    Curvature = integral_{t=0}^{t=1} ||a1 X a2||/( ||a1||^2 + 2t<a1,a2> + t^2 ||a2||^2 )

    Notes:
       Integral ( 1/(a+bx+cxx) ) dx = 2 atan( (b+2cx)/d ) / d
       where d=sqrt(4ac-b*b)

    and
       integral_{x=0}^{x=1} dx = 2/d * ( atan( (b+2c)/d ) - atan(b/d) )
                               = 2/d * atan( ( (b+2c)/d - b/d)/(1 + (b+2c)*b/(d*d) ) )
                               = 2/d * atan( 2cd/( dd + bb + 2cb))
                               = 2/d * atan( 2cd/( 4ac - bb + bb + 2cb))
                               = 2/d * atan( d/(2a + b) )

   */

  const std::vector<int> &src_x(ptr->curve().x());
  const std::vector<int> &src_y(ptr->curve().y());
  fastuidraw::vec2 a1(src_x[1], src_y[1]), a2(src_x[2], src_y[2]);
  float a, b, c, R, desc, tt;

  R=std::abs(a1.x()*a2.y() - a1.y()*a2.x());
  a=dot(a1, a1);
  b=2.0f*dot(a1, a2);
  c=dot(a2, a2);

  const float epsilon(0.000001f), epsilon2(epsilon*epsilon);

  desc=std::sqrt(std::max(epsilon2, 4.0f*a*c - b*b));
  tt= desc/std::max(epsilon, std::abs(2.0f*a + b));
  return 2.0*R*atanf(tt) / desc;

}



///////////////////////////////////////////
//IndexTextureData methods
IndexTextureData::
IndexTextureData(TaggedOutlineData &outline_data,
                 fastuidraw::ivec2 bitmap_size,
                 c_array<uint16_t> pixel_data_out):
  m_bitmap_sz(bitmap_size),
  m_outline_data(outline_data),
  m_index_pixels(pixel_data_out),
  m_intersection_data(m_bitmap_sz.x(), m_bitmap_sz.y()),
  m_winding_values(m_bitmap_sz.x(), m_bitmap_sz.y())
{
  std::fill(m_index_pixels.begin(), m_index_pixels.end(), fastuidraw::GlyphRenderDataCurvePair::completely_empty_texel);
  FASTUIDRAWassert(m_index_pixels.size() == static_cast<unsigned int>(m_bitmap_sz.x() * m_bitmap_sz.y()));

  m_outline_data.compute_analytic_values(m_intersection_data, m_reverse_components, true);
  m_outline_data.compute_winding_numbers(m_winding_values, fastuidraw::ivec2(0, 0));

  for(int I=0; I<m_outline_data.number_components(); ++I)
    {
      if(m_reverse_components[I])
        {
          outline_data.reverse_component(I);
        }
    }

  /*
    we need to also reverse the data of m_intersection_data
    for those records that use a curve that was reversed:
   */
  for(int x=0; x<m_bitmap_sz.x() - 1; ++x)
    {
      for(int y=0;y<m_bitmap_sz.y() - 1; ++y)
        {
          fastuidraw::detail::analytic_return_type &current(m_intersection_data(x, y));
          for(int side=0; side<4; ++side)
            {
              for(std::vector<fastuidraw::detail::simple_line>::iterator
                    iter=current.m_intersecions[side].begin(),
                    end=current.m_intersecions[side].end();
                  iter!=end; ++iter)
                {
                  int contourID;
                  contourID=iter->m_source.m_bezier->contourID();
                  if(m_reverse_components[contourID])
                    {
                      iter->observe_curve_reversal();
                    }
                }
            }
        }
    }
}

void
IndexTextureData::
fill_index_data(void)
{
  FASTUIDRAWassert(m_bitmap_sz.x()>0);
  FASTUIDRAWassert(m_bitmap_sz.y()>0);

  /*
    should we add slack to the image?
   */
  for(int x=0;x<m_bitmap_sz.x() - 1; ++x)
    {
      for(int y=0;y<m_bitmap_sz.y() - 1; ++y)
        {
          uint16_t &pixel(m_index_pixels[x+ y*m_bitmap_sz.x()]);

          pixel=select_index(x, y);
        }
    }
}


bool
IndexTextureData::
curve_hugs_edge(const fastuidraw::detail::BezierCurve *curve,
                const fastuidraw::ivec2 &texel_bl, const fastuidraw::ivec2 &texel_tr,
                int thresshold)
{
  if(curve->degree()!=1)
    {
      return false;
    }

  fastuidraw::ivec2 pt0(curve->pt0()), pt1(curve->pt1());

  if(pt0.x()==pt1.x())
    {
      if( std::abs(pt0.x()-texel_bl.x())<thresshold or
          std::abs(pt0.x()-texel_tr.x())<thresshold)
        {
          return true;
        }
    }
  else if(pt0.y()==pt1.y())
    {
      if( std::abs(pt0.y()-texel_bl.y())<thresshold or
          std::abs(pt0.y()-texel_tr.y())<thresshold)
        {
          return true;
        }
    }

  return false;

}

std::pair<float, const fastuidraw::detail::BezierCurve*>
IndexTextureData::
compute_feature_importance(curve_cache &curves,
                           curve_cache::iterator iter,
                           const fastuidraw::ivec2 &texel_bl, const fastuidraw::ivec2 &texel_tr,
                           float texel_area)
{
  const fastuidraw::detail::BezierCurve *a(iter->first), *b(nullptr);

  if(iter->second.size()>=2)
    {
      vec2 pt0, pt1;
      fastuidraw::vecN<vec2, 2> pt2s;
      float area0, area0a, area0b, area1;

      pt0=get_point(texel_bl, texel_tr,
                    iter->second.begin()->first, //side
                    iter->second.begin()->second.front());

      pt1=get_point(texel_bl, texel_tr,
                    iter->second.rbegin()->first, //side
                    iter->second.rbegin()->second.front());



      pt2s=get_corner_points(texel_bl, texel_tr,
                             iter->second.begin()->first,
                             iter->second.rbegin()->first,
                             pt0);

      pt1-=pt0;
      pt2s[0]-=pt0;
      pt2s[1]-=pt0;


      area0a=0.5*std::abs( pt1.x()*pt2s[0].y() - pt2s[0].x()*pt1.y());
      area0b=0.5*std::abs( pt1.x()*pt2s[1].y() - pt2s[1].x()*pt1.y());

      area0=area0a + area0b;
      area1=texel_area-area0;

      fastuidraw::vecN<vec2,3> TA(pt0, pt1+pt0, pt0+pt2s[0]);
      fastuidraw::vecN<vec2,3> TB(pt0, pt1+pt0, pt0+pt2s[1]);

      return std::make_pair(std::abs(area1-area0), a);
    }
  else
    {
      FASTUIDRAWassert(iter->second.size()==1);

      /*
        An end point ends inside the texel, thus we
        need to compute the "triangle" of the curve
        that uses that end point.
       */
      curve_cache::iterator neighbor;
      vec2 pt0, pt1, pt2;
      fastuidraw::vecN<vec2, 2> pt3;
      std::pair<float, const fastuidraw::detail::BezierCurve*> return_value;

      neighbor=curves.find(m_outline_data.next_neighbor(a));
      if(neighbor==curves.end())
        {
          neighbor=curves.find(m_outline_data.prev_neighbor(a));
          if(neighbor==curves.end())
            {
              /*
                the curve goes in and out the same side,
                i.e. the curve is a quadratic.. we will be
                lazy and pretend the area can be approximated
                by a triangle, we will use the extremal
                point of curve.
               */
              b=a;
              return_value.second=b;

              int side(iter->second.begin()->first);
              enum fastuidraw::detail::boundary_type v(static_cast<enum fastuidraw::detail::boundary_type>(side));
              enum fastuidraw::detail::coordinate_type side_type(fastuidraw::detail::side_type(v));
              int coord(fastuidraw::detail::fixed_coordinate(side_type));

              const char *side_type_labels[]=
                {
                  "left_boundary",
                  "right_boundary",
                  "below_boundary",
                  "above_boundary",
                  "no_boundary",
                };

              const char *coord_labels[2]=
                {
                  "x_fixed",
                  "y_fixed"
                };

              FASTUIDRAWunused(side_type_labels);
              FASTUIDRAWunused(coord_labels);

              if(a->extremal_points(coord).empty())
                {
                  /*
                    likely the pair of curve-a was tossed out because it was
                    parallel to a side and close to that side,
                    we make it so that this entry is still a canidate,
                    that will lose against all others be using 10X the
                    texel area as the area-diff value.
                   */
                  return std::make_pair(10.0f*texel_area, a);
                }
              pt0=a->extremal_points(coord).front();
              neighbor=iter;
            }
          else
            {
              b=neighbor->first;
              pt0=b->fpt1();
              return_value.second=b;
            }
        }
      else
        {
          b=neighbor->first;
          pt0=b->fpt0();
          return_value.second=a;
        }

      int sideA, sideB;

      sideA=iter->second.begin()->first;
      sideB=neighbor->second.rbegin()->first;


      /*
        make sure that sideA/iter is either on
        the left or bottom side.
        This is needed because when we compute
        the area of a potential pentagon by
        computing the area of a triangle fan.
       */
      if(sideA==fastuidraw::detail::above_boundary
         or sideA==fastuidraw::detail::right_boundary)
        {
          std::swap(sideA, sideB);
          std::swap(iter, neighbor);
        }


      pt1=get_point(texel_bl, texel_tr,
                    sideA,
                    iter->second.begin()->second.front());

      pt2=get_point(texel_bl, texel_tr,
                    sideB,
                    neighbor->second.rbegin()->second.front());


      if(sideA!=sideB)
        {
          pt3=get_corner_points(texel_bl, texel_tr,
                                sideA, sideB,
                                pt0);

          float area0a, area0b, area0c, area0, area1;


          area0a=compute_area(pt0, pt1, pt3[0]);
          area0b=compute_area(pt0, pt3[0], pt3[1]);
          area0c=compute_area(pt0, pt3[1], pt2);

          area0=area0a + area0b + area0c;
          area1=texel_area-area0;
          return_value.first=std::abs(area1-area0);

          fastuidraw::vecN<vec2,3> TA(pt0, pt1, pt3[0]);
          fastuidraw::vecN<vec2,3> TB(pt0, pt3[0], pt3[1]);
          fastuidraw::vecN<vec2,3> TC(pt0, pt3[1], pt2);

        }
      else
        {
          float area0, area1;

          area0=compute_area(pt0, pt1, pt2);
          area1=texel_area-area0;
          return_value.first=std::abs(area1-area0);

          fastuidraw::vecN<vec2,3> T(pt0, pt1, pt2);
        }


      return return_value;
    }
}


enum return_code
IndexTextureData::
sub_select_index(uint16_t &pixel,
                 const curve_cache &curves,
                 int x, int y,
                 int winding_value)
{
  enum return_code R(routine_fail);

  switch(curves.size())
    {
    case 0:
      {
        bool is_full;
        is_full=(winding_value!=0);

        if(is_full)
          {
            pixel=static_cast<uint16_t>(fastuidraw::GlyphRenderDataCurvePair::completely_full_texel);
          }
        else
          {
            pixel=static_cast<uint16_t>(fastuidraw::GlyphRenderDataCurvePair::completely_empty_texel);
          }
        return routine_success;
      }
      break;

    case 1:
      {
        const fastuidraw::detail::BezierCurve *a, *b;
        fastuidraw::ivec2 texel_center, ta, tb;
        int da, db;

        /*
          we need to choose do we take a and a->next_neighbor
          or a->previous_neighbor and a:
        */
        a=curves.begin()->first;
        b=m_outline_data.prev_neighbor(a);

        FASTUIDRAWassert(b->pt1() == a->pt0());

        texel_center=m_outline_data.point_from_bitmap(fastuidraw::ivec2(x, y));
        ta=texel_center - a->pt1();
        tb=texel_center - b->pt1();
        da=std::min( std::abs(ta.x()), std::abs(ta.y()) );
        db=std::min( std::abs(tb.x()), std::abs(tb.y()) );

        if(da<db)
          {
            pixel=static_cast<uint16_t>(a->curveID());
          }
        else
          {
            pixel=static_cast<uint16_t>(b->curveID());
          }
        R=routine_success;
      }
      break;

    case 2:
      {
        const fastuidraw::detail::BezierCurve *a, *b;

        a=curves.begin()->first;
        b=curves.rbegin()->first;

        FASTUIDRAWassert(a!=b);
        if(m_outline_data.next_neighbor(a)==b)
          {
            pixel=static_cast<uint16_t>(a->curveID());
            R=routine_success;
          }
        else if(m_outline_data.next_neighbor(b)==a)
          {
            pixel=static_cast<uint16_t>(b->curveID());
            R=routine_success;
          }
      }

    }

  return R;
}

uint16_t
IndexTextureData::
sub_select_index_hard_case(curve_cache &curves,
                           int x, int y,
                           const fastuidraw::ivec2 &texel_bl,
                           const fastuidraw::ivec2 &texel_tr)
{
  FASTUIDRAWunused(x);
  FASTUIDRAWunused(y);

  const fastuidraw::detail::BezierCurve *best_canidate(nullptr);
  float current_distance(0.0f);
  float texel_area;

  texel_area=std::abs(texel_bl.x()-texel_tr.x())*std::abs(texel_bl.y()-texel_tr.y());

  for(curve_cache::iterator iter=curves.begin(),
        end=curves.end(); iter!=end; ++iter)
    {
      std::pair<float, const fastuidraw::detail::BezierCurve*> v;

      v=compute_feature_importance(curves, iter,
                                   texel_bl, texel_tr,
                                   texel_area);

      if(v.second!=nullptr)
        {
          if(best_canidate==nullptr or v.first<current_distance)
            {
              best_canidate=v.second;
              current_distance=v.first;
            }
        }

    }
  FASTUIDRAWassert(best_canidate!=nullptr);
  return best_canidate->curveID();
}

void
IndexTextureData::
remove_edge_huggers(curve_cache &curves,
                    const fastuidraw::ivec2 &texel_bl,
                    const fastuidraw::ivec2 &texel_tr)
{

  int threshold(8);

  for(curve_cache::iterator iter=curves.begin(), end=curves.end(); iter!=end; )
    {
      if(curve_hugs_edge(iter->first, texel_bl, texel_tr, threshold))
        {
          curve_cache::iterator r(iter);

          ++iter;
          curves.erase(r);
        }
      else
        {
          ++iter;
        }
    }
}

bool
IndexTextureData::
intersection_should_be_used(int side,
                            const fastuidraw::detail::simple_line *intersection)
{
  /*
    because we record intersection with end points,
    we need to filter out those intersections
    that with a end point AND the curve is going
    out from the texel at the end point.

    side is a value from the enumeration
    fastuidraw::detail::boundary_type

    basic beans:
     if m_intersection type is not intersect_interior,
     then get the derivative.

     From there make a dot product with the outward vector
     perpindicular to the edge named by side.
     If it is positive, remove the edge.
   */


  if(intersection->m_intersection_type==fastuidraw::detail::intersect_interior)
    {
      return true;
    }

  fastuidraw::ivec2 deriv;

  deriv=(intersection->m_intersection_type==fastuidraw::detail::intersect_at_0)?
    intersection->m_source.m_bezier->deriv_ipt0():
    intersection->m_source.m_bezier->deriv_ipt1();

  switch(side)
    {
    case fastuidraw::detail::left_boundary:
      return deriv.x()>=0;

    case fastuidraw::detail::right_boundary:
      return deriv.x()<=0;

    case fastuidraw::detail::below_boundary:
      return deriv.y()>=0;

    case fastuidraw::detail::above_boundary:
      return deriv.y()<=0;

    default:
      return true;
    }
}


uint16_t
IndexTextureData::
select_index(int x, int y)
{
  uint16_t pixel(0);
  curve_cache curves;
  fastuidraw::ivec2 texel_bl, texel_tr;
  fastuidraw::detail::analytic_return_type &current(m_intersection_data(x, y));
  int winding_value(m_winding_values(x, y));

  texel_bl=m_outline_data.point_from_bitmap(fastuidraw::ivec2(x,y),
                                            fastuidraw::detail::bitmap_begin);

  texel_tr=m_outline_data.point_from_bitmap(fastuidraw::ivec2(x+1,y+1),
                                            fastuidraw::detail::bitmap_begin);

  /*
    we need to build a list of _all_ curves
    that intersect the texel.
  */
  for(int side=0; side<4; ++side)
    {
      for(std::vector<fastuidraw::detail::simple_line>::const_iterator
            iter=current.m_intersecions[side].begin(), end=current.m_intersecions[side].end();
          iter!=end; ++iter)
        {
          if(intersection_should_be_used(side, &*iter) )
            {
              curves[iter->m_source.m_bezier][side].push_back(&*iter);
            }
        }
    }

  switch(curves.size())
    {
    case 0:
    case 1:
    case 2:
      if(routine_success==sub_select_index(pixel, curves, x, y,
                                           winding_value))
        {
          return pixel;
        }

    default:
        {
          remove_edge_huggers(curves, texel_bl, texel_tr);
          if(routine_success!=sub_select_index(pixel, curves, x, y,
                                               winding_value))
            {
              pixel=sub_select_index_hard_case(curves, x, y, texel_bl, texel_tr);
            }
        }
    }

  return pixel;
}

/////////////////////////////////////////////
// fastuidraw::detail::CurvePairGenerator methods
fastuidraw::detail::CurvePairGenerator::
CurvePairGenerator(FT_Outline outline, ivec2 bitmap_sz, ivec2 bitmap_offset,
                   GlyphRenderDataCurvePair &output)
{
  fastuidraw::reference_counted_ptr<geometry_data_filter> filter;

  filter = FASTUIDRAWnew MakeEvenFilter();
  geometry_data gmt(nullptr, m_pts, filter);

  /*
    usually we set the inflate factor to be 4,
    from that:
     - we want all end points of curves to be even integers
     - some points in the outline from FreeType are given implicitely
    as an _average_ or 2 points
    However, we generate quadratics from cubics which
    generates end points with a divide by _64_. But that
    is an insane inflate factor that will likely cause
    overflows. So, we ignore the unlikely possibility that
    a generated end point from breaking up cubics into
    quadratics would lie on a texel boundary.
    */
  int outline_scale_factor(4), bias(-1);
  CoordinateConverter coordinate_converter(outline_scale_factor, bitmap_sz, bitmap_offset, bias);

  CollapsingContourEmitter *contour_emitter;
  TaggedOutlineData *outline_data;
  float curvature_collapse(0.05f);

  contour_emitter = FASTUIDRAWnew CollapsingContourEmitter(curvature_collapse, outline, coordinate_converter);
  outline_data = FASTUIDRAWnew TaggedOutlineData(contour_emitter, gmt);

  m_contour_emitter = contour_emitter;
  m_outline_data = outline_data;

  if(bitmap_sz.x() != 0 && bitmap_sz.y() != 0)
    {
      output.resize_active_curve_pair(bitmap_sz + ivec2(1, 1));
    }
  else
    {
      output.resize_active_curve_pair(ivec2(0, 0));
    }
}


fastuidraw::detail::CurvePairGenerator::
~CurvePairGenerator()
{
  CollapsingContourEmitter *contour_emitter;
  TaggedOutlineData *outline_data;

  contour_emitter = static_cast<CollapsingContourEmitter*>(m_contour_emitter);
  outline_data = static_cast<TaggedOutlineData*>(m_outline_data);

  FASTUIDRAWdelete(outline_data);
  FASTUIDRAWdelete(contour_emitter);
}

void
fastuidraw::detail::CurvePairGenerator::
extract_path(Path &path) const
{
  TaggedOutlineData *outline_data;
  outline_data = static_cast<TaggedOutlineData*>(m_outline_data);
  outline_data->extract_path(path);
}

void
fastuidraw::detail::CurvePairGenerator::
extract_data(GlyphRenderDataCurvePair &output)
{
  TaggedOutlineData *outline_data;
  outline_data = static_cast<TaggedOutlineData*>(m_outline_data);

  if(!output.active_curve_pair().empty())
    {
      IndexTextureData index_generator(*outline_data, output.resolution(), output.active_curve_pair());
      output.resize_geometry_data(outline_data->number_curves());
      outline_data->fill_geometry_data(output.geometry_data());
      index_generator.fill_index_data();
    }
  else
    {
      output.resize_geometry_data(0);
    }
}
