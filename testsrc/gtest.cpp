#include "igomoku.h"
#include "../src/gomoku.h"
#include "../src/gline.h"
#include "../src/grandom.h"
#include <iostream>
#include <algorithm>

using namespace nsg;

class GTestGrid : public GPointStack
{
public:
  GTestGrid(const GBaseStack& stack)
  {
    operator=(stack);
  }

  GTestGrid& operator=(const GBaseStack& stack)
  {
    clear();
    for (const GPoint& p: stack)
      (*this)[p] = true;
    return *this;
  }
};

class TestGomoku : public Gomoku
{
public:
  void testDoMove();

  void testUndo();

  void testIsGameOver();

  //Обновление и откат пятёрок
  void testMoves5();

  //Обновление и откат четверок (шахов)
  void testMoves4();

  //Обновление и откат открытых троек
  void testOpen3();

  void testHintMove5();

  void testHintVictoryMove4Chain();

  void testFindLongAttack();

  void testLastHopeMove();

  void testHintImpl();

protected:
  void testEmpty();

  void testMove(int x, int y, GPlayer player);

  bool isMove4(GPlayer player, int x, int y)
  {
    const auto& danger_move_data = m_danger_moves[player][{x, y}];
    return !danger_move_data.m_moves5.empty();
  }

  bool isOpen3(GPlayer player, int x, int y)
  {
    const auto& danger_move_data = m_danger_moves[player][{x, y}];
    return danger_move_data.m_open3;
  }

  int calcCurWgt(GPlayer player);
  int calcLinesWgt(GPlayer player);
  int calcLinesWgt(GPlayer player, GVector v1, GPoint bp, GPoint ep);
  int calcLineWgt(GPlayer player, GPoint bp, GVector v1);
};

void TestGomoku::testDoMove()
{
  assert(!doMove(-1, -1));
  assert(doMove(7, 7));
  assert(get({7, 7}).player == G_BLACK);
  //ход в занятую точку запрещен
  assert(!doMove(7, 7));
  assert(doMove(8, 7));
  assert(get({8, 7}).player == G_WHITE);
  assert(doMove(8, 8, G_BLACK));
  assert(doMove(9, 9, G_BLACK));
  assert(doMove(10, 10, G_BLACK));
  assert(!getLine5());
  assert(doMove(11, 11, G_BLACK));
  assert(getLine5());
  //игра окончена, ходы больше делать нельзя
  assert(!doMove(12, 12, G_BLACK));
}

void TestGomoku::testUndo()
{
  //Нет ходов для отмены
  assert(!undo());

  testDoMove();
  assert(getLine5());

  assert(undo());
  assert(!getLine5());

  while(undo());

  //После отката всех ходов должны вернуться к исходному состоянию
  testEmpty();
}

void TestGomoku::testEmpty()
{
  TestGomoku tmp;

  assert(!getLine5());
  assert(cells().empty());
  assert(m_moves5[G_BLACK].cells().empty());
  assert(m_moves5[G_WHITE].cells().empty());
  GPoint p{0, 0};
  do
  {
    assert(isEmptyCell(p));
    assert(m_moves5[G_BLACK].isEmptyCell(p));
    assert(m_moves5[G_WHITE].isEmptyCell(p));
    const GMoveData& data = get(p);
    const GMoveData& tmp_data = tmp.get(p);
    assert(data.wgt[G_BLACK] == tmp_data.wgt[G_BLACK]);
    assert(data.wgt[G_WHITE] == tmp_data.wgt[G_WHITE]);
    assert(data.m_moves5_count == 0);
  }
  while (next(p));
}

void TestGomoku::testMove(int x, int y, GPlayer player)
{
  int wgt1 = calcCurWgt(player);

  int move_wgt = get({x, y}).wgt[player];

  doMove(x, y, player);

  int wgt2 = calcCurWgt(player);

  assert(wgt2 == wgt1 + move_wgt);
}

int TestGomoku::calcCurWgt(GPlayer player)
{
  return calcLinesWgt(player) - calcLinesWgt(!player);
}

