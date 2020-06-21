#include "igomoku.h"
#include "../src/gomoku.h"
#include "../src/gline.h"
#include "../src/grandom.h"
#include <iostream>

//namespace nsg
//{

//extern bool adjustLineStart(GPoint& start, const GVector& v1, int lineLen, int width, int height);

//extern GVector randomV1();

//}

using namespace nsg;

class GTestGrid : public GPointStack
{
public:
  GTestGrid() : GPointStack(DEF_GRID_WIDTH, DEF_GRID_HEIGHT)
  {
    std::cout << "GTestGrid default constructor" << std::endl;
  }
  ~GTestGrid()
  {
    std::cout << "GTestGrid destructor" << std::endl;
  }
  GTestGrid(const GTestGrid& tg) : GPointStack(tg)
  {
    std::cout << "GTestGrid copy constructor" << std::endl;
  }
  GTestGrid& operator=(const GTestGrid& tg)
  {
    std::cout << "GTestGrid copy operator" << std::endl;
    GPointStack::operator=(tg);
    return *this;
  }
};

class GTestGridMovable : public GPointStack
{
public:
  GTestGridMovable() : GPointStack(DEF_GRID_WIDTH, DEF_GRID_HEIGHT)
  {
    std::cout << "GTestGridMovable default constructor" << std::endl;
  }
  ~GTestGridMovable()
  {
    std::cout << "GTestGridMovable destructor" << std::endl;
  }
  GTestGridMovable(const GTestGridMovable& tg) : GPointStack(tg)
  {
    std::cout << "GTestGridMovable copy constructor" << std::endl;
  }
  GTestGridMovable& operator=(const GTestGridMovable& tg)
  {
    std::cout << "GTestGridMovable copy operator" << std::endl;
    GPointStack::operator=(tg);
    return *this;
  }
  GTestGridMovable(GTestGridMovable&& tg) : GPointStack(std::move(tg))
  {
    std::cout << "GTestGridMovable move constructor" << std::endl;
  }
  GTestGridMovable& operator=(GTestGridMovable&& tg)
  {
    std::cout << "GTestGridMovable move operator" << std::endl;
    GPointStack::operator=(std::move(tg));
    return *this;
  }
};

GTestGrid makeGrid(const GBaseStack& stack)
{
  GTestGrid res;
  for (const GPoint& p: stack)
    res[p] = true;
  return res;
}

GTestGridMovable makeGridMovable(const GBaseStack& stack)
{
  GTestGridMovable res;
  for (const GPoint& p: stack)
    res[p] = true;
  return res;
}

void makeGrid(const GBaseStack& stack, GTestGrid& grid)
{
  grid.clear();
  for (const GPoint& p: stack)
    grid[p] = true;
}

//По результатам этого теста видно, что RVO работает не во всех случаях,
//поэтому передача целевого объекта по ссылке хотя и менее удобна, но в общем случае более эффективна,
//чем возврат по значению
void testRVO()
{
  GStack<8> stack;
  stack.push() = {7, 7};
  stack.push() = {8, 7};

  //Вызов с полной оптимизацией (компилятор не создает локальный объект res)
  std::cout << "GTestGrid grid = makeGrid(stack);" << std::endl;
  GTestGrid grid = makeGrid(stack);
  std::cout << std::endl;

  //Вызов с созданием и уничтожением локального объекта res и однократным срабатыванием оператора копирования
  //При создании и уничтожении локального объекта res выделяется и освобождается память
  //При копировании выполняется также очистка целевого объекта
  //(оптимизация не работает)
  std::cout << "grid = makeGrid(stack);" << std::endl;
  grid = makeGrid(stack);
  std::cout << std::endl;

  //Вызов с полной оптимизацией (компилятор не создает локальный объект res)
  std::cout << "GTestGridMovable gridm = makeGridMovable(stack);" << std::endl;
  GTestGridMovable gridm = makeGridMovable(stack);
  std::cout << std::endl;

  //Вызов с созданием и уничтожением локального объекта res и однократным срабатыванием оператора перемещения
  //(компилятор вызывает оператор перемещения вместо оператора копирования)
  //При создании и уничтожении локального объекта res выделяется и освобождается память
  std::cout << "gridm = makeGridMovable(stack);" << std::endl;
  gridm = makeGridMovable(stack);
  std::cout << std::endl;

  //Передача по ссылке и очистка целевого объекта внутри функции (эффективно во всех случаях)
  std::cout << "makeGrid(stack, grid);" << std::endl;
  makeGrid(stack, grid);
  std::cout << std::endl;
}

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

  void testHintMove5();

  void testHintVictoryMove4Chain();

  void testHintBestDefense();

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
  assert(m_moves5[G_BLACK].cells().empty());
  assert(m_moves5[G_WHITE].cells().empty());
  GPoint p(0, 0);
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

