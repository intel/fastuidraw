#include <assert.h>
#include <sstream>
#include <algorithm>
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
    edge(const QPointF &pt):
      m_pt(pt),
      m_arc_mode(not_arc),
      m_angle(0.0f)
    {}

    void
    handle_this_edge(QPainterPath &path, QPointF &next_pt);

    /*
      m_pt is the starting point of the edge
      and the rest of the data describe how
      to interpolate -to- the next point.
     */
    QPointF m_pt;
    std::vector<QPointF> m_control_pts;
    enum arc_mode_t m_arc_mode;
    float m_angle;
  };

  typedef std::vector<edge> outline;

}



void
edge::
handle_this_edge(QPainterPath &path, QPointF &next_pt)
{
  if(m_arc_mode == not_arc)
    {
      /* aww for futz-sake. Qt's arcTo is not
         of the form it takes an end point and
         angle, instead it is take a bounding
         rectangle, start angle and sweep angle.
         WTF?! Rather than pulling my hair out,
         we just to a lineTo.
       */
      path.lineTo(next_pt);
    }
  else
    {
      switch(m_control_pts.size())
        {
        default:
          std::cout << "Warning: more than 2 control points given!\n";
          //fall through
        case 0:
          path.lineTo(next_pt);
          break;
        case 1:
          path.quadTo(m_control_pts[0], next_pt);
          break;
        case 2:
          path.cubicTo(m_control_pts[0], m_control_pts[1], next_pt);
          break;

        }
    }
}

void
read_path(QPainterPath &path, const std::string &source, bool close_contours)
{
  std::string filtered(source);

  std::replace(filtered.begin(), filtered.end(), '(', ' ');
  std::replace(filtered.begin(), filtered.end(), ')', ' ');
  std::replace(filtered.begin(), filtered.end(), ',', ' ');
  std::istringstream istr(filtered);

  bool adding_control_pts(false);
  std::list<outline> data;
  enum arc_mode_t arc_mode(not_arc);
  QPointF current_value;
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
              qreal number;
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
                      if(current_slot == 0)
                        {
                          current_value.rx() = number;
                          current_slot = 1;
                        }
                      else
                        {
                          assert(current_slot == 1);
                          current_value.ry() = number;

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

      if(!current_outline.empty())
        {
          path.moveTo(current_outline[0].m_pt);
          for(unsigned int i = 0; i + 1 < current_outline.size(); ++i)
            {
              current_outline[i].handle_this_edge(path, current_outline[i+1].m_pt);
            }

          if(close_contours)
            {
              current_outline.back().handle_this_edge(path, current_outline.front().m_pt);
            }
        }
    }

}
