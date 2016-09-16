#include <assert.h>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>

#include "read_path.hpp"

namespace
{
  enum arc_mode_t
    {
      not_arc,
      arc
    };

  class edge
  {
  public:
    edge(const vec2 &pt):
      m_pt(pt),
      m_arc_mode(not_arc),
      m_angle(0.0f)
    {}

    void
    add_to_path(cairo_t *cr, const vec2 &end_pt) const;
    /*
      m_pt is the starting point of the edge
      and the rest of the data describe how
      to interpolate -to- the next point.
     */
    vec2 m_pt;
    std::vector<vec2> m_control_pts;
    enum arc_mode_t m_arc_mode;
    float m_angle;
  };

  typedef std::vector<edge> outline;
}

void
edge::
add_to_path(cairo_t *cr, const vec2 &end_pt) const
{
  if(m_arc_mode == arc)
    {
      cairo_arc_degrees_to(cr, m_angle, end_pt.x(), end_pt.y());
    }
  else
    {
      switch(m_control_pts.size())
        {
        default:
        case 0:
          cairo_line_to(cr, end_pt.x(), end_pt.y());
          break;

        case 1:
          cairo_bezier_to(cr,
                          m_control_pts[0].x(), m_control_pts[0].y(),
                          end_pt.x(), end_pt.y());
          break;

        case 2:
          cairo_curve_to(cr,
                         m_control_pts[0].x(), m_control_pts[0].y(),
                         m_control_pts[1].x(), m_control_pts[1].y(),
                         end_pt.x(), end_pt.y());
          break;
        }
    }
}

cairo_path_t*
read_path(cairo_t *cr, const std::string &source,
          vec2 &bounding_box_min, vec2 &bounding_box_max)
{
  std::string filtered(source);

  std::replace(filtered.begin(), filtered.end(), '(', ' ');
  std::replace(filtered.begin(), filtered.end(), ')', ' ');
  std::replace(filtered.begin(), filtered.end(), ',', ' ');
  std::istringstream istr(filtered);

  bool adding_control_pts(false);
  std::list<outline> data;
  enum arc_mode_t arc_mode(not_arc);
  vec2 current_value;
  int current_slot(0);

  /*
    this is so hideous.
   */
  while(istr)
    {
      std::string token;
      istr >> token;
      if(!istr.fail())
        {
          if(token == "]")
            {
              /* we actually do not care, the marker "["
                 is what starts a path, and thus what implicitely
                 ends it.
               */
            }
          else if(token == "[")
            {
              adding_control_pts = false;
              data.push_back(outline());
            }
          else if(token == "[[")
            {
              adding_control_pts = true;
            }
          else if(token == "]]")
            {
              adding_control_pts = false;
            }
          else if(token == "arc")
            {
              arc_mode = arc;
            }
          else
            {
              /* not a funky token so should be a number.
               */
              float number;
              std::istringstream token_istr(token);
              token_istr >> number;
              if(!token_istr.fail())
                {
                  if(arc_mode != not_arc)
                    {
                      data.back().back().m_angle = number;
                      data.back().back().m_arc_mode = arc_mode;
                      arc_mode = not_arc;
                    }
                  else
                    {
                      current_value[current_slot] = number;
                      if(current_slot == 1)
                        {
                          /* just finished reading a vec2 */
                          current_slot = 0;
                          if(!adding_control_pts)
                            {
                              data.back().push_back(current_value);
                            }
                          else if(!data.empty() && !data.back().empty())
                            {
                              data.back().back().m_control_pts.push_back(current_value);
                            }
                        }
                      else
                        {
                          assert(current_slot == 0);
                          current_slot = 1;
                        }
                    }
                }
            }
        }
    }

  /* now walk the list of outlines.
   */
  cairo_new_path(cr);
  for(std::list<outline>::const_iterator iter = data.begin(), end = data.end();
      iter != end; ++iter)
    {
      const outline &current_outline(*iter);
      if(!current_outline.empty())
        {
          cairo_move_to(cr, current_outline[0].m_pt.x(), current_outline[0].m_pt.y());
          for(unsigned int i = 0; i + 1 < current_outline.size(); ++i)
            {
              current_outline[i].add_to_path(cr, current_outline[i+1].m_pt);
            }
          current_outline.back().add_to_path(cr, current_outline.front().m_pt);
          cairo_close_path(cr);
        }
    }
  cairo_path_t *return_value;
  return_value = cairo_copy_path(cr);
  cairo_set_line_width(cr, 4.0);
  cairo_stroke_extents(cr,
                       &bounding_box_min.x(), &bounding_box_min.y(),
                       &bounding_box_max.x(), &bounding_box_max.y());

  cairo_new_path(cr);
  return return_value;
}
