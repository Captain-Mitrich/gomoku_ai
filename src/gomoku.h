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

using GBaseStack = TBaseStack<GPoint>;

template <uint MAXSIZE>
using GStack = TStack<GPoint, MAXSIZE>;

class GStateBackup
{
public:
  GStateBackup() : m_moves5_count(0)
  {}

  void addLine5Move()
  {
    ++m_moves5_count;
  }

  bool emptyLine4Moves()
  {
    return m_moves4.empty();
  }

  //ходы линий 4 всегда добавляются парами
  //(два свободных хода линии 3)
  void pushLine4Moves(const GPoint& move1, const GPoint& move2)
  {
    m_moves4.push() = move1;
    m_moves4.push() = move2;
  }

  void popLine4Moves(const GPoint*& move1, const GPoint*& move2)
  {
    move2 = &m_moves4.pop();
    move1 = &m_moves4.pop();
  }

  void pushOpen3(const GPoint& move)
  {
    m_open3_moves.push() = move;
  }

  void popOpen3(GPoint*& move)
  {
    move = &m_open3_moves.pop();
  }

public:
  //новые ходы линий 5
  uint m_moves5_count;

  //максимальное число ячеек, которые лежат на восьми линиях 5,
  //расходящихся из общего центра (ход в центр влияет на веса этих ходов)
  static const uint RELATED_MOVES_COUNT = 32;

  //бэкап весов связанных ходов
  GMoveWgt related_moves_wgt_backup[RELATED_MOVES_COUNT];

  //новые линии 3, каждая линия 3 представлена парой свободных ходов,
  //каждый такой ход реализует линию 4
  GStack<RELATED_MOVES_COUNT> m_moves4;

  //новые потенциальные открытые тройки
  GStack<RELATED_MOVES_COUNT> m_open3_moves;
};

class GMoveData : public GStateBackup
{
public:
  GMoveData(GPlayer _player = G_EMPTY) : player(_player)
  {}

public:
  GPlayer player;

  //для каждой свободной ячейки хранится вес хода каждого игрока
  //который отражает изменение веса ситуации для игрока при реализации этого хода
  //вес ситуации для игрока равен разнице между весом линий игрока и весом линий противника
  GMoveWgt wgt;
};

template <>
inline bool isEmptyItem(const GMoveData& item, const GMoveData&)
{
  return item.player == G_EMPTY;
}

template <>
inline void clearItem<GMoveData>(GMoveData& item, const GMoveData&)
{
  item.player = G_EMPTY;
}

using GGrid = TGridStack<GMoveData>;

template <>
const bool default_empty_val<bool> = false;

using GPointStack = TGridStack<bool>;

class GDangerMoveData
{
public:
  GDangerMoveData(int width, int height, bool open3 = false) : m_moves5(width, height), m_open3(open3)
  {}

  bool empty()
  {
    return m_moves5.cells().empty() && !m_open3;
  }
public:
  GPointStack m_moves5;  //Для шаха или вилки шахов храним множество финальных дополнений
  bool m_open3;          //Для открытой тройки храним признак открытой тройки
};

using GDangerMoveDataPtr = std::unique_ptr<GDangerMoveData>;

template <class T>
class TEmptyValType<std::unique_ptr<T>>
{
public:
  using type = std::nullptr_t;
};

class GAttackMove : public GPoint
{
public:
  GStack<DEF_CELL_COUNT> defense_variants;
};

class Gomoku : public IGomoku, protected GGrid
{
public:
  Gomoku(int _width = DEF_GRID_WIDTH, int _height = DEF_GRID_HEIGHT);

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

  void undoImpl();

  GPoint hintImpl(GPlayer player);
  GPoint hintSecondMove() const;
  bool hintMove5(GPlayer player, GPoint& move) const;
  bool hintBlock5(GPlayer player, GPoint& move) const;

  //Поиск выигрышной цепочки шахов
  bool hintShortestVictoryMove4Chain(
      GPlayer player,
      GPoint& move,
      uint max_move4_chain_depth,
      GBaseStack* defense_variants = 0);
  bool hintVictoryMove4Chain(
      GPlayer player,
      GPoint& move,
      uint move4_chain_depth,
      GBaseStack* defense_variants = 0);
  //Поиск выигрышной цепочки шахов и полушахов
  bool hintShortestVictoryChain(
    GPlayer player,
    GPoint& move,
    uint max_move4_chain_depth);
  bool hintVictoryChain(
      GPlayer player,
      GPoint& move,
      uint chain_depth);

  //Лучшая защита от выигрышной цепочки шахов противника
  GPoint hintBestDefense(
      GPlayer player,
      const GPoint& enemy_move4,
      const GBaseStack& variants,
      uint depth,
      uint enemy_move4_chain_depth);

  //Ход с лучшим весом при отсутствии выигрышных цепочек шахов игрока и угроз противника
  GPoint hintMaxWgt(GPlayer player, int depth);

  int calcMaxWgt(GPlayer player, int depth, GBaseStack* max_wgt_moves = 0);
  int calcWgt(GPlayer player, const GPoint& move, int depth);

  //Вес лучшей цепочки шахов
  bool findVictoryMove4Chain(GPlayer player, uint depth, GBaseStack* defense_variants = 0);

  //Поиск выигрышной цепочки шахов с заданным начальным шахом
  bool findVictoryMove4Chain(GPlayer player, const GPoint& move4, uint depth, GBaseStack* defense_variants = 0);

  //Вес блокировки шаха противника в цепочке шахов противника
  bool isDefeatBlock5(GPlayer player, const GPoint& block, uint depth, GBaseStack* defense_variants = 0);

