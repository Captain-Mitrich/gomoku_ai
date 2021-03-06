#ifndef GOMOKU_H
#define GOMOKU_H

#include "igomoku.h"
#include "ggrid.h"
#include "gline.h"
#include "gstack.h"
#include "gplayer.h"
#include "grandom.h"
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
  uint getMoveCount(GPlayer player);

protected:

  friend class GMoveMaker;
  friend class GCounterShahChainMaker;

  class GVariantsIndex : public GStack<gridSize()>
  {
  public:
    GVariantsIndex()
    {
      GPoint p{0, 0};
      do
      {
        push() = p;
      }
      while (next(p));
    }
  };

  bool randomFromTwo(GVariantsIndex& var_index, GPoint*& cur, const GPoint* end);

  void copyFrom(const Gomoku& g);

  void undoImpl();
  void doInMind(const GPoint& move, GPlayer player);
  void undoInMind();

  GPoint hintImpl(GPlayer player);
  GPoint hintSecondMove() const;
  GPoint hintThirdMove(GPlayer player);
  GPoint hintForthMove(GPlayer player);

  bool hintMove5(GPlayer player, GPoint& move) const;
  bool hintBlock5(GPlayer player, GPoint& move) const;

  //Поиск выигрышной цепочки шахов
  bool findVictoryMove4Chain(
    GPlayer player,
    uint depth,
    GBaseStack* defense_variants = 0,
    GPoint* victory_move = 0);

  bool completeVictoryMove4Chain(
    GPlayer player,
    uint depth,
    GBaseStack* defense_variants = 0,
    GPoint* victory_move = 0);

  bool findVictoryMove4Chain(
    GPlayer player,
    const GBaseStack& variants,
    uint depth,
    GBaseStack* defense_variants = 0,
    GPoint* victory_move = 0);

  //Поиск выигрышной цепочки шахов с заданным начальным шахом
  bool findVictoryMove4Chain(
    GPlayer player,
    const GPoint& move4,
    uint depth,
    GBaseStack* defense_variants = 0);

  //Вес блокировки шаха противника в цепочке шахов противника
  bool isDefeatBlock5(
    GPlayer player,
    const GPoint& block,
    uint depth,
    GBaseStack* defense_variants = 0);

  bool findLongOrVictoryMove4Chain(GPlayer player, uint depth);
  bool completeLongOrVictoryMove4Chain(GPlayer player, uint depth);
  bool findLongOrVictoryMove4Chain(GPlayer player, const GBaseStack &variants, uint depth);
  bool findLongOrVictoryMove4Chain(GPlayer player, const GPoint& move4, uint depth);
  bool isLongOrDefeatBlock5(GPlayer player, const GPoint &block, uint depth);

  bool findVictoryAttack(GPlayer player, uint depth, GPoint* victory_move = 0);
  bool findVictoryAttack(GPlayer player, const GBaseStack& variants, uint depth, GPoint* victory_move = 0);
  bool isVictoryMove(GPlayer player, const GPoint& move, uint depth);
  bool isVictoryMove4(GPlayer player, const GPoint& move, uint depth);
  bool isNearVictoryOpen3(GPlayer player, const GPoint &move, uint depth);
  bool isDefeatMove(GPlayer player, const GPoint& move, uint depth);

  bool findLongAttack(GPlayer player, uint depth, GPoint* move = 0);
  bool findLongAttack(GPlayer player, const GBaseStack& attack_moves, uint depth, GPoint* move = 0);
  bool findLongAttack(GPlayer player, const GPoint& move, uint depth);
  bool isLongDefense(GPlayer player, const GPoint& move, uint depth);

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
  bool isDangerOpen3(GPlayer player, const GPoint& move, GBaseStack* defense_variants = 0);
  bool isDangerOpen3(GBaseStack* defense_variants = 0);
  bool isMate(GPlayer player, const GPoint& move);
  bool isMate();
  bool isShah(GPlayer player);

  void backupRelatedMovesState(const GVector& v1, uint& related_moves_iter);
  void restoreRelatedMovesState();
  void restoreRelatedMovesState(const GVector& v1, uint& related_moves_iter);

  int getFirstLineMoveWgt();
  int getFurtherMoveWgt(int line_len);
  int getBlockingMoveWgt(int line_len);
  int getLineWgt(int line_len);

  uint maxAttackDepth()
  {
    return getAiLevel() * 2;
  }

  int getStoredWgt(GPlayer player, const GPoint& move)
  {
    assert(get(move).player != !player);
    return get(move).wgt[player];
  }

  int maxStoredWgt();

  void sortVariantsByWgt(GPlayer player, GVariantsIndex& variants_index);
  void sortMaxN(GPlayer player, GVariantsIndex& variants_index, uint n);

  bool cmpVariants(GPlayer player, const GPoint& variant1, const GPoint& variant2);

  static GPoint randomMove(const GBaseStack& moves);

