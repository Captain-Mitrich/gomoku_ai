#include "gomoku.h"
#include "grandom.h"

namespace nsg
{

template<>
const bool TCleaner<bool>::empty_val = false;

GomokuPtr CreateGomoku()
{
  return std::make_unique<Gomoku>();
}

GVector randomV1()
{
  GVector v1;
  do
  {
    v1 = {random(-1, 3), random(-1, 3)};
  }
  while (v1 == GVector(0, 0));
  return v1;
}

void getStartPoint(const GLine& line, int& x, int& y)
{
  x = line.start.x;
  y = line.start.y;
}

void getNextPoint(const GLine& line, int& x, int& y)
{
  x += line.v1.x;
  y += line.v1.y;
}

const GVector Gomoku::vecs1[] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}};

Gomoku::Gomoku(int _width, int _height) :
  GGrid(_width, _height),
  m_ai_level(0),
  m_line5({-1, -1}, {0, 0}),
  m_line5_moves { {_width, _height}, {_width, _height} }
{
  assert(_width >= 5 && _height >= 5);

  initMovesWgt();
}

void Gomoku::start()
{
  int x, y;
  while (undo(x, y));
}

bool Gomoku::isValidNextMove(int x, int y) const
{
  GPoint p(x, y);
  return isValidCell(p) && isEmptyCell(p);
}

bool Gomoku::doMove(int x, int y)
{
  return doMove(x, y, curPlayer());
}

bool Gomoku::doMove(int x, int y, GPlayer player)
{
  return doMove({x, y}, player);
}

bool Gomoku::doMove(const GPoint& move)
{
  return doMove(move.x, move.y);
}

bool Gomoku::doMove(const GPoint& move, GPlayer player)
{
  if (isGameOver() || !isValidCell(move) || !isEmptyCell(move))
    return false;

  push(move).player = player;

  if (isLine5Move(player, move))
    buildLine5();
  else
    updateRelatedMovesState();

  return true;
}

bool Gomoku::undo(int& x, int& y)
{
  if (cells().empty())
    return false;
  x = lastCell().x;
  y = lastCell().y;
  undoImpl();
  return true;
}

bool Gomoku::undo(GPoint& move)
{
  return undo(move.x, move.y);
}

bool Gomoku::undo()
{
  if (cells().empty())
    return false;
  undoImpl();
  return true;
}

bool Gomoku::hint(int &x, int &y)
{
  return hint(x, y, curPlayer());
}

bool Gomoku::hint(int &x, int &y, GPlayer player)
{
  if (isGameOver())
    return false;

  GPoint p = hintImpl(player);
  x = p.x;
  y = p.y;

  return true;
}

uint Gomoku::getAiLevel() const
{
  return m_ai_level;
}

void Gomoku::setAiLevel(uint ai_level)
{
  m_ai_level = (ai_level > getMaxAiLevel()) ? getMaxAiLevel() : ai_level;
}

GPoint Gomoku::hintImpl(GPlayer player)
{
  assert(!isGameOver());

  if (cells().empty())
    return {width() / 2, height() / 2};

  GPoint point;

  //второй ход
  if (cells().size() == 1)
    return hintSecondMove();

  //Финальный ход
  if (hintLine5(player, point))
    return point;

  //Блокировка финального хода противника
  if (hintLine5Block(player, point))
    return point;

  return hintMaxWgt(player, 2);
}

GPoint Gomoku::hintSecondMove() const
{
  assert(cells().size() == 1);

  const GPoint& first_move = cells().front();

  GPoint center(width() / 2, height() / 2);

  //если первый ход в центре, делаем случайный ход рядом
  if (first_move == center)
    return first_move + randomV1();

  //если первый ход у края, делаем ход в центре
  if (first_move.x == 0 || first_move.x == width() - 1 ||
      first_move.y == 0 || first_move.y == height() - 1)
  {
    assert(first_move != center);
    return center;
  }

  //делаем второй ход рядом с первым ближе к центру
  GPoint res = first_move;
  if (first_move.x - center.x > 0)
    --res.x;
  else if (first_move.x - center.x < 0)
    ++res.x;
  if (first_move.y - center.y > 0)
    --res.y;
  else if (first_move.y - center.y < 0)
    ++res.y;

  return res;
}

bool Gomoku::hintLine5(GPlayer player, GPoint &point) const
{
  const auto& line5Moves = getLine5Moves(player).cells();
  for (const auto& move: line5Moves)
  {
    if (!isEmptyCell(move))
      continue;
    point = move;
    return true;
  }
  return false;
}

bool Gomoku::hintLine5Block(GPlayer player, GPoint &point) const
{
  return hintLine5(!player, point);
}

