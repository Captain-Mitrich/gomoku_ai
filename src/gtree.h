#ifndef GTREE_H
#define GTREE_H

#include <list>

namespace nsg
{

template<class T>
class GEdge;

template <class T>
class GVertex : public std::list<GEdge<T>>
{};

template <class T>
using GTree = GVertex<T>;

template <class T>
class GEdge : public T, protected GVertex<T>
{
public:
  GVertex<T>& vertex()
  {
    return *this;
  }
};

}

#endif
