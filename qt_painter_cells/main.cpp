#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <dirent.h>

#include "qt_demo.hpp"
#include "PainterWidget.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"

#include "cell.hpp"
#include "table.hpp"
#include "random.hpp"

class command_line_list:
  public command_line_argument,
  public std::set<std::string>
{
public:
  command_line_list(const std::string &nm,
                    const std::string &desc,
                    command_line_register &p):
    command_line_argument(p),
    m_name(nm)
  {
    std::ostringstream ostr;
    ostr << "\n\t" << m_name << " value"
         << format_description_string(m_name, desc);
    m_description = tabs_to_spaces(ostr.str());
  }

  virtual
  int
  check_arg(const std::vector<std::string> &argv, int location)
  {
    int argc(argv.size());
    if(location + 1 < argc && argv[location] == m_name)
      {
        insert(argv[location+1]);
        std::cout << "\n\t" << m_name << " \""
                  << argv[location+1] << "\" ";
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[" << m_name << " value] ";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_description;
  }

private:
  std::string m_name, m_description;
};

class painter_cells:public qt_demo
{
public:
  painter_cells(void);

  ~painter_cells();

  virtual
  void
  derived_init(int w, int h);

  virtual
  void
  on_widget_delete(void);

  virtual
  void
  paint(QPainter *p);

  virtual
  void
  handle_event(QEvent *ev);

private:

  static
  void
  generate_random_colors(int count, std::vector<QColor> &out_values);

  static
  void
  dump_file(const std::string &filename, std::vector<std::string> &dest);

  void
  add_images(const std::string &filename, std::vector<named_image> &dest);

  void
  add_single_image(const std::string &filename, std::vector<named_image> &dest);

  void
  update_cts_params(void);

  enum
    {
      shift_key_down,
      ctrl_key_down,
      left_bracket_key_down,
      right_bracket_key_down,

      number_keys_tracked
    };

  command_line_argument_value<float> m_table_width, m_table_height;
  command_line_argument_value<int> m_num_cells_x, m_num_cells_y;
  command_line_argument_value<int> m_cell_group_size;
  command_line_argument_value<int> m_pixel_size;
  command_line_argument_value<int> m_fps_pixel_size;
  command_line_list m_strings;
  command_line_list m_files;
  command_line_list m_images;
  command_line_argument_value<bool> m_draw_image_name;
  command_line_argument_value<int> m_num_background_colors;
  command_line_argument_value<int> m_num_text_colors;
  command_line_argument_value<float> m_min_x_velocity, m_max_x_velocity;
  command_line_argument_value<float> m_min_y_velocity, m_max_y_velocity;
  command_line_argument_value<int> m_min_degree_per_second;
  command_line_argument_value<int> m_max_degree_per_second;
  command_line_argument_value<int> m_table_rotate_degrees_per_s;
  command_line_argument_value<float> m_change_stroke_width_rate;

  command_line_argument_value<int> m_num_frames;
  command_line_argument_value<int> m_skip_frames;
  command_line_argument_value<bool> m_init_show_all_table;
  command_line_argument_value<bool> m_init_table_rotating;
  command_line_argument_value<bool> m_init_table_clipped;
  command_line_argument_value<bool> m_init_cell_rotating;
  command_line_argument_value<bool> m_init_draw_text;
  command_line_argument_value<bool> m_init_draw_images;
  command_line_argument_value<float> m_init_stroke_width;
  command_line_argument_value<bool> m_init_anti_alias_stroking;

  CellSharedState m_cell_shared_state;
  TableParams m_table_params;
  PanZoomTrackerEvent m_zoomer;
  Table *m_table;
  simple_time m_time, m_draw_timer;
  QFont m_font, m_font_fps;

  std::vector<bool> m_key_downs;
  int m_frame;
  uint64_t m_benchmark_time_us;
  simple_time m_benchmark_timer;
  std::vector<uint64_t> m_frame_times;
};

painter_cells::
painter_cells(void):
  m_table_width(800, "table_width", "Table Width", *this),
  m_table_height(600, "table_height", "Table Height", *this),
  m_num_cells_x(10, "num_cells_x", "Number of cells across", *this),
  m_num_cells_y(10, "num_cells_y", "Number of cells down", *this),
  m_cell_group_size(1, "cell_group_size", "width and height in number of cells for cell group size", *this),
  m_pixel_size(24, "font_pixel_size", "Render size for text rendering", *this),
  m_fps_pixel_size(24, "fps_font_pixel_size", "Render size for text rendering of fps", *this),
  m_strings("add_string", "add a string to use by the cells", *this),
  m_files("add_string_file", "add a string to use by a cell, taken from file", *this),
  m_images("add_image", "Add an image to use by the cells", *this),
  m_draw_image_name(false, "draw_image_name", "If true draw the image name in each cell as part of the text", *this),
  m_num_background_colors(1, "num_background_colors", "Number of distinct background colors in cells", *this),
  m_num_text_colors(1, "num_text_colors", "Number of distinct text colors in cells", *this),
  m_min_x_velocity(-10.0f, "min_x_velocity", "Minimum x-velocity for cell content in pixels/s", *this),
  m_max_x_velocity(+10.0f, "max_x_velocity", "Maximum x-velocity for cell content in pixels/s", *this),
  m_min_y_velocity(-10.0f, "min_y_velocity", "Minimum y-velocity for cell content in pixels/s", *this),
  m_max_y_velocity(+10.0f, "max_y_velocity", "Maximum y-velocity for cell content in pixels/s", *this),
  m_min_degree_per_second(60, "min_degree_velocity", "max rotation speed in degrees/second", *this),
  m_max_degree_per_second(60, "max_degree_velocity", "max rotation speed in degrees/second", *this),
  m_table_rotate_degrees_per_s(20, "table_degree_velocity", "rotation speed of table in degrees/second", *this),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_num_frames(-1, "num_frames",
               "If positive, then run demo in benchmark mode terminating after the given number of frames",
               *this),
  m_skip_frames(1, "num_skip_frames",
                "If num_frames > 0, then gives the number of frames to ignore in benchmarking",
                *this),
  m_init_show_all_table(true, "init_show_all_table",
                        "If true, initialize scroll and zoom to show entire table",
                        *this),
  m_init_table_rotating(false, "init_table_rotating",
                        "If true, initialize table to be rotating",
                        *this),
  m_init_table_clipped(false, "init_table_clipped",
                       "If true, initialize to enable clipping on the table",
                       *this),
  m_init_cell_rotating(false, "init_cell_rotating",
                       "If true, intialize to have cells rotating",
                       *this),
  m_init_draw_text(true, "init_draw_text",
                   "If true, intialize to draw text in cells",
                   *this),
  m_init_draw_images(true, "init_draw_image",
                   "If true, intialize to draw image in cells",
                   *this),
  m_init_stroke_width(10.0f, "init_stroke_width",
                      "Initial value for stroking width",
                      *this),
  m_init_anti_alias_stroking(true, "init_antialias_stroking",
                             "Initial value for anti-aliasing for stroking",
                             *this),
  m_table(NULL),
  m_key_downs(number_keys_tracked, false)
{
  std::cout << "Controls:\n"
            << "\t[: decrease stroke width(hold shift for slower rate and ctrl for faster)\n"
            << "\t]: increase stroke width(hold shift for slower rate and ctrl for faster)\n"
            << "\ta: toggle anti-aliasing of stroking\n"
            << "\tp: pause cell rotate\n"
            << "\t0: set zoom factor to 1.0\n"
            << "\tc: toggle clipping of table\n"
            << "\tv: toggle table rotating\n"
            << "\tr: toggle rotating individual cells\n"
            << "\tt: toggle draw cell text\n"
            << "\ti: toggle draw cell image\n"
            << "\tg: toggle using glyph runs to draw text\n"
            << "\tLeft Mouse Drag: pan\n"
            << "\tHold Left Mouse, then drag up/down: zoom out/in\n";
}

painter_cells::
~painter_cells()
{
}

void
painter_cells::
on_widget_delete(void)
{
  /* we need to delete the table before the
     widget is deleted because the cell
     objects have QGylphRun objects to delete
     and their dtors need the QRawFont which
     dies with the QWidget
   */
  if(m_table != NULL)
    {
      delete m_table;
    }
}


void
painter_cells::
generate_random_colors(int count, std::vector<QColor> &out_values)
{
  out_values.resize(count);
  for(int i = 0; i < count; ++i)
    {
      int r, g, b, a;
      r = static_cast<int>(255.0f * random_value(0.0f, 1.0f));
      g = static_cast<int>(255.0f * random_value(0.0f, 1.0f));
      b = static_cast<int>(255.0f * random_value(0.0f, 1.0f));
      a = static_cast<int>(255.0f * random_value(0.2f, 0.8f));

      out_values[i] = QColor(r, g, b, a);
    }
}

void
painter_cells::
dump_file(const std::string &filename, std::vector<std::string> &dest)
{
  std::ifstream istr(filename.c_str());
  if(istr)
    {
      std::ostringstream str;
      str << istr.rdbuf();
      dest.push_back(str.str());
    }
}

void
painter_cells::
add_images(const std::string &filename, std::vector<named_image> &dest)
{
  DIR *dir;
  struct dirent *entry;

  dir = opendir(filename.c_str());
  if(!dir)
    {
      add_single_image(filename, dest);
      return;
    }

  for(entry = readdir(dir); entry != NULL; entry = readdir(dir))
    {
      std::string file;
      file = entry->d_name;
      if(file != ".." && file != ".")
        {
	  add_images(filename + "/" + file, dest);
        }
    }
  closedir(dir);
}

void
painter_cells::
add_single_image(const std::string &filename, std::vector<named_image> &dest)
{
  QImage im;
  im = QImage(filename.c_str());

  if(!im.isNull())
    {
      std::cout << "\tImage \"" << filename << "\" loaded @\n";
      dest.push_back(named_image(im, filename));
    }
}

void
painter_cells::
derived_init(int w, int h)
{
  m_table_params.m_wh = QSizeF(m_table_width.m_value, m_table_height.m_value);
  m_table_params.m_cell_count = QSize(m_num_cells_x.m_value, m_num_cells_y.m_value);
  m_table_params.m_line_color = QColor(255, 255, 255, 255);
  m_table_params.m_cell_state = &m_cell_shared_state;
  m_table_params.m_zoomer = &m_zoomer;
  m_table_params.m_draw_image_name = m_draw_image_name.m_value;
  m_table_params.m_table_rotate_degrees_per_s = m_table_rotate_degrees_per_s.m_value;
  m_table_params.m_timer_based_animation = (m_num_frames.m_value <= 0);
  m_table_params.m_pixel_size = m_pixel_size.m_value;

  m_table_params.m_texts.reserve(m_strings.size() + m_files.size());
  for(command_line_list::iterator iter = m_strings.begin(); iter != m_strings.end(); ++iter)
    {
      m_table_params.m_texts.push_back(*iter);
    }

  for(command_line_list::iterator iter = m_files.begin(); iter != m_files.end(); ++iter)
    {
      dump_file(*iter, m_table_params.m_texts);
    }

  for(command_line_list::iterator iter = m_images.begin(); iter != m_images.end(); ++iter)
    {
      add_images(*iter, m_table_params.m_images);
    }

  generate_random_colors(m_num_background_colors.m_value, m_table_params.m_background_colors);
  generate_random_colors(m_num_text_colors.m_value, m_table_params.m_text_colors);
  m_table_params.m_min_speed = QPointF(m_min_x_velocity.m_value, m_min_y_velocity.m_value);
  m_table_params.m_max_speed = QPointF(m_max_x_velocity.m_value, m_max_y_velocity.m_value);
  m_table_params.m_min_degrees_per_s = m_min_degree_per_second.m_value;
  m_table_params.m_max_degrees_per_s = m_max_degree_per_second.m_value;

  if(m_cell_group_size.m_value > 0)
    {
      m_table_params.m_max_cell_group_size = m_cell_group_size.m_value;
    }
  else
    {
      m_table_params.m_max_cell_group_size = 2 * std::max(m_num_cells_x.m_value, m_num_cells_y.m_value);
    }

  /* select font
   */
  m_font.setFamily("DejaVu Sans");
  m_font.setPixelSize(m_pixel_size.m_value);
  m_font.setStyleName("Book");
  m_cell_shared_state.m_font = m_font;
  m_font_fps = m_font;
  m_font_fps.setPixelSize(m_fps_pixel_size.m_value);

  m_table = new Table(m_table_params);
  m_table->m_clipped = m_init_table_clipped.m_value;
  m_table->m_rotating = m_init_table_rotating.m_value;
  m_cell_shared_state.m_draw_text = m_init_draw_text.m_value;
  m_cell_shared_state.m_draw_image = m_init_draw_images.m_value;
  m_cell_shared_state.m_rotating = m_init_cell_rotating.m_value;
  m_cell_shared_state.m_stroke_width = m_init_stroke_width.m_value;
  m_cell_shared_state.m_anti_alias_stroking = m_init_anti_alias_stroking.m_value;

  /* init m_zoomer so that table contents fit into screen.
   */
  QPointF twh;
  QPointF wwhh(m_table_params.m_wh.width(), m_table_params.m_wh.height());
  ScaleTranslate sc, tr1, tr2;

  twh.rx() = wwhh.x() / qreal(w);
  twh.ry() = wwhh.y() / qreal(h);
  tr1.translation(-0.5 * wwhh);
  tr2.translation(0.5f * QPointF(w, h));

  if(m_init_show_all_table.m_value)
    {
      ScaleTranslate sc;
      sc.scale(1.0f / std::max(twh.x(), twh.y()));
      m_zoomer.transformation(tr2 * sc * tr1);
    }
  else
    {
      m_zoomer.transformation(tr2 * tr1);
    }

  m_frame = -m_skip_frames.m_value;
  if(m_num_frames.m_value > 0)
    {
      m_frame_times.reserve(m_num_frames.m_value);
    }
}

void
painter_cells::
update_cts_params(void)
{
  float speed = static_cast<float>(m_draw_timer.restart()) * 0.001f;

  if(m_key_downs[shift_key_down])
    {
      speed *= 0.1f;
    }
  if(m_key_downs[ctrl_key_down])
    {
      speed *= 10.0f;
    }

  if(m_key_downs[right_bracket_key_down])
    {
      m_cell_shared_state.m_stroke_width += m_change_stroke_width_rate.m_value * speed / m_zoomer.transformation().scale();
    }

  if(m_key_downs[left_bracket_key_down])
    {
      m_cell_shared_state.m_stroke_width -= m_change_stroke_width_rate.m_value  * speed / m_zoomer.transformation().scale();
      m_cell_shared_state.m_stroke_width = std::max(m_cell_shared_state.m_stroke_width, 0.0f);
    }
}

void
painter_cells::
paint(QPainter *painter)
{
  int64_t ms, us;
  us = m_time.restart_us();
  ms = us / 1000;

  if(m_frame == 0)
    {
      m_benchmark_timer.restart();
    }
  else if(m_frame > 0)
    {
      m_frame_times.push_back(us);
    }

  if(m_num_frames.m_value > 0 && m_frame == m_num_frames.m_value)
    {
      m_benchmark_time_us = m_benchmark_timer.elapsed_us();
      std::cout << "Frame times(in us):\n";
      for(unsigned int i = 0, endi = m_frame_times.size(); i < endi; ++i)
        {
          std::cout << m_frame_times[i] << " us\n";
        }
      std::cout << "Did " << m_num_frames.m_value << " frames in "
                << m_benchmark_time_us << "us, average time = "
                << static_cast<float>(m_benchmark_time_us) / static_cast<float>(m_frame)
                << "us\n " << 1000.0f * 1000.0f * static_cast<float>(m_frame) / static_cast<float>(m_benchmark_time_us)
                << " FPS\n";
      end_demo(0);
      return;
    }
  update_cts_params();

  painter->setFont(m_font);
  painter->save();
  painter->translate(m_zoomer.transformation().translation());
  painter->scale(m_zoomer.transformation().scale(), m_zoomer.transformation().scale());
  m_cell_shared_state.m_cells_drawn = 0;
  m_table->m_bb_min = QPointF(0.0, 0.0);
  m_table->m_bb_max = QPointF(dimensions().width(), dimensions().height());
  m_table->paint(painter);
  painter->restore();

  if(m_table_params.m_timer_based_animation)
    {
      std::ostringstream ostr;

      ostr << "FPS = ";
      if(us > 0)
	{
	  ostr << static_cast<int>(1000.0f * 1000.0f / static_cast<float>(us));
	}
      else
	{
	  ostr << "NAN";
	}
      ostr << "\nms = " << ms
           << "\nDrew " << m_cell_shared_state.m_cells_drawn << " cells";
      painter->setPen(QColor(0, 255, 255, 255));
      painter->setFont(m_font_fps);
      painter->drawText(QRectF(QPointF(0.0f, 0.0f), dimensions()),
                        Qt::AlignLeft | Qt::TextDontClip | Qt::TextExpandTabs,
                        QString(ostr.str().c_str()));
    }
  ++m_frame;
}

void
painter_cells::
handle_event(QEvent *ev)
{
  m_zoomer.handle_event(ev);

  switch(ev->type())
    {
    default:
      break;

    case QEvent::KeyRelease:
    case QEvent::KeyPress:
      {
        QKeyEvent *kev(static_cast<QKeyEvent*>(ev));
        switch(kev->key())
          {
          default:
            break;

          case Qt::Key_Escape:
            if(ev->type() == QEvent::KeyRelease)
              {
                end_demo(0);
              }
            break;

          case Qt::Key_A:
            if(ev->type() == QEvent::KeyRelease && m_cell_shared_state.m_stroke_width > 0.0f)
              {
                m_cell_shared_state.m_anti_alias_stroking = !m_cell_shared_state.m_anti_alias_stroking;
                std::cout << "Stroking anti-aliasing = " << m_cell_shared_state.m_anti_alias_stroking << "\n";
              }
            break;

          case Qt::Key_V:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_table->m_rotating = !m_table->m_rotating;
                std::cout << "Table Rotating = " << m_table->m_rotating << "\n";
              }
            break;

          case Qt::Key_C:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_table->m_clipped = !m_table->m_clipped;
                std::cout << "Table clipped = " << m_table->m_clipped << "\n";
              }
            break;

          case Qt::Key_P:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_cell_shared_state.m_pause = !m_cell_shared_state.m_pause;
                std::cout << "Paused = " << m_cell_shared_state.m_pause << "\n";
              }
            break;

          case Qt::Key_R:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_cell_shared_state.m_rotating = !m_cell_shared_state.m_rotating;
                std::cout << "Rotate Cells = " << m_cell_shared_state.m_rotating << "\n";
              }
            break;

          case Qt::Key_T:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_cell_shared_state.m_draw_text = !m_cell_shared_state.m_draw_text;
                std::cout << "Draw Text = " << m_cell_shared_state.m_draw_text << "\n";
              }
            break;

          case Qt::Key_I:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_cell_shared_state.m_draw_image = !m_cell_shared_state.m_draw_image;
                std::cout << "Draw Image = " << m_cell_shared_state.m_draw_image << "\n";
              }
            break;

          case Qt::Key_0:
            if(ev->type() == QEvent::KeyRelease)
              {
                m_zoomer.transformation(ScaleTranslate());
              }
            break;

          case Qt::Key_G:
            if(ev->type() == QEvent::KeyRelease && m_cell_shared_state.m_draw_text)
              {
                m_cell_shared_state.m_use_glyph_run = !m_cell_shared_state.m_use_glyph_run;
                std::cout << "Use Glpyh Run = " << m_cell_shared_state.m_use_glyph_run << "\n";
              }
            break;

          case Qt::Key_Shift:
            if(!kev->isAutoRepeat())
              {
                m_key_downs[shift_key_down] = (ev->type() == QEvent::KeyPress);
              }
            break;

          case Qt::Key_Control:
            if(!kev->isAutoRepeat())
              {
                m_key_downs[ctrl_key_down] = (ev->type() == QEvent::KeyPress);
              }
            break;

          case Qt::Key_BracketLeft:
            if(!kev->isAutoRepeat())
              {
                m_key_downs[left_bracket_key_down] = (ev->type() == QEvent::KeyPress);
              }
            break;

          case Qt::Key_BracketRight:
            if(!kev->isAutoRepeat())
              {
                m_key_downs[right_bracket_key_down] = (ev->type() == QEvent::KeyPress);
              }
            break;
          }
      }
      break;
    }
}

int
main(int argc, char **argv)
{
  painter_cells P;
  return P.main(argc, argv);
}