protected:
  static const int WGT_VICTORY     = 1000000;
  static const int WGT_LONG_ATTACK = 999000;

  static bool isSpecialWgt(int wgt)
  {
    return wgt > 900000 || wgt < -900000;
  }

  template <uint Depth, uint MaxWgtChildrenCount>
  class GVariantsIterator
  {
  public:

    GVariantsIterator(Gomoku* g) : m_g(g), m_cur_depth(0), m_root_depth(0)
    {
      firstChild();
      assert(Depth > 0);
    }

    ~GVariantsIterator()
    {
      for (; m_cur_depth > 0; --m_cur_depth)
        undo();
    }

    uint curDepth() const
    {
      return m_cur_depth;
    }

    uint curIndex(uint depth) const
    {
      assert(depth >= 1 && depth <= Depth);
      return m_variants[depth - 1].m_cur_index;
    }

    const GPoint& getMove(uint depth, uint index)
    {
      assert(depth > 0 && depth <= Depth);
      assert(index < MaxWgtChildrenCount);
      return m_variants[depth - 1][index];
    }

    int maxWgt(uint depth) const
    {
      assert(depth >= 1 && depth <= Depth);
      return m_variants[depth - 1].m_maxwgt;
    }

    void setRootDepth(uint depth)
    {
      assert(depth < Depth);
      m_root_depth = depth;
    }

    bool next()
    {
      if (m_cur_depth == Depth)
        return setWgt(curStoredWgt() - m_g->maxStoredWgt());
      else if (!m_g->isEmptyCell(getMove(m_cur_depth + 1, 0))) //все клетки заняты
        return setWgt(curStoredWgt());
      return firstChild();
    }

    bool nextBrother()
    {
      if (++curIndex() == MaxWgtChildrenCount || !m_g->isEmptyCell(curMove()))
        return nextParent(curMaxWgt());
      undo();
      doMove();
      return true;
    }

    bool nextParent(uint max_child_wgt)
    {
      undo();
      if (--m_cur_depth <= m_root_depth)
        return false;
      if (max_child_wgt == -WGT_VICTORY)
        return nextParent(WGT_VICTORY);

      int wgt;
      if (isSpecialWgt(max_child_wgt))
        wgt = -max_child_wgt;
      else
        wgt = curStoredWgt() - max_child_wgt;

      return setWgt(wgt);
    }

    bool setWgt(int wgt)
    {
      updateMaxWgt(wgt);
      if (wgt == WGT_VICTORY)
        return nextParent(WGT_VICTORY);
      return nextBrother();
    }

    void updateMaxWgt(int wgt)
    {
      if (wgt > curMaxWgt())
      {
        curMaxWgt() = wgt;
        if (curDepth() == 1)
          m_maxwgt_moves.clear();
      }
      if (wgt == curMaxWgt() && curDepth() == 1)
        m_maxwgt_moves.push() = curMove();
    }

    int curStoredWgt()
    {
      return m_g->getStoredWgt(m_g->lastMovePlayer(), curMove());
    }

    GPoint best()
    {
      assert(!m_maxwgt_moves.empty());
      return m_maxwgt_moves[random(0, m_maxwgt_moves.size())];
    }

  protected:
    class GVariants : public GVariantsIndex
    {
    public:
      GVariants()
      {}

      int m_maxwgt = -WGT_VICTORY;
      uint m_cur_index = 0;
    };

    GVariants& curVariants()
    {
      assert(curDepth() > 0 && curDepth() <= Depth);
      return m_variants[curDepth() - 1];
    }

    uint& curIndex()
    {
      return curVariants().m_cur_index;
    }

    const GPoint& curMove() const
    {
      assert(curDepth() > 0 && curDepth() <= Depth);
      const GVariants& variants = m_variants[curDepth() - 1];
      return variants[variants.m_cur_index];
    }

    int& curMaxWgt()
    {
      return curVariants().m_maxwgt;
    }

    void doMove()
    {
      m_g->doInMind(curMove(), !m_g->lastMovePlayer());
    }

    void undo()
    {
      m_g->undoInMind();
    }

    bool firstChild()
    {
      assert(m_cur_depth < Depth);
      assert(m_g->isEmptyCell(getMove(m_cur_depth + 1, 0)));

      ++m_cur_depth;
      sortCurVariants();
      curIndex() = 0;
      curMaxWgt() = -WGT_VICTORY;
      doMove();
      return true;
    }

    void sortCurVariants()
    {
      //Ищем требуемое число элементов с максимальным весом
      m_g->sortMaxN(!m_g->lastMovePlayer(), curVariants(), MaxWgtChildrenCount);
    }

  protected:
    Gomoku* m_g;

    uint m_cur_depth;

    uint m_root_depth;

    GVariants m_variants[Depth];

    GStack<MaxWgtChildrenCount> m_maxwgt_moves;
  };

protected:
  static const GVector vecs1[];  //{1, 0}, {1, 1}, {0, 1}, {-1, 1}

  uint m_ai_level;

  GLine m_line5;

  GPointStack m_moves5[2];

  TGridStack<GDangerMoveData> m_danger_moves[2];

  GVariantsIndex m_variants_index[2];

  //При неудачном поиске выигрышной атаки определяем,
  //возможна ли длинная атака
  //false - длинной атаки нет, можно не искать
  //true - длинная атака возможна
  bool m_long_attack_possible;

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
      m_g->undoInMind();
  }

  void doMove(Gomoku* g, GPlayer player, const GPoint& move)
  {
    assert(!m_g);
    m_g = g;
    g->doInMind(move, player);
  }

protected:
  Gomoku* m_g;
};

//Класс в конструкторе реализует ходы до завершения цепочки контршахов,
//а в деструкторе откатывает их
class GCounterShahChainMaker
{
public:
  GCounterShahChainMaker(Gomoku* g) : m_g(g), m_counter(0)
  {
    assert(g);
    for (; ; )
    {
      const auto& last_move_data = m_g->get(m_g->lastCell());
      if (last_move_data.m_moves5_count != 1)
        break;
      const GPoint& block = m_g->m_moves5[last_move_data.player].lastCell();
      m_g->doInMind(block, !last_move_data.player);
      ++m_counter;
    }
  }

  ~GCounterShahChainMaker()
  {
    assert(m_g);
    while (m_counter > 0)
      undo();
  }

  void undo()
  {
    assert(m_counter > 0);
    m_g->undoInMind();
    --m_counter;
  }

protected:
  Gomoku* m_g;
  uint m_counter;
};

} //namespace nsg

#endif
