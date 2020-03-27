#include "grandom.h"
#include "assert.h"
#include <ctime>

namespace nsg
{

std::default_random_engine random_engine((uint)time(0));
static std::uniform_int_distribution dist;

int random(int base, uint count)
{
  assert(count > 0);
  return base + dist(random_engine) % count;
}

} //namespace nsg
