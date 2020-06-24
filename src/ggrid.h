#ifndef GGRID_H
#define GGRID_H

#include "gpoint.h"
#include "assert.h"
#include <vector>
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

//Обычно тип пустого значения совпадает с типом непустого значения
//Если требуется иное, нужна специализация
template <typename T>
class TEmptyValType
{
public:
  using type = T;
};

//Обычно дефолтное пустое значение получается с помощью конструктора без параметров
//Если требуется иное, нужна специализация
template <typename T>
static const typename TEmptyValType<T>::type default_empty_val = typename TEmptyValType<T>::type();

//Обычно проверка на пустоту делается путем сравнения с пустым значением
//Если требуется иное, нужна специализация
template <typename T>
bool isEmptyItem(const T& item, const typename TEmptyValType<T>::type& empty_val)
{
  return item == empty_val;
}

template <typename T>
using reftype = typename std::vector<T>::reference;

template <typename T>
using creftype = typename std::vector<T>::const_reference;

//Обычно очистка делается путем присвоения с пустого значения
//Если требуется иное, нужна специализация
template <typename T>
void clearItem(reftype<T> item, const typename TEmptyValType<T>::type& empty_val)
{
  item = empty_val;
}

template <typename T>
class TGridConst : public BaseGrid
{
public:
  using reftype = reftype<T>;
  using creftype = creftype<T>;

  //Инициализация элементов пустыми значениями с помощью конструктора без параметров
  //Если требуется иное, используйте другой конструктор
  TGridConst(int width, int height) :
    BaseGrid(width, height),
    m_empty_val(default_empty_val<T>)
  {
    //VectorInitializer::init(m_data, width * height);
    m_data.resize(width * height);
  }

  TGridConst(int width, int height, const typename TEmptyValType<T>::type& empty_val)
  {
    m_data.resize(width * height, empty_val);
    m_empty_val = empty_val;
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
  bool isEmptyItem(const T& item) const
  {
    return nsg::isEmptyItem(item, m_empty_val);
  }

  void clearItem(reftype item)
  {
    //Cleaner::clear(item);
    nsg::clearItem<T>(item, m_empty_val);
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

  typename TEmptyValType<T>::type m_empty_val;
};

template <typename T>
class TGridStack : public TGridConst<T>
{
private:
  using Base = TGridConst<T>;

public:
  TGridStack(int width, int height) : Base(width, height)
  {
    m_cells.reserve(width * height);
  }

  TGridStack(int width, int height, const typename TEmptyValType<T>::type& empty_val) : Base(width, height, empty_val)
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
