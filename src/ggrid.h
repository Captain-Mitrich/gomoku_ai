#ifndef GGRID_H
#define GGRID_H

#include "gstack.h"
#include "gpoint.h"
#include <list>

namespace nsg
{

//Обычно пустое значение по умолчанию получается с помощью конструктора без параметров
//Если требуется иное, нужна специализация
//Тип специализированного значения может отличаться от Т
template<typename T>
static const T default_empty_value = T();
//Пример специализации пустого значения
//template<class T>
//const std::nullptr_t default_empty_value<std::unique_ptr<T>> = nullptr;

//Тип указателя на член
template <typename T, typename ReturnType, bool Const, typename... Args>
struct MethodPtrTypeMeta
{
  using type = ReturnType (T::*) (Args...);
};

//Специализация для константных членов
template <typename T, typename ReturnType, typename... Args>
struct MethodPtrTypeMeta<T, ReturnType, true, Args...>
{
  using type = ReturnType (T::*) (Args...) const;
};

template <typename T, typename ReturnType, bool Const, typename... Args>
using MethodPtrType = typename MethodPtrTypeMeta<T, ReturnType, Const, Args...>::type;

//Макрос для генерации метафункции проверки наличия в классе заданного метода
//В параметр MetaFunctionName нужно задать желаемое имя метафункции
//Например DECLARE_HAS_METHOD_TYPE_TRATE(has_foo, foo)
//Пример проверки метода bool foo(ArgType1, ArgType2) const
//if constexpr (has_foo<MyType, bool, true, ArgType1, ArgType2>())
//В третьем параметре шаблона указывается, должен ли метод быть константным
//Для скалярных типов функция возвращает false
#define DECLARE_HAS_METHOD(meta_function_name, method_name)\
template <typename T, typename ReturnType, bool Const, typename... Args>\
struct meta_function_name##_s\
{\
  template<class U>\
  static constexpr void detect(...);\
  template<class U>\
  static constexpr decltype(static_cast<MethodPtrType<T, ReturnType, Const, Args...>>(&U::method_name)) detect(bool);\
  static constexpr bool value = !std::is_same_v<decltype(detect<T>(true)), void>;\
};\
template <typename T, typename ReturnType, bool Const, typename... Args>\
constexpr bool meta_function_name()\
{\
  if constexpr (std::is_scalar<T>())\
    return false;\
  else\
    return meta_function_name##_s<T, ReturnType, Const, Args...>::value;\
}

DECLARE_HAS_METHOD(hasEmpty, empty)

//Проверка на пустоту по умолчанию использует метод bool T::empty() const при его наличии,
//иначе сравнивает с пустым значением (см. TDefaultEmptyValue).
//Если требуется иное, нужна специализация
template <typename T>
class TEmptyChecker
{
public:
  static bool empty(const T& item)
  {
    if constexpr (hasEmpty<T, bool, true>())
      return item.empty();
    else
      return item == default_empty_value<T>;
  }
};

DECLARE_HAS_METHOD(hasClear, clear)

//Очистка по умолчанию использует метод void T::clear() при его наличии,
//иначе присваивает пустое значение
//Если требуется иное, нужна специализация
template <typename T>
class TCleaner
{
public:
  static void clear(T& item)
  {
    if constexpr (hasClear<T, void, false>())
      item.clear();
    else
      item = default_empty_value<T>;
  }
};

template <typename T, int W = GRID_WIDTH, int H = GRID_HEIGHT>
class TGridConst
{
public:

  TGridConst()
  {
    GPoint p{0, 0};
    do
    {
      TCleaner<T>::clear(ref(p));
    }
    while (next(p));
  }

  DELETE_COPY(TGridConst)

  static constexpr int width()
  {
    return W;
  }

  static constexpr int height()
  {
    return H;
  }

  static constexpr int gridSize()
  {
    return width() * height();
  }

  bool isEmptyCell(const GPoint& p) const
  {
    return TEmptyChecker<T>::empty(get(p));
  }

  const T& get(const GPoint& p) const
  {
    assert(isValidCell(p));
    return m_data[p.x][p.y];
  }

protected:
  void clearCell(const GPoint& p)
  {
    TCleaner<T>::clear(ref(p));
  }

  T& ref(const GPoint& p)
  {
    assert(isValidCell(p));
    return m_data[p.x][p.y];
  }

  static bool isValidCell(const GPoint& p)
  {
    return p.x >= 0 && p.x < width() && p.y >= 0 && p.y < height();
  }

  static bool next(GPoint& p)
  {
    assert(isValidCell(p));
    if (++p.x == width())
    {
      if (++p.y == height())
        return false;
      p.x = 0;
    }
    return true;
  }
protected:
  T m_data[width()][height()];
};

template <typename T, int W = GRID_WIDTH, int H = GRID_HEIGHT>
class TGridStack : public TGridConst<T, W, H>
{
private:
  using Base = TGridConst<T, W, H>;

protected:
  TStack<GPoint, W * H> m_cells;

public:

  void clear()
  {
    for (auto& cell: m_cells)
      Base::clearCell(cell);
    m_cells.clear();
  }

  const decltype(m_cells)& cells() const
  {
    return m_cells;
  }

  const GPoint& lastCell() const
  {
    return cells().back();
  }

  T& operator[](const GPoint& p)
  {
    T& data = Base::ref(p);
    if (TEmptyChecker<T>::empty(data))
      m_cells.push() = p;
    return data;
  }

  T& push(const GPoint& p)
  {
    m_cells.push() = p;
    return Base::ref(p);
  }

  void pop()
  {
    assert(!m_cells.empty());
    Base::clearCell(lastCell());
    m_cells.pop();
  }
};

template<typename T>
using ListIter = typename std::list<T>::iterator;

//Множество точек с быстрым поиском, добавлением и удалением (хэш таблица)
template<int W = GRID_WIDTH, int H = GRID_HEIGHT>
class TGridSet : public TGridConst<ListIter<GPoint>, W, H>
{
private:
  using Base = TGridConst<ListIter<GPoint>, W, H>;

public:
  const std::list<GPoint>& cells() const
  {
    return m_cells;
  }

  void insert(const GPoint& p)
  {
    auto& iter = Base::ref(p);
    //if (isEmptyItem(iter))
    if (TEmptyChecker<ListIter<GPoint>>::empty(iter))
    {
      iter = m_cells.emplace(m_cells.begin());
      (GPoint&)(*iter) = p;
    }
  }

  void remove(const GPoint& p)
  {
    auto& iter = Base::ref(p);
    //if (isEmptyItem(iter))
    if (TEmptyChecker<ListIter<GPoint>>::empty(iter))
      return;
    m_cells.erase(iter);
    TCleaner<ListIter<GPoint>>::clear(iter);
  }

protected:
  std::list<GPoint> m_cells;
};

} //namespace nsg

#endif
