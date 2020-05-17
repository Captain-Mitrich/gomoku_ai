#ifndef GSTACK_H
#define GSTACK_H

#include "gint.h"

namespace nsg
{

class BaseArray
{
public:
  BaseArray(uint _size = 0) : m_size(_size)
  {}

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

protected:
  uint m_size;
};

template<typename T, uint MAXSIZE>
class TStack : public BaseArray
{
public:
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

  T& pop()
  {
    assert(!empty());
    return m_data[--m_size];
  }

  const T* data()
  {
    return m_data;
  }

protected:
  T m_data[MAXSIZE];
};

} //namespace nsg

#endif // GSTACK_H
