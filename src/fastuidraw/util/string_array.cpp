#include <string>
#include <vector>
#include <fastuidraw/util/string_array.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include "../private/util_private.hpp"

namespace
{
  class StringArrayPrivate
  {
  public:
    StringArrayPrivate(void):
      m_c_strings_ready(true)
    {}

    StringArrayPrivate(const StringArrayPrivate &obj):
      m_strings(obj.m_strings),
      m_c_strings_ready(obj.m_strings.empty())
    {}
    
    void
    ready_c_strings(void)
    {
      if (!m_c_strings_ready)
        {
          m_c_strings.resize(m_strings.size());
          for (unsigned int i = 0; i < m_strings.size(); ++i)
            {
              m_c_strings[i] = m_strings[i].c_str();
            }
          m_c_strings_ready = true;
        }
    }

    std::vector<std::string> m_strings;
    std::vector<fastuidraw::c_string> m_c_strings;
    bool m_c_strings_ready;
  };
}

fastuidraw::string_array::
string_array(void)
{
  m_d = FASTUIDRAWnew StringArrayPrivate();
}

fastuidraw::string_array::
~string_array()
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::string_array::
string_array(const string_array &obj)
{
  StringArrayPrivate *obj_d;

  obj_d = static_cast<StringArrayPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew StringArrayPrivate(*obj_d);
}

assign_swap_implement(fastuidraw::string_array);

unsigned int
fastuidraw::string_array::
size(void) const
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  return d->m_strings.size();
}

void
fastuidraw::string_array::
resize(unsigned int size, c_string value)
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  value = (value) ? value : "";
  d->m_strings.resize(size, value);
  d->m_c_strings_ready = false;
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::string_array::
get(void) const
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  d->ready_c_strings();
  return make_c_array(d->m_c_strings);
}

fastuidraw::c_string
fastuidraw::string_array::
get(unsigned int I) const
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  FASTUIDRAWassert(I < d->m_strings.size());
  return d->m_strings[I].c_str();
}

fastuidraw::string_array&
fastuidraw::string_array::
set(unsigned int I, c_string value)
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  FASTUIDRAWassert(I < d->m_strings.size());
  value = (value) ? value : "";
  d->m_strings[I] = value;
  d->m_c_strings_ready = false;
  return *this;
}

fastuidraw::string_array&
fastuidraw::string_array::
push_back(c_string value)
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  value = (value) ? value : "";
  d->m_strings.push_back(value);
  d->m_c_strings_ready = false;
  return *this;
}

fastuidraw::string_array&
fastuidraw::string_array::
pop_back(void)
{
  StringArrayPrivate *d;
  d = static_cast<StringArrayPrivate*>(m_d);
  d->m_strings.pop_back();
  d->m_c_strings_ready = false;
  return *this;
}