int TestGomoku::calcLinesWgt(GPlayer player)
{
  return
    calcLinesWgt(player, {1, 0}, {0, 0}, {10, 14}) +
    calcLinesWgt(player, {0, 1}, {0, 0}, {14, 10}) +
    calcLinesWgt(player, {1, 1}, {0, 0}, {10, 10}) +
    calcLinesWgt(player, {1, -1}, {0, 4}, {10, 14});
}

int TestGomoku::calcLinesWgt(GPlayer player, GVector v1, GPoint bp, GPoint ep)
{
  GPoint blp = bp; //начало линии
  int wgt = 0;
  for (; ; )
  {
    wgt += calcLineWgt(player, blp, v1);

    if (blp.x == ep.x)
    {
      if (blp.y == ep.y)
        break;
      blp.x = 0;
      ++blp.y;
    }
    else
      ++blp.x;
  }
  return wgt;
}

int TestGomoku::calcLineWgt(GPlayer player, GPoint bp, GVector v1)
{
  uint player_moves_count = 0;

  GPoint ep = bp + v1 * 4;

//  GPlayer first_player = get(bp).player;
//  GPlayer last_player = get(ep).player;
//  GPlayer prev_player = isValidCell(bp - v1) ? get(bp - v1).player : G_EMPTY;
//  GPlayer next_player = isValidCell(ep + v1) ? get(ep + v1).player : G_EMPTY;

  for (; ; bp += v1)
  {
    GPlayer bp_player = get(bp).player;
    if (bp_player == !player)
      return 0;
    if (bp_player == player)
      ++player_moves_count;
    if (bp == ep)
      break;
  }

  return (player_moves_count > 0) ? getLineWgt(player_moves_count) : 0;
}

void TestGomoku::testIsGameOver()
{
  assert(!isGameOver());
  //Окончание игры при выигрыше
  for (int x = 7; x < 11; ++x)
  {
    doMove(x, x, G_BLACK);
    assert(!isGameOver());
  }
  doMove(11, 11, G_BLACK);
  assert(isGameOver());

  start();
  //Окончание игры при отсутствии свободных ячеек
  //хохо
  //хохо
  //хохо
  //охох
  //охох
  //охох
  GPoint move{0, 0};
  GPlayer player = G_BLACK;

  auto fillRowWithout5 = [&]()
  {
    assert(move.x == 0);
    for (; ; )
    {
      assert(!isGameOver());
      assert(doMove(move, player));
      if (!next(move) || move.x == 0)
        break;
      player = !player;
    }
  };

  auto fill3RowsWithout5 = [&]()
  {
    for (int i = 0; i < 3; ++i)
      fillRowWithout5();
  };

  for (int i = 0; i < 5; ++i, player = !player)
    fill3RowsWithout5();
  assert(isGameOver());
}

void TestGomoku::testMoves5()
{
  auto& moves5 = m_moves5[G_BLACK];
  for (int i = 0; i < 4; ++i)
  {
    assert(moves5.cells().empty());
    doMove(7 + i, 7, G_BLACK);
  }
  assert(moves5.cells().size() == 2 && !moves5.isEmptyCell({6, 7}) && !moves5.isEmptyCell({11, 7}));
  //число потенциальных ходов 5 фиксируется в порождающем ходе для возможности отмены
  assert(get({10, 7}).m_moves5_count == 2);

  undo();
  //При откате ходы 5 пропадают
  assert(moves5.cells().empty());
  assert(get({10, 7}).m_moves5_count == 0);

  //Если ход занят противником, то он не фиксируется как ход 5
  doMove(11, 7, G_WHITE);
  doMove(10, 7, G_BLACK);
  assert(moves5.cells().size() == 1 && moves5.isEmptyCell({11, 7}));

  //Один и тот же ход 5 не фиксируется в разных источниках
  assert(!moves5.isEmptyCell({6, 7}));
  doMove(7, 6, G_BLACK);
  doMove(5, 8, G_BLACK);
  doMove(8, 5, G_BLACK);
  doMove(4, 9, G_BLACK);
  assert(get(lastCell()).m_moves5_count == 0);
  //При откате ход 5 остается
  undo();
  assert(!moves5.isEmptyCell({6, 7}));
}

