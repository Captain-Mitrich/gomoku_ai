#ifndef GSTACK_H
#define GSTACK_H

#include "gint.h"
#include "gdefs.h"
#include <cassert>
#include <type_traits>
#include <cstdint>
#include <forward_list>

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

  uint size() const
  {
    return m_size;
  }

protected:
  uint m_size;
};

template<typename T>
class TBaseStack : public BaseArray
{
public:
  TBaseStack(T* data) : m_data(data)
  {
    assert(data);
  }

  DELETE_COPY(TBaseStack)

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

  const T& back() const
  {
    assert(!empty());
    return m_data[m_size - 1];
  }

  T& push()
  {
    T* item = new (m_data + m_size++) T;
    return *item;
  }

  void pop()
  {
    assert(!empty());
    m_data[--m_size].~T();
  }

  void clear()
  {
    if constexpr (std::is_trivially_destructible_v<T>)
      m_size = 0;
    else
    {
      while (m_size > 0)
        pop();
    }
  }

  const T* begin() const
  {
    return m_data;
  }

  T* begin()
  {
    return m_data;
  }

  const T* end() const
  {
    return m_data + m_size;
  }

  T* end()
  {
    return m_data + m_size;
  }

protected:
  T* m_data;
};

template<typename T, uint MAXSIZE>
class TStack : public TBaseStack<T>
{
public:
  TStack() : TBaseStack<T>((T*)m_array)
  {}

protected:
  //нельзя допустить конструирования всех элементов резерва,
  //поэтому резерв объявляем как массив байтов
  std::uint8_t m_array[MAXSIZE * sizeof(T)];
};

template<typename T>
class TStackAllocator : public std::allocator<T>
{
};

template<typename T>
class TGomokuStack : public std::forward_list<T, TStackAllocator<T>>
{
};

} //namespace nsg

#endif // GSTACK_H
