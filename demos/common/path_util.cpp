#include <sstream>
#include "path_util.hpp"
#include "ostream_utility.hpp"

void
extract_path_info(const fastuidraw::Path &path,
                  std::vector<fastuidraw::vec2> *out_pts,
                  std::vector<fastuidraw::vec2> *out_ctl_pts,
                  std::vector<fastuidraw::vec2> *out_arc_center_pts,
                  std::string *path_text)
{
  using namespace fastuidraw;

  std::ostringstream str;
  for (unsigned int c = 0, endc = path.number_contours(); c < endc; ++c)
    {
      reference_counted_ptr<const PathContour> C(path.contour(c));

      str << "[ ";
      for (unsigned int e = 0, end_e = C->number_points(); e < end_e; ++e)
        {
          reference_counted_ptr<const PathContour::interpolator_base> E(C->interpolator(e));
          const PathContour::arc *a;
          const PathContour::bezier *b;

          out_pts->push_back(C->point(e));
          str << C->point(e) << " ";

          a = dynamic_cast<const PathContour::arc*>(E.get());
          b = dynamic_cast<const PathContour::bezier*>(E.get());
          if (a)
            {
              range_type<float> angle(a->angle());
              float delta_angle(angle.m_end - angle.m_begin);

              out_arc_center_pts->push_back(a->center());
              str << "arc " << delta_angle * 180.0f / M_PI;
            }
          else if (b)
            {
              c_array<const vec2> pts;

              pts = b->pts().sub_array(1, b->pts().size() - 2);
              str << "[[";
              for (const vec2 &p : pts)
                {
                  out_ctl_pts->push_back(p);
                  str << p << " ";
                }
              str << "]]";
            }
        }
      str << "]\n";
    }
  *path_text = str.str();
}
