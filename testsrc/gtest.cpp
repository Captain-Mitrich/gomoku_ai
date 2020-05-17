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

GPoint randomPoint(uint width = 15, uint height = 15)
{
  return {random(0, width), random(0, height)};
}

std::vector<GPoint> getLineMoves(const GLine& line, uint len)
{
  std::vector<GPoint> vec(len);
  GPoint point = line.start;
  for (uint i = 0; i < len; point += line.v1, ++i)
    vec[i] = point;
  return vec;
}

std::list<GPoint> getShuffledLineMoves(const GLine& line, uint len)
{
  auto moves = getLineMoves(line, len);
  shuffle(moves.begin(), moves.end());
  std::list<GPoint> mlist;
  for (const auto& move: moves)
    mlist.push_back(move);
  return mlist;
}

void printMoves(const Gomoku& g)
{
  /*for (const auto& move: g.cells())
  {
    GPlayer player = g.get(move).player;
    assert(player == G_BLACK || player == G_WHITE);
    std::cout << move.x << '/' << move.y << ((player == G_BLACK) ? 'b' : 'w') << "  ";
  }
  std::cout << std::endl << std::endl;*/
}

bool doMove(Gomoku& gomoku, const GPoint& point, GPlayer player)
{
  bool res = gomoku.doMove(point, player);
  if (res)
    printMoves(gomoku);
  return res;
}

bool undo(Gomoku& gomoku)
{
  bool res = gomoku.undo();
  if (res)
    printMoves(gomoku);
  return res;
}

GPoint hint(Gomoku& gomoku, GPlayer player)
{
  GPoint p;
  assert(gomoku.hint(p.x, p.y, player));
  return p;
}

using TestFunc = void ();

void gtest(const char* name, TestFunc f, uint count = 1)
{
  std::cout << name << std::endl;
  for (; count > 0; --count)
    f();
  std::cout << "OK" << std::endl << std::endl;
}

class TestGomoku : public Gomoku
{
public:
  TestGomoku(int _width = 15, int _height = 15) : Gomoku(_width, _height)
  {}

  const GMoveData& get(const GPoint& move) const
  {
    return Gomoku::get(move);
  }

  int width() const
  {
    return Gomoku::width();
  }

  int height() const
  {
    return Gomoku::height();
  }

  const std::vector<GPoint>& cells() const
  {
    return Gomoku::cells();
  }

  bool isValidCell(const GPoint& cell)
  {
    return Gomoku::isValidCell(cell);
  }

  bool isEmptyCell(const GPoint& cell) const
  {
    return Gomoku::isEmptyCell(cell);
  }

  const GPointStack& getLine5Moves(GPlayer player) const
  {
    return Gomoku::getLine5Moves(player);
  }

  bool hintLine5(GPlayer player, GPoint& point) const
  {
    return Gomoku::hintLine5(player, point);
  }

  bool hintLine5Block(GPlayer player, GPoint& point) const
  {
    return Gomoku::hintLine5Block(player, point);
  }

  GPoint hintImpl(GPlayer player)
  {
    return Gomoku::hintImpl(player);
  }

  GPoint hintMaxWgt(GPlayer player, int depth)
  {
    return Gomoku::hintMaxWgt(player, depth);
  }

  int calcMaxWgt(GPlayer player, GPoint& max_wgt_move, int depth)
  {
    int max_wgt = WGT_DEFEAT;

    auto wgtHandler = [&](const GPoint& move, int wgt)
    {
      if (wgt > max_wgt)
      {
        max_wgt = wgt;
        max_wgt_move = move;
      }
    };

    GPoint move;
    findVictoryMove(player, move, depth, wgtHandler);

    return max_wgt;
  }

  int calcWgt(GPlayer player, const GPoint& move, int depth)
  {
    return Gomoku::calcWgt(player, move, depth);
  }

  bool next(GPoint& p) const
  {
    return Gomoku::next(p);
  }
};

