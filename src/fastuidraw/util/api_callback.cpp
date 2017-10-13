#include <list>
#include <mutex>
#include <functional>
#include <fastuidraw/util/api_callback.hpp>

namespace
{
  class APICallbackSetPrivate;
  class CallBackPrivate;
  typedef std::list<fastuidraw::APICallbackSet::CallBack*> CallBackList;

  class APICallbackSetPrivate:public fastuidraw::noncopyable
  {
  public:
    APICallbackSetPrivate(void);

    CallBackList::iterator
    insert(fastuidraw::APICallbackSet::CallBack *q);

    void
    erase(CallBackList::iterator iter);

    template<typename F>
    void
    call_callbacks(const F &fptr);

    void
    get_proc_function(void* (*get_proc)(fastuidraw::c_string))
    {
      m_mutex.lock();
      m_get_proc = get_proc;
      m_mutex.unlock();
    }

    void*
    get_proc(fastuidraw::c_string function_name)
    {
      void *return_value;
      m_mutex.lock();
      return_value = m_get_proc ?
        m_get_proc(function_name) :
        nullptr;
      m_mutex.unlock();
      return return_value;
    }

  private:
    std::recursive_mutex m_mutex;
    bool m_in_callback_sequence;

    CallBackList m_list;
    void* (*m_get_proc)(fastuidraw::c_string);
  };

  class CallBackPrivate:public fastuidraw::noncopyable
  {
  public:
    explicit
    CallBackPrivate(APICallbackSetPrivate *s):
      m_parent(s),
      m_active(true)
    {
      FASTUIDRAWassert(m_parent);
    }

    CallBackList::iterator m_location;
    APICallbackSetPrivate *m_parent;
    bool m_active;
  };
}

///////////////////////////////////////
// APICallbackSetPrivate methods
APICallbackSetPrivate::
APICallbackSetPrivate(void):
  m_in_callback_sequence(false),
  m_get_proc(nullptr)
{}

CallBackList::iterator
APICallbackSetPrivate::
insert(fastuidraw::APICallbackSet::CallBack *q)
{
  CallBackList::iterator return_value;
  m_mutex.lock();
  return_value = m_list.insert(m_list.end(), q);
  m_mutex.unlock();
  return return_value;
}

void
APICallbackSetPrivate::
erase(CallBackList::iterator iter)
{
  m_mutex.lock();
  m_list.erase(iter);
  m_mutex.unlock();
}

template<typename F>
void
APICallbackSetPrivate::
call_callbacks(const F &fptr)
{
  m_mutex.lock();
  if (!m_in_callback_sequence)
    {
      m_in_callback_sequence = true;
      for(auto iter = m_list.begin(); iter != m_list.end(); ++iter)
        {
          fptr(*iter);
        }
      m_in_callback_sequence = false;
    }
  m_mutex.unlock();
}

/////////////////////////////////////////////////
// fastuidraw::APICallbackSet::CallBack methods
fastuidraw::APICallbackSet::CallBack::
CallBack(APICallbackSet *parent)
{
  CallBackPrivate *d;
  APICallbackSetPrivate *dp;
  dp = static_cast<APICallbackSetPrivate*>(parent->m_d);

  FASTUIDRAWassert(dp);
  d = FASTUIDRAWnew CallBackPrivate(dp);
  d->m_location = dp->insert(this);

  m_d = d;
}

fastuidraw::APICallbackSet::CallBack::
~CallBack()
{
  CallBackPrivate *d;

  d = static_cast<CallBackPrivate*>(m_d);
  if (d->m_active)
    {
      d->m_parent->erase(d->m_location);
    }
  FASTUIDRAWdelete(d);
}

bool
fastuidraw::APICallbackSet::CallBack::
active(void) const
{
  CallBackPrivate *d;
  d = static_cast<CallBackPrivate*>(m_d);
  return d->m_active;
}

void
fastuidraw::APICallbackSet::CallBack::
active(bool b)
{
  CallBackPrivate *d;
  d = static_cast<CallBackPrivate*>(m_d);

  if(d->m_active == b)
    {
      return;
    }

  d->m_active = b;
  if(b)
    {
      d->m_location = d->m_parent->insert(this);
    }
  else
    {
      d->m_parent->erase(d->m_location);
    }
}

//////////////////////////////////////////////
// fastuidraw::APICallbackSet methods
fastuidraw::APICallbackSet::
APICallbackSet(void)
{
  m_d = FASTUIDRAWnew APICallbackSetPrivate();
}

fastuidraw::APICallbackSet::
~APICallbackSet()
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::APICallbackSet::
get_proc_function(void* (*get_proc)(c_string))
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->get_proc_function(get_proc);
}

void*
fastuidraw::APICallbackSet::
get_proc(c_string function_name)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  return d->get_proc(function_name);
}

void
fastuidraw::APICallbackSet::
call_unloadable_function(c_string function_name)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::on_call_unloadable_function,
                              std::placeholders::_1, function_name));
}

void
fastuidraw::APICallbackSet::
pre_call(c_string call_string_values,
         c_string call_string_src,
         c_string function_name,
         void *function_ptr,
         c_string src_file, int src_line)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::pre_call,
                              std::placeholders::_1,
                              call_string_values,
                              call_string_src,
                              function_name,
                              function_ptr,
                              src_file, src_line));
}

void
fastuidraw::APICallbackSet::
post_call(c_string call_string_values,
          c_string call_string_src,
          c_string function_name,
          c_string error_string,
          void *function_ptr,
          c_string src_file, int src_line)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::post_call,
                              std::placeholders::_1,
                              call_string_values,
                              call_string_src,
                              function_name,
                              error_string,
                              function_ptr,
                              src_file, src_line));
}
