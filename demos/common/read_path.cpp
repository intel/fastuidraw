#include <sstream>
#include <algorithm>
#include <list>

#include "ostream_utility.hpp"
#include "read_path.hpp"

namespace
{
  enum arc_mode_t
    {
      not_arc,
      arc
    };

  enum add_mode_t
    {
      add_mode,
      add_reverse_mode,
      add_no_mode,
    };

  class edge
  {
  public:
    edge(const fastuidraw::vec2 &pt):
      m_pt(pt),
      m_arc_mode(not_arc),
      m_angle(0.0f)
    {}

    /*
      m_pt is the starting point of the edge
      and the rest of the data describe how
      to interpolate -to- the next point.
     */
    fastuidraw::vec2 m_pt;
    std::vector<fastuidraw::vec2> m_control_pts;
    enum arc_mode_t m_arc_mode;
    float m_angle;
  };

  typedef std::vector<edge> outline;
}



void
read_path(fastuidraw::Path &path, const std::string &source)
{
  std::string filtered(source);

  std::replace(filtered.begin(), filtered.end(), '(', ' ');
  std::replace(filtered.begin(), filtered.end(), ')', ' ');
  std::replace(filtered.begin(), filtered.end(), ',', ' ');
  std::istringstream istr(filtered);

  bool adding_control_pts(false);
  std::list<outline> data;
  enum arc_mode_t arc_mode(not_arc);
  enum add_mode_t mode(add_no_mode);
  fastuidraw::vec2 current_value;
  int current_slot(0);

  /*
    this is so hideous.
   */
  while(istr)
    {
      std::string token;
      istr >> token;
      if (!istr.fail())
        {
          if (token == "]")
            {
              if (mode == add_reverse_mode && !data.empty())
                {
                  std::reverse(data.back().begin(), data.back().end());
                }
              mode = add_no_mode;
            }
          else if (token == "[")
            {
              mode = add_mode;
              adding_control_pts = false;
              data.push_back(outline());
            }
          else if (token == "R[")
            {
              mode = add_reverse_mode;
              adding_control_pts = false;
              data.push_back(outline());
            }
          else if (token == "[[")
            {
              adding_control_pts = true;
            }
          else if (token == "]]")
            {
              adding_control_pts = false;
            }
          else if (token == "arc")
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
              if (!token_istr.fail())
                {
                  if (arc_mode != not_arc)
                    {
                      data.back().back().m_angle = number;
                      data.back().back().m_arc_mode = arc_mode;
                      arc_mode = not_arc;
                    }
                  else
                    {
                      current_value[current_slot] = number;
                      if (current_slot == 1)
                        {
                          /* just finished reading a vec2 */
                          current_slot = 0;
                          if (!adding_control_pts)
                            {
                              data.back().push_back(current_value);
                            }
                          else if (!data.empty() && !data.back().empty())
                            {
                              data.back().back().m_control_pts.push_back(current_value);
                            }
                        }
                      else
                        {
                          FASTUIDRAWassert(current_slot == 0);
                          current_slot = 1;
                        }
                    }
                }
            }
        }
    }

  /* now walk the list of outlines.
   */
  for(std::list<outline>::const_iterator iter = data.begin(), end = data.end();
      iter != end; ++iter)
    {
      const outline &current_outline(*iter);

      if (!current_outline.empty())
        {
          path << current_outline[0].m_pt;
          for(unsigned int i = 0; i + 1 < current_outline.size(); ++i)
            {
              const edge &current_edge(current_outline[i]);
              const edge &next_edge(current_outline[i+1]);

              if (current_edge.m_arc_mode == not_arc)
                {
                  for(unsigned int c = 0; c < current_edge.m_control_pts.size(); ++c)
                    {
                      path << fastuidraw::Path::control_point(current_edge.m_control_pts[c]);
                    }
                  path << next_edge.m_pt;
                }
              else
                {
                  path << fastuidraw::Path::arc_degrees(current_edge.m_angle, next_edge.m_pt);
                }
            }

          const edge &current_edge(current_outline.back());
          if (current_edge.m_arc_mode == not_arc)
            {
              for(unsigned int c = 0; c < current_edge.m_control_pts.size(); ++c)
                {
                  path << fastuidraw::Path::control_point(current_edge.m_control_pts[c]);
                }
              path << fastuidraw::Path::contour_end();
            }
          else
            {
              path << fastuidraw::Path::contour_end_arc_degrees(current_edge.m_angle);
            }
        }
    }

}