void testEmpty(const TestGomoku& g)
{
  TestGomoku tmp;

  assert(!g.getLine5());
  assert(g.cells().empty());
  assert(g.getLine5Moves(G_BLACK).cells().empty());
  assert(g.getLine5Moves(G_WHITE).cells().empty());
  GPoint p(0, 0);
  do
  {
    assert(g.isEmptyCell(p));
    assert(g.getLine5Moves(G_BLACK).isEmptyCell(p));
    assert(g.getLine5Moves(G_WHITE).isEmptyCell(p));
    const GMoveData& data = g.get(p);
    const GMoveData& tmp_data = tmp.get(p);
    assert(data.wgt[G_BLACK] == tmp_data.wgt[G_BLACK]);
    assert(data.wgt[G_WHITE] == tmp_data.wgt[G_WHITE]);
    assert(data.line5_moves_count == 0);
  }
  while (g.next(p));
}

//проверяем, что после рестарта игры все данные очищены
void testStartRandom()
{
  TestGomoku g;

  for (int i = 0; i < 10; ++i)
  {
    GPoint bp = randomPoint(), wp = randomPoint();
    GVector bv1 = randomV1(), wv1 = randomV1();

    for (int j = 0; j < 5; ++j, bp += bv1, wp += wv1)
    {
      //не все ходы будут сделаны, но это не страшно
      doMove(g, bp, G_BLACK);
      doMove(g, wp, G_WHITE);
    }

    if (g.isGameOver())
      break;
  }

  g.start();
  testEmpty(g);
}

void testStart()
{
  TestGomoku g;
  doMove(g, {0, 12}, G_BLACK);
  doMove(g, {11, 13}, G_WHITE);
  doMove(g, {12, 14}, G_WHITE);
  doMove(g, {0, 2}, G_BLACK);
  doMove(g, {11, 8}, G_WHITE);
  doMove(g, {1, 2}, G_BLACK);
  doMove(g, {11, 9}, G_WHITE);
  doMove(g, {2, 2}, G_BLACK);
  doMove(g, {11, 10}, G_WHITE);
  doMove(g, {3, 2}, G_BLACK);
  doMove(g, {11, 11}, G_WHITE);
  doMove(g, {4, 2}, G_BLACK);

  g.start();
  testEmpty(g);

  for (int i = 0; i < 20; ++i)
    testStartRandom();
}

//проверка окончания игры после установки линии 5
//и отмены окончания игры после отмены выигрышного хода
void testGameOverLine5(const GPoint& center, const GPoint& v1)
{
  TestGomoku g;

  GPoint lineStart = center;
  if (!adjustLineStart(lineStart, v1, 5, g.width(), g.height()))
    return;

  auto moves = getShuffledLineMoves({lineStart, v1}, 5);
  for (const auto& bmove: moves)
  {
    doMove(g, bmove, G_BLACK);
    if (bmove == moves.back())
    {
      assert(g.isGameOver());
      const GLine* l5 = g.getLine5();
      assert(l5);
      int x5, y5;
      getStartPoint(*l5, x5, y5);
      for (int i = 0; ; ++i, getNextPoint(*l5, x5, y5))
      {
        assert(std::find(moves.begin(), moves.end(), GPoint(x5, y5)) != moves.end());
        if (i == 4)
          break;
      }
    }
    else
    {
      assert(!g.isGameOver());
      GPoint wmove;
      do
      {
        wmove = randomPoint();
      } while (std::find(moves.begin(), moves.end(), wmove) != moves.end());
      doMove(g, wmove, G_WHITE);
      assert(!g.isGameOver());
    }
  }
}

void testGameOverLine5()
{
  testGameOverLine5(randomPoint(), randomV1());
}

