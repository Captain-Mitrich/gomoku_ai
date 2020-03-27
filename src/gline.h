#ifndef GLINE_H
#define GLINE_H

#include "gpoint.h"

namespace nsg
{

class GLine
{
public:
  GPoint start;
  GVector v1;

  GLine(const GPoint& _start, const GVector& _v1) :
    start(_start), v1(_v1)
  {}
};

} //namespace nsg

#endif