GPoint Gomoku::hintMaxWgt(GPlayer player, uint depth)
{
  std::vector<GPoint> max_wgt_moves;
  max_wgt_moves.reserve(width() * height() - cells().size());

  int max_wgt = WGT_DEFEAT;

  auto wgtHandler = [&](const GPoint& move, int wgt)
  {
    if (wgt < max_wgt)
      return;
    if (wgt > max_wgt)
    {
      max_wgt = wgt;
      max_wgt_moves.clear();
    }
    max_wgt_moves.push_back(move);
  };

  GPoint move;
  if (findVictoryMove(player, move, depth, wgtHandler))
    return move;

  if (max_wgt == WGT_DEFEAT)
  {
    //Все ходы проигрышные
    //Перекрываем лучший выигрышный ход противника
    return hintMaxWgt(!player, 1);
    /*hintBestVictoryMove(!player, move);
    assert(isValidCell(move) && isEmptyCell(move));
    return move;*/
  }

  assert(!max_wgt_moves.empty());
  return max_wgt_moves[random(0, max_wgt_moves.size())];
}

/*void Gomoku::hintBestVictoryMove(GPlayer player, GPoint& vmove)
{
  uint depth;
  auto wgtHandler = [](const GPoint& , int)
  {
  };

  GPoint move;
  for (depth = 1; ; depth += 2)
  {
    //сначала проверим заданный ход (если задан) - у него больше шансов
    if (isValidCell(vmove) && isEmptyCell(vmove) && calcWgt(player, vmove, depth) == WGT_VICTORY)
      return;
    if (!findVictoryMove(player, move, depth, wgtHandler))
      continue;
    vmove = move;
    return;
  }
}*/

int Gomoku::calcMaxWgt(uint depth)
{
  int max_wgt = WGT_DEFEAT;

  auto wgtHandler = [&](const GPoint& , int wgt)
  {
    if (wgt > max_wgt)
      max_wgt = wgt;
  };

  GPoint move;
  findVictoryMove(curPlayer(), move, depth, wgtHandler);
  return max_wgt;
}

int Gomoku::calcWgt(GPlayer player, const GPoint &move, uint depth)
{
  assert(isEmptyCell(move));

  int wgt = get(move).wgt[player];

  if (depth == 0)
    return wgt;

  GMoveData& move_data = push(move);
  move_data.player = player;
  updateRelatedMovesState();

  if (move_data.line5_moves_count > 1)
    wgt = WGT_VICTORY;
  else
  {
    //Если у игрока линия 4, то у противника единственный вариант - блокировать ее
    //Иначе ищем максимальный вес противника
    int enemy_wgt =
      (move_data.line5_moves_count == 1) ?
      calcWgt(!player, getLine5Moves(player).lastCell(), depth - 1) :
      calcMaxWgt(depth - 1);

    //если у противника выигрыш, возвращаем проигрыш
    if (enemy_wgt == WGT_VICTORY)
      wgt = WGT_DEFEAT;
    //и наоборот
    else if (enemy_wgt == WGT_DEFEAT)
      wgt = WGT_VICTORY;
    else
      //для вычисления веса лучшей последовательности ходов
      //из веса головы последовательности вычитается вес хвоста последовательности
      wgt -= enemy_wgt;
  }

  restoreRelatedMovesState();
  pop();

  return wgt;
}

bool Gomoku::isGameOver() const
{
  return getLine5() || cells().size() == width() * height();
}

const GLine* Gomoku::getLine5() const
{
  return isValidCell(m_line5.start) ? &m_line5 : 0;
}

const GPointStack& Gomoku::getLine5Moves(GPlayer player) const
{
  return m_line5_moves[player];
}

void Gomoku::undoImpl()
{
  if (isGameOver())
    undoLine5();
  else
    restoreRelatedMovesState();
  pop();
}

GPlayer Gomoku::lastMovePlayer() const
{
  assert(!cells().empty());
  return get(lastCell()).player;
}

GPlayer Gomoku::curPlayer() const
{
  return cells().empty() ? G_BLACK : !lastMovePlayer();
}

void Gomoku::addLine5Move(GPlayer player, const GPoint& p)
{
  m_line5_moves[player].push(p) = true;
}

void Gomoku::removeLine5Move(GPlayer player/*, const GPoint& p*/)
{
  assert(!m_line5_moves[player].cells().empty());
  m_line5_moves[player].pop();
}

bool Gomoku::isLine5Move(GPlayer player, const GPoint& move) const
{
  return !m_line5_moves[player].isEmptyCell(move);
}

