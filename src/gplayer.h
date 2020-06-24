#ifndef GPLAYER_H
#define GPLAYER_H

#include "assert.h"

namespace nsg
{

enum GPlayer
{
  G_BLACK = 0,
  G_WHITE = 1,
  G_EMPTY = -1
};

inline GPlayer operator!(GPlayer player)
{
  assert(player == G_BLACK || player == G_WHITE);
  return (player == G_BLACK) ? G_WHITE : G_BLACK;
}

} //namespace nsg

#endif
