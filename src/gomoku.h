#ifndef GOMOKU_H
#define GOMOKU_H

#include "igomoku.h"
#include "ggrid.h"
#include "gstack.h"
#include "gtree.h"
#include "gline.h"
#include "gplayer.h"
#include "gint.h"
#include <iostream>

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

  void popLine4Moves(GPoint& move1, GPoint& move2)
  {
    move2 = m_moves4.back();
    m_moves4.pop();
    move1 = m_moves4.back();
    m_moves4.pop();
  }

  void pushOpen3(const GPoint& move)
  {
    m_open3_moves.push() = move;
  }

  void popOpen3(GPoint& move)
  {
    move = m_open3_moves.back();
    m_open3_moves.pop();
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

template<int W, int H>
using GGrid = TGridStack<GMoveData, W, H>;

template<>
const bool default_empty_value<bool> = false;

using GPointStack = TGridStack<bool>;

class GDangerMoveData
{
public:
  GDangerMoveData(bool open3 = false) : m_open3(open3)
  {}

  bool empty() const
  {
    return m_moves5.empty() && !m_open3;
  }

  void clear()
  {
    m_moves5.clear();
    m_open3 = false;
  }

public:
  GStack<8> m_moves5; //Для шаха или вилки шахов храним множество финальных дополнений
  bool      m_open3;  //Для открытой тройки храним признак открытой тройки
};

struct GMove4Pair
{
  GPoint move4;
  GPoint block;
};

using GMove4Tree = GTree<GMove4Pair>;
using GMove4Edge = GEdge<GMove4Pair>;

class Gomoku : public IGomoku, protected GGrid<GRID_WIDTH, GRID_HEIGHT>
{
public:
  Gomoku();

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

  GPoint hintMaxWgt(GPlayer player);

  int calcMaxWgt(GPlayer player, uint depth, GBaseStack* max_wgt_moves = 0);

  int calcWgt(GPlayer player, const GPoint& move, uint depth, bool forced);

  //Поиск выигрышной цепочки шахов
  bool findVictoryMove4Chain(
    GPlayer player,
    GBaseStack* defense_variants = 0,
    GPoint* victory_move = 0);

  //Поиск выигрышной цепочки шахов
  //для дочерних вариантов заданного непустого узла
  bool processChildMoves4(
    GMove4Tree& tree,
    GPlayer player,
    GBaseStack* defense_variants = 0,
    GPoint* victory_move = 0);

  //Поиск выигрышной цепочки шахов для дочернего варианта
  bool findMove4Chain(
    GMove4Edge& edge,
    GPlayer player,
    GBaseStack* defense_variants = 0);

  //Поиск выигрышого шаха (вилки 4х4)
  //среди вариантов, рассматриваемых на следующем уровне глубины
  bool addChildMoves4(
    GMove4Tree& leaf,
    GPlayer player,
    const GBaseStack& variants,
    GBaseStack* defense_variants = 0,
    GPoint* victory_move = 0);

  //Добавляем новый шах (если заданный вариант является шахом)
  //в заданный узел в качестве дочернего
  //и проверяем, является ли он выигрышным (вилкой 4х4)
  bool addChildMove4(
    GMove4Tree& leaf,
    GPlayer player,
    const GPoint& variants,
    GPoint* unforced_move4,
    GBaseStack* defense_variants = 0);

//  bool findVictoryMove4Chain(
//    GPlayer player,
//    uint depth,
//    GBaseStack* defense_variants = 0,
//    GPoint* victory_move = 0);

//  bool completeVictoryMove4Chain(
//    GPlayer player,
//    uint depth,
//    GBaseStack* defense_variants = 0,
//    GPoint* victory_move = 0);

//  bool findVictoryMove4Chain(
//    GPlayer player,
//    const GBaseStack& variants,
//    uint depth,
//    GBaseStack* defense_variants = 0,
//    GPoint* victory_move = 0);

//  //Поиск выигрышной цепочки шахов с заданным начальным шахом
//  bool findVictoryMove4Chain(
//    GPlayer player,
//    const GPoint& move4,
//    uint depth,
//    GBaseStack* defense_variants = 0);

//  //Вес блокировки шаха противника в цепочке шахов противника
//  bool isDefeatBlock5(
//    GPlayer player,
//    const GPoint& block,
//    uint depth,
//    GBaseStack* defense_variants = 0);

  int calcMaxAttackWgt(uint depth);
  //int calcMaxAttackWgt(GPlayer player, uint depth, bool new_attack_chain);

  int calcMaxAttackWgt(GPlayer player, const GBaseStack& attack_moves, uint depth);

  int calcAttackWgt(GPlayer player, const GPoint& move, uint depth);

  //Ответ атакующего игрока на контршах
  int calcForcedAttackWgt(GPlayer player, const GPoint& move, uint depth, bool new_counter_shah_chain);

  //Попытка продолжить атаку в после контратакующего полушаха
  //int calcMaxForcedAttackWgt(GPlayer player, const GBaseStack& defense_variants, uint depth);

  int calcMaxDefenseWgt(
    GPlayer player,
    const GBaseStack& variants,
    uint depth,
    bool new_attack_chain,
    bool new_counter_shah_chain);

  int calcDefenseWgt(GPlayer player, const GPoint& move, uint depth, bool new_attack_chain, bool new_counter_shah_chain);

  int calcMaxCounterShahWgt(
    GPlayer player,
    const GBaseStack& variants,
    uint depth,
    int& max_wgt,
    bool new_attack_chain,
    GPointStack& processed_moves);

  void getChainMoves(GStack<32>& chain_moves);
  void getChainMoves(GPlayer player, GPoint center, const GVector& v1, GStack<32>& chain_moves);
  bool isSpace5(GPlayer player, const GPoint& center, const GVector& v1);
  void getSpace(GPlayer player, GPoint move, const GVector& v1, int& space);

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

  void initMovesWgt();
  void initWgt(const GLine& line5);

  void addWgt(const GPoint& p, GPlayer player, int wgt_delta);

  void updateRelatedMovesState();
  void updateRelatedMovesState(const GVector& v1);
  void updateOpen3(const GVector& v1);
  void updateOpen3_Xxx(const GPoint& p3, const GVector& v1);
  void updateOpen3_X_xx(const GPoint& p5, const GVector& v1);
  void updateOpen3_xXx(const GPoint& p2, const GVector& v1);
  void updateOpen3_Xx_x(const GPoint& p3, const GVector& v1);
  void addOpen3(const GPoint& move);
  void undoOpen3(GMoveData& moveData);
  bool isDangerOpen3(GPlayer player, const GPoint& move, GBaseStack* defense_variants);

  void backupRelatedMovesState(const GVector& v1, uint& related_moves_iter);
  void restoreRelatedMovesState();
  void restoreRelatedMovesState(const GVector& v1, uint& related_moves_iter);

  int getFirstLineMoveWgt();
  int getFurtherMoveWgt(int line_len);
  int getBlockingMoveWgt(int line_len);
  int getLineWgt(int line_len);

  GPointStack& getDefenseGrid(uint depth)
  {
    assert(depth < MAX_DEPTH);
    m_grids[depth].clear();
    return m_grids[depth];
  }

  uint maxAttackDepth()
  {
    return getAiLevel();
  }

  int getStoredWgt(GPlayer player, const GPoint& move)
  {
    assert(get(move).player != !player);
    return get(move).wgt[player];
  }

  static bool isVictoryOrDefeat(int wgt)
  {
    return wgt == WGT_VICTORY || wgt == WGT_DEFEAT;
    //return wgt >= WGT_LONG_ATTACK || wgt <= WGT_LONG_DEFENSE;
  }

  static int updateMaxWgt(const GPoint& move, int wgt, GBaseStack* max_wgt_moves, int& max_wgt);

  static GPoint randomMove(const GBaseStack& moves);

protected:
  static const GVector vecs1[];  //{1, 0}, {1, 1}, {0, 1}, {-1, 1}

  static const int WGT_VICTORY = std::numeric_limits<int>::max() - 1;
  static const int WGT_DEFEAT  = -WGT_VICTORY;
  static const int WGT_LONG_ATTACK = 1000000000;
  static const int WGT_LONG_DEFENSE = -WGT_LONG_ATTACK;
  //Вес длинной атаки может быть скорректирован весом ситуации, в том числе в меньшую сторону
  //Поэтому нужен допуск, чтобы распознать длинную атаку
  static const int WGT_MIN_LONG_ATTACK = 999000000;
  static const int WGT_MIN_LONG_DEFENSE = -WGT_MIN_LONG_ATTACK;

  uint m_ai_level;

  GLine m_line5;

  GPointStack m_moves5[2];

  TGridStack<GDangerMoveData> m_danger_moves[2];

  const static uint MAX_DEPTH = gridSize() / 2;

  //Эти объекты используются на разной глубине рекурсии
  //Их нельзя создавать в стэке, поскольку создание является дорогой операцией
  GPointStack m_grids[MAX_DEPTH];

  int m_enemy_attack_wgt;

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
