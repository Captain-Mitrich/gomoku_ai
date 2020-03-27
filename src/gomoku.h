#ifndef GOMOKU_H
#define GOMOKU_H

#include "igomoku.h"
#include "gline.h"
#include "ggrid.h"
#include "gplayer.h"
#include "gint.h"
#include <list>

namespace nsg
{

class GMoveWgt
{
public:
  int& operator[](GPlayer player)
  {
    assert(player == G_BLACK || player == G_WHITE);
    return wgt[player];
  }

  int operator[](GPlayer player) const
  {
    assert(player == G_BLACK || player == G_WHITE);
    return wgt[player];
  }

protected:
  //вес хода для обоих игроков
  int wgt[2] = {0, 0};
};

class GStateBackup
{
public:
  GStateBackup() : line5_moves_count(0)
  {}

  void addLine5Move(const GPoint& move)
  {
    ++line5_moves_count;
  }

public:
  //новые ходы линий 5
  uint line5_moves_count;

  //максимальное число ячеек, которые лежат на восьми линиях 5,
  //расходящихся из общего центра (ход в центр влияет на веса этих ходов)
  static const uint RELATED_MOVES_COUNT = 32;

  //бэкап весов связанных ходов
  GMoveWgt related_moves_wgt_backup[RELATED_MOVES_COUNT];
};

class GMoveData : public GStateBackup
{
public:
  GMoveData() : player(G_EMPTY)
  {}

  bool empty() const
  {
    return player == G_EMPTY;
  }

  void clear()
  {
    player = G_EMPTY;
  }

public:
  GPlayer player;

  //для каждой свободной ячейки хранится вес хода каждого игрока
  //который отражает изменение веса ситуации для игрока при реализации этого хода
  //вес ситуации для игрока равен разнице между весом линий игрока и весом линий противника
  GMoveWgt wgt;
};

using GGrid = TGridStack<GMoveData, TCleanerByMethods<GMoveData>>;

using GPointStack = TGridStack<bool, TCleaner<bool>, TCleaner<bool>>;

class Gomoku : public IGomoku, public GGrid
{
public:
  Gomoku(int _width = 15, int _height = 15);

  void start() override;
  bool isValidNextMove(int x, int y) const override;
  bool doMove(int x, int y) override;
  bool doMove(int x, int y, GPlayer player);
  bool doMove(const GPoint& move);
  bool doMove(const GPoint& move, GPlayer player);
  bool undo(int& x, int& y) override;
  bool undo(GPoint& move);
  bool undo();
  bool hint(int& x, int& y) override;
  bool hint(int& x, int& y, GPlayer player);
  bool isGameOver() const override;
  const GLine* getLine5() const override;
  uint getAiLevel() const override;
  void setAiLevel(uint level) override;

protected:

  const GPointStack& getLine5Moves(GPlayer player) const;

  void undoImpl();

  GPoint hintImpl(GPlayer player);
  GPoint hintSecondMove() const;
  bool hintLine5(GPlayer player, GPoint& point) const;
  bool hintLine5Block(GPlayer player, GPoint& point) const;
  GPoint hintMaxWgt(GPlayer player, uint depth);

  //если уже найден выигрышный ход, то ищем выигрышный ход,
  //который быстрее всего приводит к победе
  //void hintBestVictoryMove(GPlayer player, GPoint& vmove);

  int calcMaxWgt(uint depth);
  int calcWgt(GPlayer player, const GPoint& move, uint depth);

  GPlayer lastMovePlayer() const;
  GPlayer curPlayer() const;

  void addLine5Move(GPlayer player, const GPoint& line5Move);
  void removeLine5Move(GPlayer player);
  bool isLine5Move(GPlayer player, const GPoint& move) const;
  bool buildLine5();
  bool buildLine5(const GVector& v1);
  void undoLine5();

  bool adjustLineStart(GPoint& start, const GVector& v1, int lineLen) const;

  void initMovesWgt();
  void initWgt(const GLine& line5);

  void addWgt(const GPoint& p, GPlayer player, int wgt_delta);

  void updateRelatedMovesState();
  void updateRelatedMovesState(const GVector& v1);
  void backupRelatedMovesState(const GVector& v1, uint& related_moves_iter);
  void restoreRelatedMovesState();
  void restoreRelatedMovesState(const GVector& v1, uint& related_moves_iter);

  int getFirstLineMoveWgt();
  int getFurtherMoveWgt(uint line_len);
  int getBlockingMoveWgt(uint line_len);
  int getLineWgt(uint line_len);

  template<typename PointHandler>
  bool findMove(GPoint& move, PointHandler pointHandler)
  {
    move.x = move.y = 0;
    do
    {
      if (pointHandler(move))
        return true;
    }
    while(next(move));

    return false;
  }

  template<typename WgtHandler>
  bool findVictoryMove(GPlayer player, GPoint& move, uint depth, WgtHandler wgtHandler)
  {
    int wgt;
    auto moveHandler = [&](const GPoint& move)
    {
      if (!isEmptyCell(move))
        return false;

      wgt = calcWgt(player, move, depth);

      wgtHandler(move, wgt);

      return wgt == WGT_VICTORY;
    };

    return findMove(move, moveHandler);
  }

  bool next(GPoint& point) const;

protected:
  static const GVector vecs1[];  //{1, 0}, {1, 1}, {0, 1}, {-1, 1}

  static const int WGT_VICTORY = std::numeric_limits<int>::max();
  static const int WGT_DEFEAT  = std::numeric_limits<int>::min();

  uint m_ai_level;

  GLine m_line5;

  GPointStack m_line5_moves[2];
};

} //namespace nsg

#endif