bool Gomoku::buildLine5()
{
  for (int i = 0; i < 4; ++i)
  {
    if (buildLine5(vecs1[i]))
      return true;
  }

  return false;
}

bool Gomoku::buildLine5(const GVector& v1)
{
  GPoint center = lastCell();
  GPlayer player = get(center).player;
  assert(player != G_EMPTY);

  GPoint p = center;

  int i;
  for (i = 1; i < 5; ++i)
  {
    p += v1;
    if (!isValidCell(p) || get(p).player != player)
      break;
  }

  p = center;

  for (; i < 5; ++i)
  {
    p -= v1;
    if (!isValidCell(p) || get(p).player != player)
      return false;
  }

  m_line5.start = p;
  m_line5.v1 = v1;
  return true;
}

void Gomoku::undoLine5()
{
  m_line5 = {{-1, -1}, {0, 0}};
}

bool adjustLineStart(GPoint& start, const GVector& v1, int lineLen, int width, int height)
{
  GVector v = v1 * lineLen;
  while (!BaseGrid::isValidCell(start + v, width, height))
    start -= v1;
  return BaseGrid::isValidCell(start, width, height);
}

bool Gomoku::adjustLineStart(GPoint &start, const GVector &v1, int lineLen) const
{
  return nsg::adjustLineStart(start, v1, lineLen, width(), height());
}

void Gomoku::initMovesWgt()
{
  GPoint lineStart(0, 0);
  do
  {
    for (int i = 0; i < 4; ++i)
      initWgt({lineStart, vecs1[i]});
  }
  while (next(lineStart));
}

void Gomoku::initWgt(const GLine& line5)
{
  GPoint p = line5.start;
  //учитываем только линии, которые полностью лежат в поле
  if (!isValidCell(p + line5.v1 * 4))
    return;
  for (int i = 0; ; ++i)
  {
    addWgt(p, G_BLACK, 1);
    addWgt(p, G_WHITE, 1);
    if (i == 4)
      break;
    p += line5.v1;
    if (!isValidCell(p))
      break;
  }
}

void Gomoku::addWgt(const GPoint &point, GPlayer player, int wgt)
{
  ref(point).wgt[player] += wgt;
}

void Gomoku::updateRelatedMovesState()
{
  uint related_move_iter = 0;

  for (int i = 0; i < 4; ++i)
  {
    backupRelatedMovesState(vecs1[i], related_move_iter);
    backupRelatedMovesState(-vecs1[i], related_move_iter);
    updateRelatedMovesState(vecs1[i]);
  }
}

