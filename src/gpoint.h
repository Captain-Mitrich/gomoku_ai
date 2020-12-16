#ifndef GPOINT_H
#define GPOINT_H

namespace nsg
{

class GPoint
{
public:
  int x, y;

public:
//  GPoint(int _x = 0, int _y = 0) : x(_x), y(_y)
//  {}

//  GPoint(const GPoint& point) = default;

  bool operator==(const GPoint& point) const
  {
    return x == point.x && y == point.y;
  }

  bool operator!=(const GPoint& point) const
  {
    return !operator==(point);
  }

  GPoint operator+(const GPoint& vector) const
  {
    return {x + vector.x, y + vector.y};
  }

  void operator+=(const GPoint& vector)
  {
    x += vector.x;
    y += vector.y;
  }

  GPoint operator-() const
  {
    return {-x, -y};
  }

  GPoint operator-(const GPoint& vector) const
  {
    return {x - vector.x, y - vector.y};
  }

  void operator-=(const GPoint& vector)
  {
    x -= vector.x;
    y -= vector.y;
  }

  GPoint operator*(int k) const
  {
    return {x * k, y * k};
  }

  void operator*=(int k)
  {
    x *= k;
    y *= k;
  }
};

using GVector = GPoint;

} //namespace nsg

#endif
