#include <fstream>
#include <fastuidraw/util/fastuidraw_memory.hpp>

#include "ostream_utility.hpp"
#include "colorstop_command_line.hpp"
#include "read_colorstops.hpp"

/////////////////////////////////
// color_stop_arguments methods
color_stop_arguments::
color_stop_arguments(command_line_register &parent):
  command_line_argument(parent)
{
  m_add = produce_formatted_detailed_description("add_stop label place R G B A",
                                                 "where label is the name of the color stop "
                                                 "sequence place is \"time\" of color stop, "
                                                 "R, G, B and A are color value of color stop as"
                                                 "8-bit integers");
  m_add2 = produce_formatted_detailed_description("add_stop_file filename",
                                                  "Creates a color stop from the specified file, "
                                                  "giving it the label as the file name");
  m_disc = produce_formatted_detailed_description("discretization label N",
                                                  "where label is the name of the color stop sequence "
                                                  "and N is the discretization of the color stop "
                                                  "(default value is 16)");
}


color_stop_arguments::
~color_stop_arguments()
{
  for(hoard::iterator iter = m_values.begin(), end = m_values.end(); iter != end; ++iter)
    {
      FASTUIDRAWdelete(iter->second);
    }
}

color_stop_arguments::colorstop_data&
color_stop_arguments::
fetch(const std::string &pname)
{
  hoard::iterator iter;
  iter = m_values.find(pname);
  if(iter != m_values.end())
    {
      return *iter->second;
    }

  colorstop_data *p;
  p = FASTUIDRAWnew colorstop_data();
  m_values[pname] = p;
  return *p;
}

int
color_stop_arguments::
check_arg(const std::vector<std::string> &args, int location)
{
  const std::string &str(args[location]);
  if(static_cast<unsigned int>(location+6) < args.size() && str == "add_stop")
    {
      int R, G, B, A;
      float t;
      fastuidraw::u8vec4 color;
      std::string name;

      name = args[location + 1];
      t = std::atof(args[location + 2].c_str());
      R = std::atoi(args[location + 3].c_str());
      G = std::atoi(args[location + 4].c_str());
      B = std::atoi(args[location + 5].c_str());
      A = std::atoi(args[location + 6].c_str());
      color = fastuidraw::u8vec4(R, G, B, A);
      std::cout << "\n[" << name << "] add color " << std::make_pair(t, fastuidraw::ivec4(color));

      fetch(name).m_stops.add(fastuidraw::ColorStop(color, t));
      return 7;
    }
  else if(static_cast<unsigned int>(location+1) < args.size() && str == "add_stop_file")
    {
      std::string name(args[location + 1]);
      std::ifstream infile(name.c_str());
      if(infile)
        {
          fetch(name).m_stops.clear();
          read_colorstops(fetch(name).m_stops, infile);
          std::cout << "\nAdd colorstop from file " << name;
        }
      return 2;
    }
  else if(static_cast<unsigned int>(location+2) < args.size() && str == "discretization")
    {
      std::string name;
      int N;

      name = args[location + 1];
      N = std::atoi(args[location + 2].c_str());
      std::cout << "\n[" << name << " discretization = " << N;

      fetch(name).m_discretization = N;
      return 3;
    }
  return 0;
}

void
color_stop_arguments::
print_command_line_description(std::ostream &ostr) const
{
  ostr << "[add_stop label place R G B A] [discretization label N]";
}


void
color_stop_arguments::
print_detailed_description(std::ostream &ostr) const
{
  ostr << m_add << "\n" << m_add2 << "\n" << m_disc;
}