void testGameOverFullGrid()
{
  TestGomoku g(5, 5);

  int size = g.width() * g.height();

  //заполняем сетку с установкой финальной линии последним ходом
  //xoxox
  //oxoxo
  //xoxox
  //oxoxo
  //xxoxo
  GPoint move;
  for (move.y = 0; move.y < g.height(); ++move.y)
  {
    for (move.x = (move.y == g.height() - 1) ? 1 : 0; move.x < g.width(); ++move.x)
    {
      assert(g.doMove(move));
      assert(!g.isGameOver());
    }
  }
  assert(g.doMove(0, 4));
  assert(g.isGameOver());

  const GLine* l5 = g.getLine5();
  assert(l5);
  GPoint p;
  getStartPoint(*l5, p.x, p.y);
  for (int i = 0; ; ++i, getNextPoint(*l5, p.x, p.y))
  {
    assert(g.get(p).player == G_BLACK);
    if (i == 4)
      break;
  }
  //откатываем последний ход
  assert(g.undo());
  assert(!g.isGameOver() && !g.getLine5());
  //откатываем еще 3 хода
  for (int i = 0; i < 3; ++i)
  {
    assert(g.undo());
    assert(!g.isGameOver());
  }
  //устанавливаем последние 4 хода так, чтобы избежать линии 5 (oxxxo)
  assert(g.doMove(0, 4));
  assert(g.doMove(2, 4));
  assert(g.doMove(4, 4));
  assert(g.doMove(3, 4));
  assert(g.isGameOver() && !g.getLine5());
}

//тестируем один финальный ход в одной из позиций
//х ххх, хх хх, ххх х
void testLine5Moves_xx_xx()
{
  GPoint center = randomPoint();
  GVector v1 = randomV1();

  TestGomoku g;

  const auto& line5Moves = g.getLine5Moves(G_BLACK);

  GPoint hp;

  GPoint lineStart = center;
  if (!adjustLineStart(lineStart, v1, 5, g.width(), g.height()))
    return;

  auto moves = getShuffledLineMoves({lineStart, v1}, 5);

  GPoint hole = lineStart + v1 * random(1, 3);
  moves.erase(std::find(moves.begin(), moves.end(), hole));
  assert(moves.size() == 4);

  for (const auto& move: moves)
  {
    assert(doMove(g, move, G_BLACK));
    if (move == moves.back())
    {
      assert(line5Moves.cells().size() == 1 && line5Moves.cells().front() == hole);
      assert(hint(g, G_BLACK) == hole);
      assert(hint(g, G_WHITE) == hole);
      break;
    }
    else
      assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
  }

  //блокировка финального хода (позиция ххохх)
  assert(doMove(g, hole, G_WHITE));
  assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));

  //отмена блокировки (позиция хх хх)
  assert(undo(g));
  assert(line5Moves.cells().size() == 1 && line5Moves.cells().front() == hole);
  assert(hint(g, G_BLACK) == hole);
  assert(hint(g, G_WHITE) == hole);

  //отмена позиции хх хх (позиция х  хх или х хх)
  assert(undo(g));
  assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));

  //отсутствие финальных ходов в позиции х охх или хохх
  assert(doMove(g, hole, G_WHITE));
  assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
  //отсутствие финальных ходов в позиции хxохх
  assert(doMove(g, moves.back(), G_BLACK));
  assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
}

//тестируем один финальный ход, завершающий несколько линий 5
//(позиция х хххх или хх ххх)
void testLine5Moves_xxx_xx()
{
  TestGomoku g;
  const auto& line5Moves = g.getLine5Moves(G_BLACK);

  GPoint hp;

  GPoint hole = {7, 7};
  GVector v1 = randomV1();

  uint part1_len = random(1, 2);
  auto part1_moves = getShuffledLineMoves({hole - v1 * part1_len, v1}, part1_len);

  uint part2_len = 4 - part1_len;
  auto part2_moves = getShuffledLineMoves({hole + v1 * 2, v1}, part2_len);

  for (const auto& move: part1_moves)
  {
    assert(doMove(g, move, G_BLACK));
    assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
  }

  for (const auto& move: part2_moves)
  {
    assert(doMove(g, move, G_BLACK));
    assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
  }

  assert(doMove(g, hole + v1, G_BLACK));
  assert(line5Moves.cells().size() > 0 && !line5Moves.isEmptyCell(hole));

  //блокировка финального хода (позиция ххоххx)
  assert(doMove(g, hole, G_WHITE));
  assert(!g.hintLine5(G_BLACK, hp) || hp != hole);
  assert(!g.hintLine5Block(G_WHITE, hp) || hp != hole);

  //отмена блокировки (позиция хх хх)
  assert(undo(g));
  assert(line5Moves.cells().size() > 0 && !line5Moves.isEmptyCell(hole));

  //отмена позиции хх ххх
  assert(undo(g));
  assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
}