void Gomoku::updateRelatedMovesState(const GVector& v1)
{
  const GPoint& last_move = lastCell();
  GMoveData& last_move_data = ref(last_move);
  GPlayer player = last_move_data.player;

  uint counts[2] = {0, 0};

  GVector v4 = v1 * 4;
  GPoint ep = last_move + v4;
  while (!isValidCell(ep))
    ep -= v1;

  GPoint bp = ep - v4;
  if (!isValidCell(bp))
    return;

  for (; ; ep -= v1)
  {
    assert(isValidCell(ep));
    if (!isEmptyCell(ep))
      ++counts[get(ep).player];
    if (ep == bp)
      break;
  }

  ep = last_move - v4;
  while (!isValidCell(ep))
    ep += v1;

  int playerWgtDelta, enemyWgtDelta;

  for (; ; )
  {
    assert(isValidCell(bp));

    assert(counts[player] > 0 && counts[player] < 5);
    assert(counts[!player] < 5);
    assert(counts[player] + counts[!player] <= 5);

    //рассматриваем линию 5 от bp в направлении v1
    //определим изменение веса ходов обоих игроков в свободные ячейки этой линии
    //для этого определим вклад линии в вес кандидатов до и после последнего хода и вычтем старую составляющую веса из новой
    //вклад линии в вес кандидата равен изменению веса линии в результате реализации кандидата
    //это не нужно делать, если до последнего хода линия уже содержала ходы обоих игроков,
    //поскольку в этом случае она заведомо не могла стать выигрышной
    //также это не нужно делать, если последний ход заблокировал линию 4 противника, поскольку в этом случае линия оказывается заполненной,
    //и не остается кандидатов, вес которых можно было бы изменить
    if (counts[player] == 1 && counts[!player] < 4 || counts[!player] == 0)
    {
      if (counts[!player] == 0)
      {
        //последний ход развил линию игрока
        if (counts[player] == 1)
        {
          //оставшиеся кандидаты игрока теперь развивают линию длины 1
          playerWgtDelta = getFurtherMoveWgt(1) - getFirstLineMoveWgt();
          //кандидаты противника раньше начинали новую линию, а теперь блокируют линию игрока длиной 1
          enemyWgtDelta = getBlockingMoveWgt(1) - getFirstLineMoveWgt();
        }
        else
        {
          //оставшиеся кандидаты игрока теперь развивают линию большей длины
          playerWgtDelta = getFurtherMoveWgt(counts[player]) - getFurtherMoveWgt(counts[player] - 1);
          //кандидаты противника теперь блокируют линию игрока большей длины
          enemyWgtDelta = getBlockingMoveWgt(counts[player]) - getBlockingMoveWgt(counts[player] - 1);
        }
      }
      else //counts[player] == 1
      {
        //последний ход заблокировал линию противника
        //оставшиеся кандидаты игрока больше не блокируют линию противника
        playerWgtDelta = - getBlockingMoveWgt(counts[!player]);
        //кандидаты противника больше не развивают свою линию
        enemyWgtDelta = - getFurtherMoveWgt(counts[!player]);
      }

      if (counts[player] == 4 || playerWgtDelta != 0 || enemyWgtDelta != 0)
      {
        //фиксируем изменения во всех пустых ячейках линии
        uint empty_count = 5 - counts[0] - counts[1];
        assert(empty_count > 0);
        for (GPoint p = bp; ; p += v1)
        {
          if (!isEmptyCell(p))
            continue;

          if (counts[player] == 4)
          {
            //если мы рассматриваем линию 4 и ее пустая ячейка уже
            //зафиксирована как завершающий ход другой линии,
            //то рассматриваемая линия уже ничего не меняет
            if (isLine5Move(player, p))
              break;
            addLine5Move(player, p);
            last_move_data.addLine5Move(p);
          }

          addWgt(p, player, playerWgtDelta);
          addWgt(p, !player, enemyWgtDelta);

          if (--empty_count == 0)
            break;
        }
      }
    }

    if (bp == ep)
      break;
    if (!isEmptyCell(bp + v4))
      --counts[get(bp + v4).player];
    bp -= v1;
    if (!isEmptyCell(bp))
      ++counts[get(bp).player];
  }
}

void Gomoku::backupRelatedMovesState(const GVector& v1, uint& related_moves_iter)
{
  GPoint p = lastCell();
  GMoveData& last_move_data = ref(p);

  for (uint i = 0; ; ++i)
  {
    p += v1;
    if (!isValidCell(p))
      break;
    if (isEmptyCell(p))
    {
      assert(related_moves_iter < GStateBackup::RELATED_MOVES_COUNT);
      last_move_data.related_moves_wgt_backup[related_moves_iter] = get(p).wgt;
      ++related_moves_iter;
    }
    if (i == 3)
      break;
  }
}

void Gomoku::restoreRelatedMovesState()
{
  uint related_moves_iter = 0;

  for (int i = 0; i < 4; ++i)
  {
    restoreRelatedMovesState(vecs1[i], related_moves_iter);
    restoreRelatedMovesState(-vecs1[i], related_moves_iter);
  }
}

void Gomoku::restoreRelatedMovesState(const GVector& v1, uint& related_moves_iter)
{
  GPoint p = lastCell();
  GMoveData& last_move_data = ref(p);

  for (; last_move_data.line5_moves_count > 0; --last_move_data.line5_moves_count)
    m_line5_moves[last_move_data.player].pop();

  for (uint i = 0; ; ++i)
  {
    p += v1;
    if (!isValidCell(p))
      break;
    if (isEmptyCell(p))
    {
      assert(related_moves_iter < GStateBackup::RELATED_MOVES_COUNT);
      ref(p).wgt = last_move_data.related_moves_wgt_backup[related_moves_iter];
      ++related_moves_iter;
    }
    if (i == 3)
      break;
  }
}

int Gomoku::getFirstLineMoveWgt()
{
  return getLineWgt(1);
}

int Gomoku::getFurtherMoveWgt(uint line_len)
{
  assert(line_len > 0 && line_len < 5);
  return getLineWgt(line_len + 1) - getLineWgt(line_len);
}

int Gomoku::getBlockingMoveWgt(uint line_len)
{
  assert(line_len > 0 && line_len < 5);
  return getLineWgt(line_len);
}

int Gomoku::getLineWgt(uint line_len)
{
  assert(line_len > 0 && line_len <= 5);
  return line_len * 2 - 1;
}

bool Gomoku::next(GPoint& p) const
{
  assert(isValidCell(p));
  if (++p.x == width())
  {
    if (++p.y == height())
      return false;
    p.x = 0;
  }
  return true;
}

} //namespace nsg
