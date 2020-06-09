#include "igomoku.h"
#include "../src/gomoku.h"
#include "../src/gline.h"
#include "../src/grandom.h"
#include <iostream>

namespace nsg
{

extern bool adjustLineStart(GPoint& start, const GVector& v1, int lineLen, int width, int height);

extern GVector randomV1();

}

using namespace nsg;

class TestGomoku : public Gomoku
{
public:
  TestGomoku(int _width = 15, int _height = 15) : Gomoku(_width, _height)
  {}

  void testDoMove();

  void testUndo();

  void testIsGameOver();

  //Обновление и откат пятёрок
  void testMoves5();

  //Обновление и откат четверок (шахов)
  void testMoves4();

  //Обновление и откат открытых троек
  void testOpen3();

protected:
  void testEmpty();

  bool isMove4(GPlayer player, int x, int y)
  {
    const auto& danger_move_data = m_danger_moves[player][{x, y}];
    return danger_move_data && danger_move_data->m_moves5.cells().size() > 0;
  }

  bool isOpen3(GPlayer player, int x, int y)
  {
    const auto& danger_move_data = m_danger_moves[player][{x, y}];
    return danger_move_data && danger_move_data->m_open3;
  }
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
  assert(getLine5Moves(G_BLACK).cells().empty());
  assert(getLine5Moves(G_WHITE).cells().empty());
  GPoint p(0, 0);
  do
  {
    assert(isEmptyCell(p));
    assert(getLine5Moves(G_BLACK).isEmptyCell(p));
    assert(getLine5Moves(G_WHITE).isEmptyCell(p));
    const GMoveData& data = get(p);
    const GMoveData& tmp_data = tmp.get(p);
    assert(data.wgt[G_BLACK] == tmp_data.wgt[G_BLACK]);
    assert(data.wgt[G_WHITE] == tmp_data.wgt[G_WHITE]);
    assert(data.m_moves5_count == 0);
  }
  while (next(p));
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

  //Окончание игры при отсутствии свободных ячеек
  TestGomoku g(4, 4);
  GPoint move(0, 0);
  do
  {
    assert(!g.isGameOver());
    g.doMove(move);
    assert(!g.getLine5());
  }
  while (g.next(move));
  assert(g.isGameOver());
}

void TestGomoku::testMoves5()
{
  auto& moves5 = m_line5_moves[G_BLACK];
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
  const auto& last_move_data = get({9, 7});
  //В порождающем ходе шахи фиксируются парами
  assert(last_move_data.m_moves4.size() == 6);
  const auto& move4_data_57 = danger_moves[{5, 7}];
  assert(move4_data_57 && move4_data_57->m_moves5.cells().size() == 1 && !move4_data_57->m_moves5.isEmptyCell({6, 7}));
  const auto& move4_data_67 = danger_moves[{6, 7}];
  assert(move4_data_67 && move4_data_67->m_moves5.cells().size() == 2 && !move4_data_67->m_moves5.isEmptyCell({5, 7}) && !move4_data_67->m_moves5.isEmptyCell({10, 7}));

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
  assert(last_move_data.m_moves4.empty());

  //В ситуации ххХ__х нужно избежать дублирования одной и той же пары ходов 4
  doMove(12, 7, G_BLACK);
  doMove(9, 7, G_BLACK);
  assert(last_move_data.m_moves4.size() == 6);

  //Один и тот же ход 4 может быть добавлен разными порождающими ходами с разными парными ходами
  doMove(6, 8, G_BLACK);
  doMove(6, 10, G_BLACK);
  doMove(6, 6, G_BLACK);
  assert(move4_data_67 && move4_data_67->m_moves5.cells().size() == 3 && !move4_data_67->m_moves5.isEmptyCell({6, 9}));
  assert(get({6, 6}).m_moves4.size() == 2);
  //При откате этот ход 4 не пропадает, но пропадает один связанный с ним ход 5
  undo();
  assert(move4_data_67 && move4_data_67->m_moves5.cells().size() == 2 && move4_data_67->m_moves5.isEmptyCell({6, 9}));
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
    assert(danger_move_data && !danger_move_data->m_open3);
  }
  //Ххх__х
  assert(isOpen3(G_BLACK, 6, 7));
  //хх_Хх - шах, но не открытая тройка
  {
    const auto& danger_move_data = danger_moves.get({10, 7});
    assert(danger_move_data && !danger_move_data->m_open3);
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
    assert(danger_move_data && !danger_move_data->m_open3);
  }
  //х_хХх - шах, но не открытая тройка
  {
    const auto& danger_move_data = danger_moves.get({8, 8});
    assert(danger_move_data && !danger_move_data->m_open3);
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
  assert(open3_moves.size() == 2 && open3_moves[0] != GPoint(9, 5) && open3_moves[1] != GPoint(9, 5));
  //При откате последнего хода возможность открытой тройки сохраняется
  undo();
  assert(!danger_moves.isEmptyCell({9, 5}));
}

using TestFunc = void (TestGomoku::*)();

void gtest(const char* name, TestFunc f, uint count = 1)
{
  std::cout << name << std::endl;
  TestGomoku g;
  for (; count > 0; --count)
    (g.*f)();
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
}