//тестируем два финальных хода в позиции хххх
void testLine5Moves_xxxx()
{
  GPoint center = randomPoint();
  GVector v1 = randomV1();

  TestGomoku g;

  const auto& line5Moves = g.getLine5Moves(G_BLACK);

  GPoint hp;

  GPoint lineStart = center;
  if (!adjustLineStart(lineStart, v1, 4, g.width(), g.height()))
    return;

  auto moves = getShuffledLineMoves({lineStart, v1}, 4);

  for (const GPoint& move: moves)
  {
    assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
    assert(doMove(g, move, G_BLACK));
  }

  GPoint hole1(lineStart - v1);
  GPoint hole2(hole1 + v1 * 5);

  if (g.isValidCell(hole1) && g.isValidCell(hole2))
  {
    assert(line5Moves.cells().size() == 2 && !line5Moves.isEmptyCell(hole1) && !line5Moves.isEmptyCell(hole2));
    hp = hint(g, G_BLACK);
    assert(hp == hole1 || hp == hole2);
    hp = hint(g, G_WHITE);
    assert(hp == hole1 || hp == hole2);

    //блокировка одного финального хода (позиция охххх)
    assert(g.doMove(hole1));
    assert(g.hintLine5(G_BLACK, hp) && hp == hole2);
    assert(g.hintLine5Block(G_WHITE, hp) && hp == hole2);

    //блокировка обоих финальных ходов (позиция оххххо)
    assert(doMove(g, hole2, G_WHITE));
    assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));

    //отмена второй блокировки
    assert(undo(g));
    assert(g.hintLine5(G_BLACK, hp) && hp == hole2);
    assert(g.hintLine5Block(G_WHITE, hp) && hp == hole2);

    //отмена первой блокировки
    assert(undo(g));
    assert(g.hintLine5(G_BLACK, hp));
    assert(hp == hole1 || hp == hole2);
    assert(g.hintLine5Block(G_WHITE, hp));
    assert(hp == hole1 || hp == hole2);
  }
  else if (!g.isValidCell(hole1) && !g.isValidCell(hole2))
    //линия примыкает к краю поля с двух сторон (/xxxx\)
    assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
  else
  {
    //линия примыкает к краю поля с одной стороны (xxxx|)
    GPoint& hole = g.isValidCell(hole1) ? hole1 : hole2;
    assert(hint(g, G_BLACK) == hole);
    assert(hint(g, G_WHITE) == hole);

    //блокировка финального хода (позиция охххх)
    assert(g.doMove(hole));
    assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));

    //отмена блокировки
    assert(undo(g));
    assert(hint(g, G_BLACK) == hole);
    assert(hint(g, G_WHITE) == hole);
  }

  //отмена позиции хххх
  assert(undo(g));
  assert(!g.hintLine5(G_BLACK, hp) && !g.hintLine5Block(G_WHITE, hp));
}

void testMaxWgt(int depth)
{
  TestGomoku g;

  assert(g.doMove(7, 7));
  assert(g.doMove(8, 8));
  assert(g.doMove(8, 6));

  GPoint hp = g.hintMaxWgt(G_WHITE, depth);
  assert(hp.x == 6 && hp.y == 8);
}

void testMaxWgtDepth0()
{
  testMaxWgt(0);
}

void testMaxWgtDepth2()
{
  testMaxWgt(2);
}

//Проверяем, что при наличии симметричных кандидатов с максимальным весом
//они предлагаются оба
void testSameMaxWgt()
{
  TestGomoku g;

  assert(doMove(g, {7, 7}, G_BLACK));
  assert(doMove(g, {8, 6}, G_BLACK));

  GPoint hp;
  int count1 = 0, count2 = 0;
  for (int i = 0; i < 100; ++i)
  {
    hp = g.hintMaxWgt(G_WHITE, 0);
    if (hp.x == 9 && hp.y == 5)
      ++count1;
    else if (hp.x == 6 && hp.y == 8)
      ++count2;
    else
      assert(false);
    if (count1 > 0 && count2 > 0)
      return;
  }
  assert(false);
}