void TestGomoku::testMoves4()
{
  auto& danger_moves = dangerMoves(G_BLACK);

  doMove(7, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  assert(!isMove4(G_BLACK, 5, 7) && !isMove4(G_BLACK, 6, 7) && !isMove4(G_BLACK, 10, 7) && !isMove4(G_BLACK, 11, 7));
  doMove(9, 7, G_BLACK);
  //Следующие ходы являются шахами
  assert(isDangerMove4(G_BLACK, {5, 7}) && isDangerMove4(G_BLACK, {6, 7}) && isDangerMove4(G_BLACK, {10, 7}) && isDangerMove4(G_BLACK, {11, 7}));
  const auto& move_data_97 = get({9, 7});
  //В порождающем ходе шахи фиксируются парами
  assert(move_data_97.m_moves4.size() == 6);
  const auto& move4_data_57 = danger_moves[{5, 7}];
  GTestGrid moves5(move4_data_57.m_moves5);
  assert(moves5.cells().size() == 1 && !moves5.isEmptyCell({6, 7}));
  const auto& move4_data_67 = danger_moves[{6, 7}];
  moves5 = move4_data_67.m_moves5;
  assert(moves5.cells().size() == 2 && !moves5.isEmptyCell({5, 7}) && !moves5.isEmptyCell({10, 7}));

  doMove(10, 7, G_WHITE);
  //Ходы (10, 7), (11, 7) остаются зафиксированными в множестве ходов 4 и в порождающем ходе, но перестают быть опасными
  assert(isDangerMove4(G_BLACK, {5, 7}) && isDangerMove4(G_BLACK, {6, 7}));
  assert(isMove4(G_BLACK, 10, 7) && !isDangerMove4(G_BLACK, {10, 7}) && isMove4(G_BLACK, 11, 7) && !isDangerMove4(G_BLACK, {11, 7}));

  undo();
  //При отмене блокирующего хода все ходы 4 снова становятся опасными
  assert(isDangerMove4(G_BLACK, {5, 7}) && isDangerMove4(G_BLACK, {6, 7}) && isDangerMove4(G_BLACK, {10, 7}) && isDangerMove4(G_BLACK, {11, 7}));

  undo();
  //При отмене порождающего хода все ходы 4 пропадают
  assert(!isMove4(G_BLACK, 5, 7) && !isMove4(G_BLACK, 6, 7) && !isMove4(G_BLACK, 10, 7) && !isMove4(G_BLACK, 11, 7));
  assert(move_data_97.m_moves4.empty());

  //В ситуации ххХ__х нужно избежать дублирования одной и той же пары ходов 4
  doMove(12, 7, G_BLACK);
  doMove(9, 7, G_BLACK);
  assert(move_data_97.m_moves4.size() == 6);

  //Один и тот же ход 4 может быть добавлен разными порождающими ходами с разными парными ходами
  doMove(6, 8, G_BLACK);
  doMove(6, 10, G_BLACK);
  doMove(6, 6, G_BLACK);
  moves5 = move4_data_67.m_moves5;
  assert(moves5.cells().size() == 3 && !moves5.isEmptyCell({6, 9}));
  assert(get({6, 6}).m_moves4.size() == 2);
  //При откате этот ход 4 не пропадает, но пропадает один связанный с ним ход 5
  undo();
  moves5 = move4_data_67.m_moves5;
  assert(moves5.cells().size() == 2 && moves5.isEmptyCell({6, 9}));
}

void TestGomoku::testOpen3()
{
  auto& danger_moves = dangerMoves(G_BLACK);

  doMove(7, 7, G_BLACK);
  assert(danger_moves.cells().empty());

  doMove(8, 7, G_BLACK);
  const auto& last_move_data = get({8, 7});
  assert(last_move_data.m_open3_moves.size() == 4);
  //xxX
  assert(isOpen3(G_BLACK, 9, 7));
  //Xxx
  assert(isOpen3(G_BLACK, 6, 7));
  //хх_Х
  assert(isOpen3(G_BLACK, 10, 7));
  //Х_хх
  assert(isOpen3(G_BLACK, 5, 7));
  assert(danger_moves.cells().size() == 4);

  undo();
  //При откате открытые тройки пропадают
  assert(danger_moves.cells().empty());
  assert(last_move_data.m_open3_moves.empty());
  doMove(9, 7, G_WHITE);
  doMove(8, 7, G_BLACK);
  //Не Хххо
  //Не ххО
  //Не Х_ххо
  //Не ххоХ
  assert(danger_moves.cells().empty());

  undo();
  undo();
  doMove(10, 7, G_WHITE);
  doMove(8, 7, G_BLACK);
  //Ххх_о
  assert(isOpen3(G_BLACK, 6, 7));
  //Х_хх_о
  assert(isOpen3(G_BLACK, 5, 7));
  //Не ххХо
  //Не хх_О
  assert(danger_moves.cells().size() == 2);

  undo();
  undo();
  doMove(11, 7, G_WHITE);
  doMove(5, 7, G_WHITE);
  doMove(8, 7, G_BLACK);
  //Не о_ххХ_о
  //Не оХхх
  //Не О_хх
  //Не хх_Хо
  assert(danger_moves.cells().empty());

  undo();
  undo();
  undo();
  doMove(11, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  //ххХ_х - шах, но не открытая тройка
  {
    const auto& danger_move_data = danger_moves.get({9, 7});
    assert(!danger_move_data.empty() && !danger_move_data.m_open3);
  }
  //Ххх__х
  assert(isOpen3(G_BLACK, 6, 7));
  //хх_Хх - шах, но не открытая тройка
  {
    const auto& danger_move_data = danger_moves.get({10, 7});
    assert(!danger_move_data.empty() && !danger_move_data.m_open3);
  }
  //Х_хх__х
  assert(isOpen3(G_BLACK, 5, 7));

  undo();
  undo();
  undo();
  doMove(3, 1, G_BLACK);
  doMove(2, 2, G_BLACK);
  //Не ххХ|
  //Не |_Ххх_|
  //Не |Х_хх
  assert(danger_moves.cells().empty());

  undo();
  doMove(4, 0, G_BLACK);
  //Не Ххх|
  //Не Х_хх|
  assert(danger_moves.cells().empty());

  undo();
  undo();
  doMove(7, 7, G_BLACK);
  assert(danger_moves.cells().empty());

  doMove(5, 5, G_BLACK);
  //хХх
  assert(isOpen3(G_BLACK, 6, 6));
  //x_xX
  assert(isOpen3(G_BLACK, 8, 8));
  //Xx_x
  assert(isOpen3(G_BLACK, 4, 4));
  assert(danger_moves.cells().size() == 3);

  undo();
  doMove(4, 4, G_WHITE);
  doMove(5, 5, G_BLACK);
  //Не охХх
  //Не ох_хХ
  //Не Ох_х
  assert(danger_moves.cells().empty());

  undo();
  undo();
  doMove(3, 3);
  doMove(5, 5, G_BLACK);
  //о_хХх__
  assert(isOpen3(G_BLACK, 6, 6));
  //о_x_xX_
  assert(isOpen3(G_BLACK, 8, 8));
  //Не оХх_х
  assert(danger_moves.cells().size() == 2);

  undo();
  undo();
  doMove(3, 3, G_WHITE);
  doMove(9, 9, G_WHITE);
  doMove(5, 5, G_BLACK);
  //Не о_хХх_о
  //Не оХх_х
  //Не х_хХо
  assert(danger_moves.cells().empty());

  undo();
  undo();
  undo();
  undo();
  doMove(5, 5, G_BLACK);
  doMove(9, 9, G_BLACK);
  doMove(7, 7, G_BLACK);
  //Хх_х_х
  assert(isOpen3(G_BLACK, 4, 4));
  //х_х_хХ
  assert(isOpen3(G_BLACK, 10, 10));
  //хХх_х - шах, но не открытая тройка
  {
    const auto& danger_move_data = danger_moves.get({6, 6});
    assert(!danger_move_data.empty() && !danger_move_data.m_open3);
  }
  //х_хХх - шах, но не открытая тройка
  {
    const auto& danger_move_data = danger_moves.get({8, 8});
    assert(!danger_move_data.empty() && !danger_move_data.m_open3);
  }

  undo();
  undo();
  undo();
  doMove(3, 1);
  doMove(1, 3);
  //Не |_xXx_|
  //Не |Xx_x
  //Не x_xX|
  assert(danger_moves.cells().empty());

  undo();
  undo();
  doMove(7, 7, G_BLACK);
  doMove(7, 10, G_BLACK);

  //хХ_х
  assert(isOpen3(G_BLACK, 7, 8));
  //х_Хх
  assert(isOpen3(G_BLACK, 7, 9));

  undo();
  doMove(7, 6, G_WHITE);
  doMove(7, 10, G_BLACK);
  //Не х_Ххо
  //Не хХ_хо
  assert(danger_moves.cells().empty());

  undo();
  undo();
  undo();
  doMove(0, 14, G_BLACK);
  doMove(3, 11, G_BLACK);
  //Не хХ_х|
  //Не х_Хх|
  assert(danger_moves.cells().empty());

  undo();
  undo();

  //Один и тот же ход, реализующий открытую тройку, не фиксируется в разных источниках
  doMove(7, 7, G_BLACK);
  doMove(6, 8, G_BLACK);
  assert(!danger_moves.isEmptyCell({9, 5}));
  doMove(10, 6, G_BLACK);
  doMove(8, 4, G_BLACK);
  const auto& open3_moves = get({8, 4}).m_open3_moves;
  assert(open3_moves.size() == 2 && open3_moves[0] != (GPoint{9, 5}) && open3_moves[1] != (GPoint{9, 5}));
  //При откате последнего хода возможность открытой тройки сохраняется
  undo();
  assert(!danger_moves.isEmptyCell({9, 5}));
}

void TestGomoku::testHintMove5()
{
  GPoint move5;

  doMove(7, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  doMove(6, 7, G_BLACK);
  assert(!hintMove5(G_BLACK, move5) && !hintBlock5(G_WHITE, move5));

  doMove(5, 7, G_WHITE);
  doMove(9, 7, G_BLACK);
  assert(hintMove5(G_BLACK, move5) && move5 == (GPoint{10, 7}));
  assert(hintBlock5(G_WHITE, move5) && move5 == (GPoint{10, 7}));

  doMove(10, 7, G_WHITE);
  assert(!hintMove5(G_BLACK, move5) && !hintBlock5(G_WHITE, move5));
}

void TestGomoku::testHintVictoryMove4Chain()
{
  doMove(7, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  assert(!findVictoryMove4Chain(G_BLACK, {9, 7}, 0));
  doMove(6, 7, G_BLACK);

  GPoint move4;
  assert(findVictoryMove4Chain(G_BLACK, 0, 0, &move4));
  assert((move4 == GPoint{5, 7} || move4 == GPoint{9, 7}));

  GStack<32> defense_variants;
  assert(findVictoryMove4Chain(G_BLACK, {9, 7}, 0, &defense_variants));
  GTestGrid grid(defense_variants);
  assert(grid.cells().size() == 2 && !grid.isEmptyCell({9, 7}) && !grid.isEmptyCell({5, 7}));
  defense_variants.clear();
  assert(findVictoryMove4Chain(G_BLACK, {5, 7}, 0, &defense_variants));
  grid = defense_variants;
  assert(grid.cells().size() == 2 && !grid.isEmptyCell({9, 7}) && !grid.isEmptyCell({5, 7}));

  doMove(9, 7, G_WHITE);
  assert(!findVictoryMove4Chain(G_BLACK, {9, 7}, 0));
  assert(!findVictoryMove4Chain(G_BLACK, {5, 7}, 0));
  assert(!findVictoryMove4Chain(G_BLACK, 0));

  undo();
  doMove(10, 7, G_WHITE);
  assert(!findVictoryMove4Chain(G_BLACK, {9, 7}, 0));
  assert(findVictoryMove4Chain(G_BLACK, {5, 7}, 0));

  doMove(10, 6, G_BLACK);
  doMove(11, 5, G_BLACK);
  doMove(12, 4, G_BLACK);
  doMove(13, 3, G_WHITE);
  doMove(10, 8, G_BLACK);
  doMove(11, 9, G_BLACK);
  doMove(12, 10, G_BLACK);
  doMove(13, 11, G_WHITE);
  defense_variants.clear();
  assert(findVictoryMove4Chain(G_BLACK, {9, 7}, 0, &defense_variants));
  //Вилка кратностью больше двух контрится только одним ходом
  assert((defense_variants.size() == 1 && defense_variants[0] == GPoint{9, 7}));

  start();
  doMove(7, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  doMove(6, 7, G_BLACK);
  doMove(9, 7, G_WHITE);
  doMove(5, 5, G_BLACK);
  doMove(5, 4, G_BLACK);
  //Мат в два хода
  defense_variants.clear();
  assert(findVictoryMove4Chain(G_BLACK, 1, &defense_variants, &move4));
  assert((move4 == GPoint{5, 7}));
  assert(defense_variants.size() == 5);
  grid = defense_variants;
  assert(
    grid.cells().size() == 5 &&
    !grid.isEmptyCell({5, 7}) &&
    !grid.isEmptyCell({4, 7}) &&
    !grid.isEmptyCell({5, 3}) &&
    !grid.isEmptyCell({5, 6}) &&
    !grid.isEmptyCell({5, 8}));

  doMove(4, 8, G_WHITE);
  doMove(4, 9, G_WHITE);

  //Тоже мат в два хода, но к списку защитных ходов добавляются ходы,
  //благодаря которым ответ на первый шах становится контршахом
  defense_variants.clear();
  assert(findVictoryMove4Chain(G_BLACK, 1, &defense_variants, &move4));
  assert((move4 == GPoint{5, 7}));
  //Защитные ходы могут дублироваться
  assert(defense_variants.size() >= 9);
  grid = defense_variants;
  assert(
    grid.cells().size() == 9 &&
    !grid.isEmptyCell({5, 7}) &&
    !grid.isEmptyCell({4, 7}) &&
    !grid.isEmptyCell({5, 3}) &&
    !grid.isEmptyCell({5, 6}) &&
    !grid.isEmptyCell({5, 8}) &&
    !grid.isEmptyCell({4, 6}) &&
    !grid.isEmptyCell({4, 10}) &&
    !grid.isEmptyCell({4, 5}) &&
    !grid.isEmptyCell({4, 11}));

  doMove(4, 10, G_WHITE);
  doMove(4, 11, G_BLACK);
  //Ответ на первый шах является контршахом,
  //поэтому мат в два хода невозможен
  assert(!findVictoryMove4Chain(G_BLACK, 1));

  doMove(3, 7, G_BLACK);
  doMove(2, 8, G_BLACK);
  doMove(6, 4, G_WHITE);
  //Ответ на контршах также является контршахом,
  //поэтому здесь мат в три хода
  assert(!findVictoryMove4Chain(G_BLACK, 1));
  defense_variants.clear();
  assert(findVictoryMove4Chain(G_BLACK, 2, &defense_variants, &move4));
  //assert((move4 == GPoint{5, 7}));
  assert(defense_variants.size() == 7);
  grid = defense_variants;
  assert(
    grid.cells().size() == 7 &&
    !grid.isEmptyCell({5, 7}) &&
    !grid.isEmptyCell({4, 7}) &&
    !grid.isEmptyCell({4, 6}) &&
    !grid.isEmptyCell({1, 9}) &&
    !grid.isEmptyCell({5, 3}) &&
    !grid.isEmptyCell({5, 6}) &&
    !grid.isEmptyCell({5, 8}));

  undo();
  undo();
  undo();
  undo();
  doMove(3, 7, G_BLACK);
  doMove(2, 8, G_BLACK);
  doMove(6, 4, G_WHITE);

  //Ответ на первый шах является контрматом,
  //поэтому мат в три хода невозможен
  assert(!findVictoryMove4Chain(G_BLACK, 2));
}

void TestGomoku::testFindLongAttack()
{
  setAiLevel(4);

  doMove(7, 7); //
  doMove(6, 8);
  doMove(7, 6); //
  doMove(7, 8);
  doMove(8, 8); //
  doMove(8, 7);
  doMove(9, 6); //
  doMove(6, 6);
  doMove(6, 7); //
  doMove(5, 8);
  doMove(9, 4); //
  doMove(4, 8);
  doMove(3, 8); //
  doMove(7, 5);
  doMove(5, 7); //
  doMove(8, 5);
  doMove(10, 5); //
  doMove(9, 7);
  doMove(10, 4); //
  doMove(8, 6);
  doMove(10, 8); //
  doMove(5, 5);
  doMove(4, 5); //
  doMove(6, 4);
  doMove(5, 3); //

  assert(findLongAttack(G_WHITE, {7, 3}, 5));
}

void TestGomoku::testLastHopeMove()
{
  setAiLevel(4);

  testMove(7, 7, G_BLACK); //
  testMove(6, 6, G_WHITE);
  testMove(7, 8, G_BLACK); //
  testMove(7, 6, G_WHITE);
  testMove(8, 6, G_BLACK); //
  testMove(6, 8, G_WHITE);
  testMove(8, 7, G_BLACK); //
  testMove(9, 7, G_WHITE);
  testMove(8, 9, G_BLACK); //
  testMove(8, 10, G_WHITE);
  testMove(6, 9, G_BLACK); //
  testMove(9, 6, G_WHITE);
  testMove(7, 9, G_BLACK); //
  testMove(9, 9, G_WHITE);
  testMove(9, 10, G_BLACK); //

  GPoint lhm = lastHopeMove(G_WHITE);
  assert(lhm == (GPoint{9, 8}));
}

void TestGomoku::testHintImpl()
{
  setAiLevel(4);

  doMove(7, 7); //
  doMove(6, 7);
  doMove(6, 6); //
  doMove(5, 5);
  doMove(7, 6); //
  doMove(7, 8);
  doMove(5, 6); //
  doMove(4, 6);
  doMove(6, 5); //
  doMove(6, 4);
  doMove(3, 7); //
  doMove(7, 4);
  doMove(8, 4); //
  doMove(9, 6);
  doMove(8, 7); //
  doMove(9, 8);
  doMove(7, 5); //
  doMove(5, 7);

  {
    GMoveMaker gmm(this, G_BLACK, {9, 5});
    assert(findVictoryMove4Chain(G_BLACK, {8, 5}, 1));
  }

  //Ход черных
  //Белые следующим ходом могут реализовать вилку 3х3
  //Черные же ходом (9, 5) могут сыграть на опережение и
  //создать угрозу вилки 4х3
  //Но это плохой ход, потому что белые блокируют эту угрозу,
  //одновременно начиная свою атаку полушахом
  //С учетом угрозы вилки 3х3 (см. выше) эта атака белых неблокируема
  assert(hintImpl(G_BLACK) != (GPoint{9, 5}));
}

using TestFunc = void();
void gtest(const char* name, TestFunc f, uint count = 1)
{
  std::cout << name << std::endl;
  for (; count > 0; --count)
    f();
  std::cout << "OK" << std::endl << std::endl;
}

template<class T>
using TestMember = void (T::*)();

template<class T>
void gtest(const char* name, TestMember<T> f, uint count = 1)
{
  std::cout << name << std::endl;
  for (; count > 0; --count)
  {
    T obj;
    (obj.*f)();
  }
  std::cout << "OK" << std::endl << std::endl;
}

int main()
{
  gtest("testDoMove", &TestGomoku::testDoMove);
  gtest("testUndo", &TestGomoku::testUndo);
  gtest("testIsGameOver", &TestGomoku::testIsGameOver);
  gtest("testMoves5", &TestGomoku::testMoves5);
  gtest("testMoves4", &TestGomoku::testMoves4);
  gtest("testOpen3", &TestGomoku::testOpen3);
  gtest("testHintMove5", &TestGomoku::testHintMove5);
  gtest("testHintVictoryMove4Chain", &TestGomoku::testHintVictoryMove4Chain);
  gtest("testFindLongAttack", &TestGomoku::testFindLongAttack);
  gtest("testLastHopeMove", &TestGomoku::testLastHopeMove);
  gtest("testHintImpl", &TestGomoku::testHintImpl);
}
