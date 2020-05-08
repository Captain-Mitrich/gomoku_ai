#ifndef GOMOKU_H
#define GOMOKU_H

#include "igomoku.h"
#include "gline.h"
#include "ggrid.h"
#include "gstack.h"
#include "gplayer.h"
#include "gint.h"
#include <iostream>

namespace nsg
{

static const uint DEF_GRID_WIDTH = 15;
static const uint DEF_GRID_HEIGHT = 15;
static const uint DEF_CELL_COUNT = DEF_GRID_WIDTH * DEF_GRID_HEIGHT;

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

template <uint MAXSIZE>
using GStack = TStack<GPoint, MAXSIZE>;

class GStateBackup
{
public:
  GStateBackup() : line5_moves_count(0)
  {}

  void addLine5Move()
  {
    ++line5_moves_count;
  }

  bool emptyLine4Moves()
  {
    return m_line4_moves.empty();
  }

  //ходы линий 4 всегда добавляются парами
  //(два свободных хода линии 3)
  void pushLine4Moves(const GPoint& move1, const GPoint& move2)
  {
    m_line4_moves.push() = move1;
    m_line4_moves.push() = move2;
  }

  void popLine4Moves(const GPoint*& move1, const GPoint*& move2)
  {
    move2 = &m_line4_moves.pop();
    move1 = &m_line4_moves.pop();
  }

public:
  //новые ходы линий 5
  uint line5_moves_count;

  //максимальное число ячеек, которые лежат на восьми линиях 5,
  //расходящихся из общего центра (ход в центр влияет на веса этих ходов)
  static const uint RELATED_MOVES_COUNT = 32;

  //бэкап весов связанных ходов
  GMoveWgt related_moves_wgt_backup[RELATED_MOVES_COUNT];

  //новые линии 3, каждая линия 3 представлена парой свободных ходов,
  //каждый такой ход реализует линию 4
  GStack<RELATED_MOVES_COUNT> m_line4_moves;
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

using GPointStack = TGridStack<bool, TCleaner<bool>, TCleaner<bool>>;

//Каждый ход может реализовать одну линию 4 или вилку из нескольких линий 4
//Для таких ходов хранится список дополняющих ходов до линии 5
using GLine4MoveData = GPointStack;
using GLine4MoveDataPtr = std::unique_ptr<GLine4MoveData>;

using GGrid = TGridStack<GMoveData, TCleanerByMethods<GMoveData>>;

class Gomoku : public IGomoku, protected GGrid
{
public:
  Gomoku(int _width = DEF_GRID_WIDTH, int _height = DEF_GRID_HEIGHT);

  const GGrid& grid() const;

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

  friend class GMoveMaker;

  const GPointStack& getLine5Moves(GPlayer player) const;

  void undoImpl();

  GPoint hintImpl(GPlayer player);
  GPoint hintSecondMove() const;
  bool hintLine5(GPlayer player, GPoint& move) const;
  bool hintLine5Block(GPlayer player, GPoint& move) const;

  //Поиск выигрышной цепочки шахов
  bool hintShortestVictoryMove4Chain(GPlayer player, GPoint& move, GStack<DEF_CELL_COUNT>* defense_variants = 0);
  bool hintVictoryMove4Chain(GPlayer player, GPoint& move, uint max_depth, GStack<DEF_CELL_COUNT>* defense_variants = 0);

  //Лучшая защита от выигрышной цепочки шахов противника
  GPoint hintBestDefense(GPlayer player, const GPoint& enemy_move4, const GStack<DEF_CELL_COUNT>& variants, int depth);

  //Ход с лучшим весом при отсутствии выигрышных цепочек шахов игрока и угроз противника
  GPoint hintMaxWgt(GPlayer player, int depth);

  int calcMaxWgt(GPlayer player, int depth);
  int calcWgt(GPlayer player, const GPoint& move, int depth);

  //Вес лучшей цепочки шахов
  int calcMaxMove4ChainWgt(GPlayer player, uint depth, GStack<DEF_CELL_COUNT>* defense_variants = 0);
  //Вес лучшей цепочки шахов с заданным начальным шахом
  int calcMove4ChainWgt(GPlayer player, const GPoint& move4, uint depth, GStack<DEF_CELL_COUNT>* defense_variants = 0);
  //Вес блокировки шаха противника в цепочке шахов противника
  int calcBlock5Wgt(GPlayer player, const GPoint& block, uint depth, GStack<DEF_CELL_COUNT>* defense_variants = 0);

  //Вес лучшей блокировки выигрышной цепочки шахов
  int calcMaxDefenseWgt(GPlayer player, const GStack<DEF_CELL_COUNT>& variants, int depth);
  //Вес контршаха для защиты от выигрышной цепочки шахов противника
  int calcDefenseMove4Wgt(GPlayer player, const GPoint& move4, int depth);
  //Вес хода для защиты от выигрышной цепочки шахов противника
  int calcDefenseWgt(GPlayer player, const GPoint& move, int depth);

  GPlayer lastMovePlayer() const;
  GPlayer curPlayer() const;

  //На четной глубине ход игрока,
  //за которого в данный момент играет ии
  bool isAiDepth(int depth) const;

  void addLine5Move(GPlayer player, const GPoint& line5Move);
  void removeLine5Move(GPlayer player);
  bool isLine5Move(GPlayer player, const GPoint& move) const;
  void addLine4Moves(GPlayer player, const GPoint& move1, const GPoint& move2, GMoveData& source);
  void undoLine4Moves(GMoveData& source);
  bool isLine4Move(GPlayer player, const GPoint& move) const;

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
  int getFurtherMoveWgt(int line_len);
  int getBlockingMoveWgt(int line_len);
  int getLineWgt(int line_len);

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
  bool findVictoryMove(GPlayer player, GPoint& move, int depth, WgtHandler wgtHandler)
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
  TGridStack<GLine4MoveDataPtr, TPtrCleaner<GLine4MoveDataPtr>> m_line4_moves[2];
};

//raii move maker
class GMoveMaker
{
public:
  GMoveMaker() : m_g(0)
  {}

  GMoveMaker(Gomoku* g, GPlayer player, const GPoint& move) : m_g(0)
  {
    doMove(g, player, move);
  }

  ~GMoveMaker()
  {
    if (m_g)
    {
      m_g->restoreRelatedMovesState();
      m_g->pop();
    }
  }

  void doMove(Gomoku* g, GPlayer player, const GPoint& move)
  {
    assert(!m_g);
    m_g = g;
    g->push(move).player = player;
    g->updateRelatedMovesState();
  }

protected:
  Gomoku* m_g;
};

} //namespace nsg

#endif
