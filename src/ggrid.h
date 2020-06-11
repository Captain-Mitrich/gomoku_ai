#ifndef GGRID_H
#define GGRID_H

#include "gpoint.h"
#include "assert.h"
#include <vector>
#include <forward_list>
#include <list>

namespace nsg
{

class BaseGrid
{
public:
  BaseGrid(int width = 0, int height = 0) : m_width(width), m_height(height)
  {
    assert(width > 0 && height > 0);
  }

  int width() const
  {
    return m_width;
  }

  int height() const
  {
    return m_height;
  }

  int index(const GPoint& p) const
  {
    return p.y * m_height + p.x;
  }

  static bool isValidCell(const GPoint& p, int width, int height)
  {
    return p.x >= 0 && p.x < width && p.y >= 0 && p.y < height;
  }

  bool isValidCell(const GPoint& p) const
  {
    return isValidCell(p, m_width, m_height);
  }

protected:
  int m_width;
  int m_height;
};

template<typename T>
class TVectorInitializer
{
public:
  static void init(std::vector<T>& vec, size_t size)
  {
    vec.resize(size);
  }
};

template<typename T, typename TEmpty = T>
class TCleaner
{
public:
  static void init(std::vector<T>& vec, size_t size)
  {
    vec.resize(size, empty_val);
  }

  static bool empty(const T& item)
  {
    return item == empty_val;
  }

  static void clear(typename std::vector<T>::reference item)
  {
    item = empty_val;
  }

protected:
  static const TEmpty empty_val;
};

template<typename T, typename TEmpty>
const TEmpty TCleaner<T, TEmpty>::empty_val = TEmpty();

template<typename TPtr>
using TPtrCleaner = TCleaner<TPtr, std::nullptr_t>;

template<typename T>
class TCleanerByMethods
{
public:
  static bool empty(const T& item)
  {
    return item.empty();
  }

  static void clear(T& item)
  {
    item.clear();
  }
};

template <typename T, class Cleaner = TCleaner<T>, class VectorInitializer = TVectorInitializer<T>>
class TGridConst : public BaseGrid
{
public:
  using reftype = typename std::vector<T>::reference;
  using creftype = typename std::vector<T>::const_reference;

  TGridConst(int width, int height) :
    BaseGrid(width, height)
  {
    VectorInitializer::init(m_data, width * height);
  }

  static bool isEmptyItem(const T& item)
  {
    return Cleaner::empty(item);
  }

  bool isEmptyCell(const GPoint& p) const
  {
    return isEmptyItem(get(p));
  }

  creftype get(const GPoint& p) const
  {
    assert(isValidCell(p));
    return m_data[this->index(p)];
  }

protected:
  static void clearItem(reftype item)
  {
    Cleaner::clear(item);
  }

  void clearCell(const GPoint& p)
  {
    clearItem(ref(p));
  }

  reftype ref(const GPoint& p)
  {
    assert(isValidCell(p));
    return m_data[index(p)];
  }

protected:
  std::vector<T> m_data;
};

template <typename T, class Cleaner = TCleaner<T>, class VectorInitializer = TVectorInitializer<T>>
class TGridStack : public TGridConst<T, Cleaner, VectorInitializer>
{
private:
  using Base = TGridConst<T, Cleaner, VectorInitializer>;

public:
  TGridStack(int width, int height) : Base(width, height)
  {
    m_cells.reserve(width * height);
  }

  void clear()
  {
    for (auto& cell: m_cells)
      Base::clearCell(cell);
    m_cells.clear();
  }

  const std::vector<GPoint>& cells() const
  {
    return m_cells;
  }

  const GPoint& lastCell() const
  {
    return cells().back();
  }

  typename Base::reftype operator[](const GPoint& p)
  {
    typename Base::reftype data = Base::ref(p);
    if (Base::isEmptyItem(data))
      m_cells.push_back(p);
    return data;
  }

  typename Base::reftype push(const GPoint& p)
  {
    m_cells.push_back(p);
    return Base::ref(p);
  }

  void pop()
  {
    assert(!m_cells.empty());
    Base::clearCell(lastCell());
    m_cells.pop_back();
  }

protected:
  std::vector<GPoint> m_cells;
};

template<typename T>
using ListIter = typename std::list<T>::iterator;

//Множество точек с быстрым поиском, добавлением и удалением (хэш таблица)
class TGridSet : public TGridConst<ListIter<GPoint>>
{
public:
  TGridSet(int width, int height) : Base(width, height)
  {}

  const std::list<GPoint>& cells() const
  {
    return m_cells;
  }

  void insert(const GPoint& p)
  {
    auto& iter = Base::ref(p);
    if (this->isEmptyItem(iter))
    {
      iter = m_cells.emplace(m_cells.begin());
      (GPoint&)(*iter) = p;
    }
  }

  void remove(const GPoint& p)
  {
    auto& iter = Base::ref(p);
    if (this->isEmptyItem(iter))
      return;
    m_cells.erase(iter);
    this->clearItem(iter);
  }

protected:
  std::list<GPoint> m_cells;

private:
  using Base = TGridConst<ListIter<GPoint>>;
};

} //namespace nsg

#endif