//  doMove(6, 7, G_BLACK);
//  //Ход (4, 7) является шахом, но он не фиксируется как шах,
//  //поскольку парный с ним ход (5, 7) является ходом 5
//  assert(!isMove4(G_BLACK, 4, 7));
//  assert(get({6, 7}).m_moves4.empty());

//  undo();
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
  assert(move4_data_67 && move4_data_67->m_moves5.cells().size() == 3 && !move4_data_67->m_moves5.isEmptyCell({6, 9}));
  assert(get({6, 6}).m_moves4.size() == 2);
  //При откате этот ход 4 не пропадает, но пропадает один связанный с ним ход 5
  undo();
  assert(move4_data_67 && move4_data_67->m_moves5.cells().size() == 2 && move4_data_67->m_moves5.isEmptyCell({6, 9}));

  doMove(5, 7);
  //Ход (6, 7) является ходом 5. При этом он не должен фиксироваться как ход 4

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

void TestGomoku::testHintMove5()
{
  GPoint move5;

  doMove(7, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  doMove(6, 7, G_BLACK);
  assert(!hintMove5(G_BLACK, move5) && !hintBlock5(G_WHITE, move5));

  doMove(5, 7, G_WHITE);
  doMove(9, 7, G_BLACK);
  assert(hintMove5(G_BLACK, move5) && move5 == GPoint(10, 7));
  assert(hintBlock5(G_WHITE, move5) && move5 == GPoint(10, 7));

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
  assert(hintShortestVictoryMove4Chain(G_BLACK, move4, 0));
  assert(move4 == GPoint(5, 7) || move4 == GPoint(9, 7));

  GStack<32> defense_variants;
  assert(findVictoryMove4Chain(G_BLACK, {9, 7}, 0, &defense_variants));
  auto grid = makeGrid(defense_variants);
  assert(grid.cells().size() == 3 && !grid.isEmptyCell({9, 7}) && !grid.isEmptyCell({5, 7}) && !grid.isEmptyCell({10, 7}));
  defense_variants.clear();
  assert(findVictoryMove4Chain(G_BLACK, {5, 7}, 0, &defense_variants));
  makeGrid(defense_variants, grid);
  assert(grid.cells().size() == 3 && !grid.isEmptyCell({9, 7}) && !grid.isEmptyCell({5, 7}) && !grid.isEmptyCell({4, 7}));

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
  assert(defense_variants.size() == 1 && defense_variants[0] == GPoint(9, 7));

  start();
  doMove(7, 7, G_BLACK);
  doMove(8, 7, G_BLACK);
  doMove(6, 7, G_BLACK);
  doMove(9, 7, G_WHITE);
  doMove(5, 5, G_BLACK);
  doMove(5, 4, G_BLACK);
  //Мат в два хода
  defense_variants.clear();
  assert(hintShortestVictoryMove4Chain(G_BLACK, move4, 1, &defense_variants));
  assert(move4 == GPoint(5, 7));
  assert(defense_variants.size() == 5);
  makeGrid(defense_variants, grid);
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
  assert(hintShortestVictoryMove4Chain(G_BLACK, move4, 1, &defense_variants));
  assert(move4 == GPoint(5, 7));
  //Защитные ходы могут дублироваться
  assert(defense_variants.size() >= 9);
  makeGrid(defense_variants, grid);
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
  assert(!hintShortestVictoryMove4Chain(G_BLACK, move4, 1));

  doMove(3, 7, G_BLACK);
  doMove(2, 8, G_BLACK);
  doMove(6, 4, G_WHITE);
  //Ответ на контршах также является контршахом,
  //поэтому здесь мат в три хода
  assert(!hintShortestVictoryMove4Chain(G_BLACK, move4, 1));
  defense_variants.clear();
  assert(hintShortestVictoryMove4Chain(G_BLACK, move4, 2, &defense_variants));
  assert(move4 == GPoint(5, 7));
  assert(defense_variants.size() == 7);
  makeGrid(defense_variants, grid);
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
  assert(!hintShortestVictoryMove4Chain(G_BLACK, move4, 2));
}

void TestGomoku::testHintBestDefense()
{
  doMove(7, 7, G_BLACK);
  doMove(8, 8, G_BLACK);
  doMove(9, 9, G_BLACK);

  GStack<DEF_CELL_COUNT> defense_variants;
  defense_variants.push() = {5, 5};
  defense_variants.push() = {10, 10};
  defense_variants.push() = {6, 6};

  //Полушах контрится смежными ходами
  assert(calcMaxDefenseWgt(G_WHITE, defense_variants, 0, 0) > WGT_DEFEAT);
  GPoint best_defense = hintBestDefense(G_WHITE, {-1, -1}, defense_variants, 0, 0);
  assert(best_defense == GPoint(6, 6) || best_defense == GPoint(10, 10));

  doMove(6, 8, G_BLACK);
  doMove(4, 10, G_BLACK);
  //Вилка 3х3 не контрится
  assert(calcMaxDefenseWgt(G_WHITE, defense_variants, 0, 0) == WGT_DEFEAT);
  assert(hintBestDefense(G_WHITE, {-1, -1}, defense_variants, 0, 0) == GPoint(-1, -1));

  doMove(5, 7, G_WHITE);
  doMove(5, 8, G_WHITE);
  doMove(5, 10, G_WHITE);
  doMove(5, 6, G_BLACK);
  //Нейтрализуем вилку контршахом
  //Контршахи не рассматриваются как контршахи на глубине 0 при уровне сложности 0
  assert(calcMaxDefenseWgt(G_WHITE, defense_variants, 0, 0) == WGT_DEFEAT);
  //На глубине > 0 контршахи рассматриваются как контршахи
  assert(calcMaxDefenseWgt(G_WHITE, defense_variants, 1, 0) > WGT_DEFEAT);
  assert(hintBestDefense(G_WHITE, {-1, -1}, defense_variants, 1, 0) == GPoint(5, 9));

  undo();
  undo();
  undo();
  undo();
  undo();
  undo();
  doMove(7, 10, G_WHITE);
  doMove(8, 10, G_WHITE);
  doMove(11, 10, G_WHITE);
  doMove(11, 9, G_BLACK);
  doMove(13, 7, G_BLACK);
  doMove(9, 11, G_BLACK);
  //Если защитный вариант (в нашем случае 10, 10)является контршахом,
  //то он рассматривается как защитный вариант
  //независимо от глубины и уровня сложности
  assert(isDangerMove4(G_WHITE, {10, 10}));
  assert(calcMaxDefenseWgt(G_WHITE, defense_variants, 0, 0) > WGT_DEFEAT);
  assert(hintBestDefense(G_WHITE, {-1, -1}, defense_variants, 0, 0) == GPoint(10, 10));
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
  gtest("testRVO", testRVO);
  gtest("testDoMove", &TestGomoku::testDoMove);
  gtest("testUndo", &TestGomoku::testUndo);
  gtest("testIsGameOver", &TestGomoku::testIsGameOver);
  gtest("testMoves5", &TestGomoku::testMoves5);
  gtest("testMoves4", &TestGomoku::testMoves4);
  gtest("testOpen3", &TestGomoku::testOpen3);
  gtest("testHintMove5", &TestGomoku::testHintMove5);
  gtest("testHintVictoryMove4Chain", &TestGomoku::testHintVictoryMove4Chain);
  gtest("testHintBestDefense", &TestGomoku::testHintBestDefense);
}
