#ifndef IGOMOKU_H
#define IGOMOKU_H

#include <memory>

namespace nsg
{

const int G_FIELD_SIZE = 15;
const int G_CELL_COUNT = G_FIELD_SIZE * G_FIELD_SIZE;

class GGame;

class GLine;
void getStartPoint(const GLine& line, int& x, int& y);
void getNextPoint(const GLine& line, int& x, int& y);

class IGomoku
{
public:
  virtual ~IGomoku() = default;

  virtual void start() = 0;
  virtual bool isValidNextMove(int x, int y) const = 0;
  virtual bool doMove(int x, int y) = 0;
  virtual bool undo(int& x, int& y) = 0;
  virtual bool hint(int& x, int& y) = 0;
  virtual bool isGameOver() const = 0;
  virtual const GLine* getLine5() const = 0;
  virtual unsigned getAiLevel() const = 0;
  virtual void setAiLevel(unsigned level) = 0;

  static unsigned getMaxAiLevel()
  {
    return 4;
  }
};

using GomokuPtr = std::unique_ptr<IGomoku>;

GomokuPtr CreateGomoku();

} //namespace nsg

#endif // IGOMOKU_H