void testMaxWgt2()
{
  TestGomoku g;

  g.doMove(7, 7);
  g.doMove(8, 7);
  g.doMove(6, 8);
  g.doMove(5, 9);
  g.doMove(7, 8);
  g.doMove(7, 6);
  g.doMove(6, 5);
  g.doMove(5, 8);
  g.doMove(5, 6);

  GPoint hp = g.hintMaxWgt(G_WHITE, 2);
  assert(hp == GPoint(6, 7) || hp == GPoint(8, 9));

  g.doMove(6, 7);

  hp = g.hintImpl(G_BLACK);
  assert(hp == GPoint(8, 5) || hp == GPoint(4, 9));
}

void testCalcWgt()
{
  TestGomoku g;

  int black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];

  g.doMove(7, 7);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(7, 8);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(6, 6);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(5, 5);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(6, 8);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(6, 7);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(4, 6);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(5, 6);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(4, 5);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(5, 7);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(5, 4);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(5, 9);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];
  g.doMove(5, 8);
  black_3_6_wgt = g.get({3, 6}).wgt[G_BLACK];

  int wgt1, wgt1_0, wgt1_1, wgt1_1_0, wgt1_2;
  int wgt2, wgt2_0, wgt2_1, wgt2_1_0, wgt2_2;
  wgt1 = g.calcWgt(G_WHITE, GPoint(10, 5), 2);
  wgt2 = g.calcWgt(G_WHITE, GPoint(4, 7), 2);

  wgt1_0 = g.get(GPoint(10, 5)).wgt[G_WHITE];
  wgt2_0 = g.get(GPoint(4, 7)).wgt[G_WHITE];

  GPoint max_wgt_move1, max_wgt_move2;

  {
    GMoveMaker gmm1(&g, G_WHITE, {10, 5});
    wgt1_1 = g.calcMaxWgt(G_BLACK, max_wgt_move1, 1);
    wgt1_1_0 = g.get(max_wgt_move1).wgt[G_BLACK];
    assert(wgt1 == wgt1_0 - wgt1_1);
  }

  {
    GMoveMaker gmm2(&g, G_WHITE, {4, 7});
    wgt2_1 = g.calcMaxWgt(G_BLACK, max_wgt_move2, 1);
    wgt2_1_0 = g.get(max_wgt_move2).wgt[G_BLACK];
    assert(wgt2 == wgt2_0 - wgt2_1);
  }

  assert(wgt1 < wgt2);
}

void testHintBestDefense()
{
  TestGomoku g;

  g.setAiLevel(1);

  g.doMove(7, 7);
  g.doMove(8, 6);
  g.doMove(7, 5);
  g.doMove(6, 6);
  g.doMove(7, 6);
  g.doMove(7, 8);
  g.doMove(8, 5);
  g.doMove(6, 5);
  g.doMove(7, 4);
  g.doMove(7, 3);
  g.doMove(6, 7);
  g.doMove(5, 8);
  g.doMove(10, 5);
  g.doMove(9, 4);
  g.doMove(10, 7);
  g.doMove(6, 3);
  g.doMove(9, 8);
  g.doMove(9, 5);

  GPoint hp = g.hintImpl(G_BLACK);
  assert(hp == GPoint(10, 6) || hp == GPoint(6, 2));
}

int main()
{
  gtest("testStart", testStart);
  gtest("testGameOverLine5", testGameOverLine5, 20);
  gtest("testGameOverFullGrid", testGameOverFullGrid);
  gtest("testLine5Moves_xx_xx", testLine5Moves_xx_xx, 20);
  gtest("testLine5Moves_xxxx", testLine5Moves_xxxx, 20);
  gtest("testLine5Moves_xxx_xx", testLine5Moves_xxx_xx, 20);
  gtest("testMaxWgtDepth0", testMaxWgtDepth0, 10);
  gtest("testMaxWgtDepth2", testMaxWgtDepth2);
  gtest("testSameMaxWgt", testSameMaxWgt);
  gtest("testMaxWgt2", testMaxWgt2);
  gtest("testCalcWgt", testCalcWgt);
  gtest("testHintBestDefense", testHintBestDefense);
}