  //Является ли ход выигрышным
  bool isVictoryMove(GPlayer player, const GPoint& move, bool forced, uint depth);

  //Блокирует ли ход цепочку шахов и полушахов противника
  bool isDefeatMove(GPlayer player, const GPoint& move, uint depth);

  //Поиск защитной цепочки шахов в ответ на полушах
  bool findDefenseMove4Chain(GPlayer player, uint depth, uint defense_move4_chain_depth);

  //Защищает ли контршах от выигрышной цепочки противника
  bool isDefenseMove4(GPlayer player, const GPoint& move4, uint depth, uint defense_move4_chain_depth);

  bool isVictoryForcedMove(GPlayer player, const GPoint& move, uint depth, uint defense_move4_chain_depth);

  int calcMaxAttackWgt(GPlayer player, uint depth, GBaseStack* max_wgt_moves = 0);

  int calcAttackWgt(GPlayer player, const GAttackMove& move, uint depth);

  std::list<GAttackMove> findAttackMoves(GPlayer player);

  //Вес лучшей блокировки выигрышной цепочки шахов
  int calcMaxDefenseWgt(
    GPlayer player,
    const GBaseStack& variants,
    uint depth,
    uint enemy_move4_chain_depth,
    bool is_player_forced,
    bool is_enemy_forced,
    GBaseStack* max_wgt_moves = 0);

  //Вес контршаха для защиты от выигрышной цепочки шахов противника
  int calcDefenseMove4Wgt(
    GPlayer player,
    const GPoint& move4,
    const GBaseStack& variants,
    uint depth,
    uint enemy_move4_chain_depth,
    bool is_player_forced,
    bool is_enemy_forced);

  //Вес хода для защиты от выигрышной цепочки шахов противника
  int calcDefenseWgt(
    GPlayer player,
    const GPoint& move,
    uint depth,
    uint enemy_move4_chain_depth,
    bool is_player_forced,
    bool is_enemy_forced);

  GPlayer lastMovePlayer() const;
  GPlayer curPlayer() const;

  //На четной глубине ход игрока,
  //за которого в данный момент играет ии
  bool isAiDepth(uint depth) const;

  void addMove5(GPlayer player, const GPoint& move5);
  void removeMove5(GPlayer player);
  bool isMove5(GPlayer player, const GPoint& move) const;
  void addMoves4(GPlayer player, const GPoint& move1, const GPoint& move2, GMoveData& source);
  void undoMoves4(GMoveData& source);
  bool isDangerMove4(GPlayer player, const GPoint& move) const;

  bool buildLine5();
  bool buildLine5(const GVector& v1);
  void undoLine5();

  bool adjustLineStart(GPoint& start, const GVector& v1, int lineLen) const;

  void initMovesWgt();
  void initWgt(const GLine& line5);

  void addWgt(const GPoint& p, GPlayer player, int wgt_delta);

  void updateRelatedMovesState();
  void updateRelatedMovesState(const GVector& v1);
  //void updateMoves4(const GVector& v1);
  void updateOpen3(const GVector& v1);
  void updateOpen3_Xxx(const GPoint& p3, const GVector& v1);
  void updateOpen3_X_xx(const GPoint& p5, const GVector& v1);
  void updateOpen3_xXx(const GPoint& p2, const GVector& v1);
  void updateOpen3_Xx_x(const GPoint& p3, const GVector& v1);
  void addOpen3(const GPoint& move);
  void undoOpen3(GMoveData& moveData);

  void backupRelatedMovesState(const GVector& v1, uint& related_moves_iter);
  void restoreRelatedMovesState();
  void restoreRelatedMovesState(const GVector& v1, uint& related_moves_iter);

  int getFirstLineMoveWgt();
  int getFurtherMoveWgt(int line_len);
  int getBlockingMoveWgt(int line_len);
  int getLineWgt(int line_len);

  bool next(GPoint& point) const;

  GPointStack& getGrid(uint depth)
  {
    assert(depth < MAX_DEPTH);
    return m_grids[depth];
  }

  bool isSpecialWgt(int wgt)
  {
    return wgt == WGT_VICTORY || wgt == WGT_DEFEAT || wgt == WGT_NEAR_VICTORY || wgt == WGT_NEAR_DEFEAT;
  }

  uint getReasonableMove4ChainDepth(uint cur_depth)
  {
    return (getAiLevel() > cur_depth) ? (getAiLevel() - cur_depth) : 0;
  }

  static int updateMaxWgt(const GPoint& move, int wgt, GBaseStack* max_wgt_moves, int& max_wgt);

protected:
  static const GVector vecs1[];  //{1, 0}, {1, 1}, {0, 1}, {-1, 1}

  static const int WGT_VICTORY = std::numeric_limits<int>::max() - 1;
  static const int WGT_DEFEAT  = -WGT_VICTORY;
  static const int WGT_NEAR_VICTORY = WGT_VICTORY - 1;
  static const int WGT_NEAR_DEFEAT = -WGT_NEAR_VICTORY;

  uint m_ai_level;

  GLine m_line5;

  GPointStack m_moves5[2];

  TGridStack<GDangerMoveDataPtr/*, TPtrCleaner<GDangerMoveDataPtr>*/> m_danger_moves[2];

  const static uint MAX_DEPTH = 10;

  //Эти объекты используются на разной глубине рекурсии
  //Их нельзя создавать в стэке, поскольку создание является дорогой операцией
  GPointStack m_grids[MAX_DEPTH];

protected:
  decltype(m_danger_moves[G_BLACK]) dangerMoves(GPlayer player)
  {
    return m_danger_moves[player];
  }
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
