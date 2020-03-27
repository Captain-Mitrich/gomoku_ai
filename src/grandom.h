#ifndef GRANDOM_H
#define GRANDOM_H

#include "gint.h"
#include <random>
#include <algorithm>

namespace nsg
{

extern std::default_random_engine random_engine;

int random(int base, uint count);

template <class Iter>
void shuffle(Iter b, Iter e)
{
  std::shuffle(b, e, random_engine);
}

} //namespace nsg

#endif
