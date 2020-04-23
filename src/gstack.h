#ifndef GSTACK_H
#define GSTACK_H

#include "gint.h"

namespace nsg
{

template<typename T, uint MAXSIZE>
class GStack
{
public:
  bool empty() const
  {
    return m_size == 0;
  }

  void clear()
  {
    m_size = 0;
  }

  uint size() const
  {
    return m_size;
  }

  T& operator[](uint i)
  {
    assert(i < m_size);
    return m_data[i];
  }

  const T& operator[](uint i) const
  {
    assert(i < m_size);
    return m_data[i];
  }

  T& back()
  {
    assert(!empty());
    return m_data[m_size - 1];
  }

  T& push()
  {
    assert(m_size < MAXSIZE);
    return m_data[m_size++];
  }

  const T& pop()
  {
    assert(!empty());
    return m_data[--m_size];
  }

protected:
  T m_data[MAXSIZE];

  uint m_size = 0;
};

} //namespace nsg

#endif // GSTACK_H
