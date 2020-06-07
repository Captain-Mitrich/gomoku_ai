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
  m_line5_moves { {_width, _height}, {_width, _height} },
  m_danger_moves { {_width, _height}, {_width, _height} }
{
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

  GPoint move;

  //второй ход
  if (cells().size() == 1)
    return hintSecondMove();

  //Финальный ход
  if (hintLine5(player, move))
    return move;

  //Блокировка финального хода противника
  if (hintLine5Block(player, move))
    return move;

  //Выигрышная цепочка шахов
  GPoint move4;
  if (hintShortestVictoryMove4Chain(player, move4, getAiLevel()))
    return move4;

  GStack<DEF_CELL_COUNT> defense_variants;

  //Блокировка выигрышной цепочки шахов противника
  if (hintShortestVictoryMove4Chain(!player, move4, getAiLevel(), &defense_variants))
    return hintBestDefense(player, move4, defense_variants, 2, getAiLevel());

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

bool Gomoku::hintShortestVictoryMove4Chain(
    GPlayer player,
    GPoint &move,
    uint max_depth,
    GStack<DEF_CELL_COUNT> *defense_variants)
{
  for (uint depth = 0; depth <= max_depth; ++depth)
  {
    if (hintVictoryMove4Chain(player, move, depth, defense_variants))
      return true;
    assert(!defense_variants || defense_variants->empty());
  }
  return false;
}

bool Gomoku::hintVictoryMove4Chain(
  GPlayer player,
  GPoint &move,
  uint depth,
  GStack<DEF_CELL_COUNT>* defense_variants
  )
{
  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (calcMove4ChainWgt(player, move4, depth, defense_variants) != WGT_VICTORY)
      continue;

    move = move4;
    return true;
  }

  return false;
}

bool Gomoku::hintShortestVictoryChain(
    GPlayer player,
    GPoint &move,
    uint max_depth)
{
  for (uint depth = 0; depth <= max_depth; ++depth)
  {
    if (hintVictoryChain(player, move, depth))
      return true;
  }
  return false;
}

bool Gomoku::hintVictoryChain(
  GPlayer player,
  GPoint &move,
  uint depth)
{
  const auto& danger_moves = dangerMoves(player);
  for (const auto& danger_move: danger_moves.cells())
  {
    if (!isVictoryMove(player, danger_move, false, depth))
      continue;
    move = danger_move;
    return true;
  }

  return false;
}

GPoint Gomoku::hintBestDefense(
  GPlayer player,
  const GPoint& enemy_move4,
  const GStack<DEF_CELL_COUNT>& variants,
  int depth,
  uint enemy_move4_chain_depth)
{
  std::vector<GPoint> max_wgt_moves;
  max_wgt_moves.reserve(width() * height() - cells().size());

  int max_wgt = WGT_DEFEAT;

  //Пробуем контршахи
  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (!isLine4Move(player, move4))
      continue;
    int wgt = calcDefenseMove4Wgt(player, move4, depth, enemy_move4_chain_depth);
    if (wgt == WGT_VICTORY)
      return move4;
    if (wgt < max_wgt)
      continue;
    if (wgt > max_wgt)
    {
      max_wgt = wgt;
      max_wgt_moves.clear();
    }
    max_wgt_moves.push_back(move4);
  }

  //Пробуем варианты, заданные на входе
  for (uint i = 0; i < variants.size(); ++i)
  {
    const GPoint& move = variants[i];
    if (isLine4Move(player, move)) //вариант уже проверен
      continue;

    int wgt = calcDefenseWgt(player, move, depth, enemy_move4_chain_depth);
    if (wgt == WGT_VICTORY)
      return move;
    if (wgt < max_wgt)
      continue;
    if (wgt > max_wgt)
    {
      max_wgt = wgt;
      max_wgt_moves.clear();
    }
    max_wgt_moves.push_back(move);
  }

  if (max_wgt == WGT_DEFEAT)
  {
    //Все ходы проигрышные
    //Блокируем исходный шах противника
    return enemy_move4;
  }

  assert(!max_wgt_moves.empty());
  return max_wgt_moves[random(0, max_wgt_moves.size())];
}

GPoint Gomoku::hintMaxWgt(GPlayer player, int depth)
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
    return hintMaxWgt(!player, 0);
  }

  assert(!max_wgt_moves.empty());
  return max_wgt_moves[random(0, max_wgt_moves.size())];
}

int Gomoku::calcMaxWgt(GPlayer player, int depth)
{
  int max_wgt = WGT_DEFEAT;

  auto wgtHandler = [&](const GPoint&, int wgt)
  {
    if (wgt > max_wgt)
      max_wgt = wgt;
  };

  GPoint move;
  findVictoryMove(player, move, depth, wgtHandler);

  return max_wgt;
}

int Gomoku::calcWgt(GPlayer player, const GPoint &move, int depth)
{
  assert(isEmptyCell(move));

  int wgt = get(move).wgt[player];

  if (depth <= 0)
    return wgt;

  GMoveMaker gmm(this, player, move);
  const GMoveData& move_data = get(move);

  assert(move_data.m_moves5_count <= 1);

  GStack<DEF_CELL_COUNT> defense_variants;

  GPoint tmp_move4;

  int enemy_wgt;
  if (move_data.m_moves5_count == 1)
  {
    //Этот шах не может быть началом выигрышной цепочки шахов
    //(проверено на предыдущем уровне)
    //У противника единственный вариант - блокировать ход 5
    enemy_wgt = calcWgt(!player, getLine5Moves(player).lastCell(), depth - 1);
  }
  else if (depth >= 1 && hintShortestVictoryMove4Chain(player, tmp_move4, (depth == 2) ? (getAiLevel() / 2) : 0, &defense_variants))
    //Противник должен блокировать выигрышную цепочку шахов
    enemy_wgt = calcMaxDefenseWgt(!player, defense_variants, depth - 1, getAiLevel() / 2);
  else
    //Ищем максимальный вес противника
    enemy_wgt = calcMaxWgt(!player, depth - 1);

  //если у противника выигрыш, возвращаем проигрыш
  if (enemy_wgt == WGT_VICTORY)
    return WGT_DEFEAT;
  //и наоборот
  else if (enemy_wgt == WGT_DEFEAT)
    return WGT_VICTORY;

  //для вычисления веса лучшей последовательности ходов
  //из веса головы последовательности вычитается вес хвоста последовательности
  return wgt - enemy_wgt;
}

int Gomoku::calcMaxMove4ChainWgt(GPlayer player, uint depth, GBaseStack* defense_variants)
{
  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (calcMove4ChainWgt(player, move4, depth, defense_variants) == WGT_VICTORY)
      return WGT_VICTORY;
  }

  //Учитываем только выигрышные последовательности
  return 0;
}

int Gomoku::calcMove4ChainWgt(GPlayer player, const GPoint& move4, uint depth, GBaseStack* defense_variants)
{
  if (!isEmptyCell(move4))
    return 0;
  const auto& move4_data = dangerMoves(player).get(move4);
  if (!move4_data)
    return 0;
  const auto& moves5 = move4_data->m_moves5.cells();
  uint move5_count = 0;
  uint move5_pair[2];
  for (uint i = 0; i < moves5.size(); ++i)
  {
    if (!isEmptyCell(moves5[i]))
      continue;
    if (++move5_count > 2)
      break;
    move5_pair[move5_count - 1] = i;
  }
  if (move5_count == 0)
    //Все дополнения до линии 5 заблокированы
    return 0;
  if (move5_count >= 2)
  {
    //вилка 4х4 (выигрыш)
    if (defense_variants)
    {
      //Если у вилки ровно два завершения, то любое из них может быть блокирующим ходом
      //Иначе ни одно из них не может быть блокирующим ходом
      if (move5_count == 2)
      {
        defense_variants->push() = moves5[move5_pair[0]];
        defense_variants->push() = moves5[move5_pair[1]];
      }
      //Исходный шах в любом случае может быть блокирующим ходом
      defense_variants->push() = move4;
    }
    return WGT_VICTORY;
  }

  if (depth > 0)
  {
    GMoveMaker gmm(this, player, move4);

    const GPoint& enemy_block = getLine5Moves(player).lastCell();
    if (calcBlock5Wgt(!player, enemy_block, depth, defense_variants) == WGT_DEFEAT)
    {
      if (defense_variants)
      {
        //Как сам шах, так и ответ на него может быть блокирующим ходом
        defense_variants->push() = move4;
      }
      return WGT_VICTORY;
    }
  }

  return 0;
}

int Gomoku::calcBlock5Wgt(GPlayer player, const GPoint& block, uint depth, /*GStack<DEF_CELL_COUNT>*/GBaseStack* defense_variants)
{
  assert(isEmptyCell(block));
  assert(depth > 0);

  GMoveMaker gmm(this, player, block);
  const auto& block_data = get(block);

  if (block_data.m_moves5_count > 1)
    //Блокирующий ход реализует вилку 4х4, поэтому является выигрышным
    return WGT_VICTORY;

  //По возможности продолжаем цепочку шахов противника
  int enemy_move4_wgt = 0;
  if (block_data.m_moves5_count == 1)
  {
    //Блокирующий ход является контршахом
    const GPoint& enemy_move = getLine5Moves(player).lastCell();
    if (isLine4Move(!player, enemy_move))
      //Блокирующий ход противника также является контршахом,
      //поэтому имеем возможность продолжить цепочку шахов противника
      enemy_move4_wgt = calcMove4ChainWgt(!player, enemy_move, depth - 1, defense_variants);
  }
  else
    enemy_move4_wgt = calcMaxMove4ChainWgt(!player, depth - 1, defense_variants);

  if (enemy_move4_wgt != WGT_VICTORY)
    return 0;

  if (defense_variants)
  {
    //Если блокировка реализует линию 3 и есть ход, который развивает ее до шаха,
    //то такой ход нужно отнести к защитным вариантам,
    //поскольку противник будет вынужден блокировать контршах вместо продолжения атаки
    for (uint i = 0; i < block_data.m_line4_moves.size(); ++i)
      defense_variants->push() = block_data.m_line4_moves[i];
    //Сама блокировка также является защитным вариантом
    defense_variants->push() = block;
  }
  return WGT_DEFEAT;
}

bool Gomoku::isVictoryMove(GPlayer player, const GPoint &move, bool forced, uint depth)
{
  if (!isEmptyCell(move))
    return false;

  const auto& danger_move_data = dangerMoves(player).get(move);

  bool is_danger_move4 = false;
  if (danger_move_data != nullptr)
  {
    const auto& moves5 = danger_move_data->m_moves5.cells();
    for (uint i = 0; i < moves5.size(); ++i)
    {
      if (!isEmptyCell(moves5[i]))
        continue;
      if (is_danger_move4) //Ход реализует вилку 4х4
        return true;
      is_danger_move4 = true;
    }
  }

  if (depth == 0)
    return false;

  if (is_danger_move4)
  {
    GMoveMaker gmm(this, player, move);

    return isDefeatMove(!player, getLine5Moves(player).lastCell(), depth);
  }

  //Продолжаем атаку, если ход является открытой тройкой,
  //или ход является вынужденным и от предыдущего хода осталась незаблокированная открытая тройка
  if (!danger_move_data->m_open3 &&
      (!forced || calcMaxMove4ChainWgt(player, 0) != WGT_VICTORY))
    return false;

  GMoveMaker gmm(this, player, move);

  for (uint enemy_move4_chain_depth = 0; enemy_move4_chain_depth <= getAiLevel(); ++enemy_move4_chain_depth)
  {
    if (findDefenseMove4Chain(!player, depth, enemy_move4_chain_depth))
      return false;
  }

  return true;
}

bool Gomoku::isDefeatMove(GPlayer player, const GPoint& move, uint depth)
{
  assert(isEmptyCell(move));
  assert(depth > 0);

  GMoveMaker gmm(this, player, move);
  const auto& move_data = get(move);

  if (move_data.m_moves5_count > 1)
    //Блокирующий ход реализует вилку 4х4, поэтому является выигрышным
    return true;

  if (move_data.m_moves5_count == 1)
  {
    //Блокирующий ход является контршахом
    const GPoint& player_move = getLine5Moves(!player).lastCell();
    return isVictoryMove(player, player_move, true, depth - 1);
  }

  GPoint tmp_move;
  return hintVictoryChain(!player, tmp_move, depth - 1);
}

bool Gomoku::findDefenseMove4Chain(GPlayer player, uint depth, uint defense_move4_chain_depth)
{
  assert(depth > 0);

  if (defense_move4_chain_depth == 0)
  {
    GStack<4> def_vars;
    if (calcMaxMove4ChainWgt(!player, 0, &def_vars) == WGT_VICTORY)
    {
      uint i;
      for (i = 0; i < def_vars.size(); ++i)
      {
        if (isDefeatMove(!player, def_vars[i], depth))
          return true;
      }
    }
    return false;
  }

  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (isDefenseMove4(player, move4, depth, defense_move4_chain_depth))
      return true;
  }

  return false;
}

bool Gomoku::isDefenseMove4(GPlayer player, const GPoint &move4, uint depth, uint defense_move4_chain_depth)
{
  assert(defense_move4_chain_depth > 0);

  if (!isEmptyCell(move4))
    return false;

  const auto& move4_data = dangerMoves(player).get(move4);

  if (!move4_data)
    return false;

  const auto& moves5 = move4_data->m_moves5.cells();
  bool is_danger_move4 = false;
  for (uint i = 0; i < moves5.size(); ++i)
  {
    if (!isEmptyCell(moves5[i]))
      continue;
    if (is_danger_move4) //Ход реализует вилку 4х4, тем самым блокируя атаку противника
      return true;
    is_danger_move4 = true;
  }

  if (!is_danger_move4)
    return false;

  GMoveMaker gmm(this, player, move4);

  return !isVictoryForcedMove(!player, m_line5_moves[player].lastCell(), depth, defense_move4_chain_depth);
}

bool Gomoku::isVictoryForcedMove(GPlayer player, const GPoint &move, uint depth, uint defense_move4_chain_depth)
{
  assert(defense_move4_chain_depth > 0);

  GMoveMaker gmm(this, player, move);

  const auto& move_data = get(move);

  if (move_data.m_moves5_count > 1)  //Вынужденный ход реализует вилку 4х4, то есть является выигрышным
    return true;

  if (move_data.m_moves5_count == 1) //Вынужденный ход реализует контршах
  {
    const GPoint& enemy_block = m_line5_moves[player].lastCell();
    if (defense_move4_chain_depth > 1)
    {
      //Вынужденный ход игрока должен продолжить защитную цепочку контршахов
      return !isDefenseMove4(!player, enemy_block, depth, defense_move4_chain_depth - 1);
    }
    //Вынужденный ход игрока должен блокировать полушахи противника
    return !isDefeatMove(!player, enemy_block, depth);
  }

  return !findDefenseMove4Chain(!player, depth, defense_move4_chain_depth - 1);
}

int Gomoku::calcMaxDefenseWgt(
  GPlayer player,
  const GStack<DEF_CELL_COUNT>& variants,
  int depth,
  uint enemy_move4_chain_depth)
{
  int max_wgt = WGT_DEFEAT;

  int min_depth = -(int)getAiLevel();

  //Пробуем контршахи, если позволяет уровень сложности
  if (depth > min_depth)
  {
    const auto& moves4 = dangerMoves(player);
    for (const auto& move4: moves4.cells())
    {
      if (!isLine4Move(player, move4))
        continue;
      int wgt = calcDefenseMove4Wgt(player, move4, depth, enemy_move4_chain_depth);
      if (wgt == WGT_VICTORY)
        return WGT_VICTORY;
      if (wgt > max_wgt)
        max_wgt = wgt;
    }
  }

  //Пробуем варианты, заданные на входе
  for (uint i = 0; i < variants.size(); ++i)
  {
    const GPoint& move = variants[i];
    if (depth > min_depth && isLine4Move(player, move)) //вариант уже проверен
      continue;

    int wgt = calcDefenseWgt(player, move, depth, enemy_move4_chain_depth);
    if (wgt == WGT_VICTORY)
      return wgt;
    if (wgt > max_wgt)
      max_wgt = wgt;
  }

  return max_wgt;
}

int Gomoku::calcDefenseMove4Wgt(GPlayer player, const GPoint &move4, int depth, uint enemy_move4_chain_depth)
{
  int min_depth = -(int)getAiLevel();
  assert(depth > min_depth);

  assert(isEmptyCell(move4));

  int wgt = get(move4).wgt[player];

  GMoveMaker gmm(this, player, move4);

  const auto& move4_data = get(move4);

  if (move4_data.m_moves5_count > 1)
    return WGT_VICTORY;

  assert(move4_data.m_moves5_count == 1);
  const GPoint& enemy_move = m_line5_moves[player].lastCell();
  int enemy_wgt = get(enemy_move).wgt[!player];

  {
    GMoveMaker gmm(this, !player, enemy_move);
    const auto& enemy_move_data = get(enemy_move);

    if (enemy_move_data.m_moves5_count > 1)
      return  WGT_DEFEAT;

    int player_wgt;

    GStack<DEF_CELL_COUNT> defense_variants;
    GPoint tmp_move4;
    if (enemy_move_data.m_moves5_count == 1)
    {
      const auto& player_move = m_line5_moves[!player].lastCell();

      if (depth - 2 > min_depth && isLine4Move(player, player_move))
        player_wgt = calcDefenseMove4Wgt(player, player_move, depth - 2, enemy_move4_chain_depth);
      else
        player_wgt = calcDefenseWgt(player, player_move, depth - 2, enemy_move4_chain_depth);
    }
    else if (hintShortestVictoryMove4Chain(!player, tmp_move4, enemy_move4_chain_depth, &defense_variants))
    {
      //если атака противника не заблокирована, блокируем рекурсивно
      player_wgt = calcMaxDefenseWgt(player, defense_variants, depth - 2, enemy_move4_chain_depth);
    }
    else if (isAiDepth(depth))
    {
      //Учитываем максимальный вес следующего хода игрока,
      //для которого подбираем ход (веса всех вариантов уже известны)
      player_wgt = calcMaxWgt(player, depth - 2);
    }
    else
    {
      //Ход подбирается для enemy,
      //поэтому считаем вес последовательности, которая заканчивается на enemy_move
      player_wgt = 0;
    }

    if (player_wgt == WGT_VICTORY || player_wgt == WGT_DEFEAT)
      return player_wgt;
    else
      return wgt - enemy_wgt + player_wgt;
  }
}

int Gomoku::calcDefenseWgt(GPlayer player, const GPoint& move, int depth, uint enemy_move4_chain_depth)
{
  assert(isEmptyCell(move));

  int wgt = get(move).wgt[player];

  GMoveMaker pmm(this, player, move);

  const auto& move_data = get(move);
  if (move_data.m_moves5_count > 1)
    return WGT_VICTORY;

  bool is_move4 = (move_data.m_moves5_count == 1);

  GMoveMaker emm;

  if (is_move4)
  {
    //Закрываем шах
    const GPoint& enemy_block = m_line5_moves[player].lastCell();

    int enemy_wgt = get(enemy_block).wgt[!player];

    emm.doMove(this, !player, enemy_block);

    const auto& enemy_block_data = get(enemy_block);
    if (enemy_block_data.m_moves5_count > 1)
      return WGT_DEFEAT;

    //Если блокирующий ход противника является контршахом,
    //продолжаем рекурсивно
    if (enemy_block_data.m_moves5_count == 1)
    {
      const GPoint& player_block = m_line5_moves[!player].lastCell();
      int player_wgt = calcDefenseWgt(player, player_block, depth, enemy_move4_chain_depth);
      if (player_wgt == WGT_VICTORY || player_wgt == WGT_DEFEAT)
        return player_wgt;
      return wgt - enemy_wgt + player_wgt;
    }

    //Если выигрышная цепочка противника не заблокирована,
    //значит ход является проигрышным
    if (calcMaxMove4ChainWgt(!player, enemy_move4_chain_depth) == WGT_VICTORY)
      return WGT_DEFEAT;

    if (depth > 1)
    {
      //Уровень сложности позволяет уточнить вес на следующем уровне глубины
      int player_wgt = calcMaxWgt(player, depth - 2);
      if (player_wgt == WGT_VICTORY || player_wgt == WGT_DEFEAT)
        return player_wgt;
      return wgt - enemy_wgt + player_wgt;
    }

    return wgt - enemy_wgt;
  }

  //Если выигрышная цепочка противника не заблокирована,
  //значит ход является проигрышным
  if (calcMaxMove4ChainWgt(!player, enemy_move4_chain_depth) == WGT_VICTORY)
    return WGT_DEFEAT;

  //Уровень сложности позволяет уточнить вес на следующем уровне глубины
  if (depth > 0)
  {
    int enemy_wgt;
    GPoint tmp_move4;

    GStack<DEF_CELL_COUNT> defense_variants;
    if (depth >= 1 && hintShortestVictoryMove4Chain(player, tmp_move4, (depth == 2) ? (getAiLevel() / 2) : 0, &defense_variants))
      enemy_wgt = calcMaxDefenseWgt(!player, defense_variants, depth - 1, getAiLevel() / 2);
    else
      enemy_wgt = calcMaxWgt(!player, depth - 1);
    if (enemy_wgt == WGT_VICTORY)
      return WGT_DEFEAT;
    else if (enemy_wgt == WGT_DEFEAT)
      return WGT_VICTORY;
    return wgt - enemy_wgt;
  }

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

bool Gomoku::isAiDepth(int depth) const
{
  //поскольку подбор начинается на глубине 2
  //ходы ai на четной глубине
  return (depth & 1) == 0;
}

void Gomoku::addLine5Move(GPlayer player, const GPoint& p)
{
  m_line5_moves[player].push(p) = true;
}

void Gomoku::removeLine5Move(GPlayer player)
{
  assert(!m_line5_moves[player].cells().empty());
  m_line5_moves[player].pop();
}

bool Gomoku::isLine5Move(GPlayer player, const GPoint& move) const
{
  return !m_line5_moves[player].isEmptyCell(move);
}

void Gomoku::addLine4Moves(GPlayer player, const GPoint& move1, const GPoint& move2, GMoveData& source)
{
  auto& danger_moves = dangerMoves(player);

  auto& data1 = danger_moves[move1];
  if (!data1)
    data1 = std::make_unique<GDangerMoveData>(width(), height());
  //Одна и та же пара ходов линии 4 может встретиться при одновременной реализации нескольких линий 3
  //(ситуация ххX__х)
  //Нужно избежать дублирования при добавлении
  else if (!data1->m_moves5.isEmptyCell(move2))
  {
    assert(danger_moves[move2] && !danger_moves[move2]->m_moves5.isEmptyCell(move1));
    return;
  }
  data1->m_moves5.push(move2) = true;
  auto& data2 = danger_moves[move2];
  if (!data2)
    data2 = std::make_unique<GDangerMoveData>(width(), height());
  assert(data2->m_moves5.isEmptyCell(move1));
  data2->m_moves5.push(move1) = true;
  source.pushLine4Moves(move1, move2);
}

void Gomoku::undoLine4Moves(GMoveData& source)
{
  auto& danger_moves = dangerMoves(source.player);
  const GPoint* move1, * move2;
  while (!source.emptyLine4Moves())
  {
    source.popLine4Moves(move1, move2);
    auto& data2 = danger_moves[*move2];
    assert(data2 && !data2->m_moves5.cells().empty() && data2->m_moves5.cells().back() == *move1);
    data2->m_moves5.pop();
    if (data2->empty())
    {
      assert(danger_moves.cells().back() == *move2);
      danger_moves.pop();
    }
    auto& data1 = danger_moves[*move1];
    assert(data1 && !data1->m_moves5.cells().empty() && data1->m_moves5.cells().back() == *move2);
    data1->m_moves5.pop();
    if (data1->empty())
    {
      assert(danger_moves.cells().back() == *move1);
      danger_moves.pop();
    }
  }
}

bool Gomoku::isLine4Move(GPlayer player, const GPoint& move) const
{
  if (!isEmptyCell(move))
    return false;
  const auto& line4MoveData = m_danger_moves[player].get(move);
  if (!line4MoveData)
    return false;
  //ход является ходом линии 4, если для него заданы парные и хотя бы один из них не занят
  const auto& moves5 = line4MoveData->m_moves5.cells();
  for (uint i = 0; i < moves5.size(); ++i)
  {
    if (isEmptyCell(moves5[i]))
      return true;
  }
  return false;
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

  //ищем полушахи (открытые тройки)
  for (int i = 0; i < 4; ++i)
  {
    updateOpen3(vecs1[i]);
    updateOpen3(-vecs1[i]);
  }
}

void Gomoku::updateRelatedMovesState(const GVector& v1)
{
  const GPoint& last_move = lastCell();
  GMoveData& last_move_data = ref(last_move);
  GPlayer player = last_move_data.player;

  int counts[2] = {0, 0};

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

  GPoint empty_points[2];

  for (; ; )
  {
    assert(isValidCell(bp));

    assert(counts[player] > 0 && counts[player] < 5);
    assert(counts[!player] >= 0 && counts[!player] < 5);
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
      GPlayer first_player = get(bp + v4).player;
      GPlayer last_player = get(bp).player;
      GPlayer prev_player = isValidCell(bp + v4 + v1) ? get(bp + v4 + v1).player : G_EMPTY;
      GPlayer next_player = isValidCell(bp - v1) ? get(bp - v1).player : G_EMPTY;
      if (counts[!player] == 0)
      {
        //последний ход развил линию игрока
        //не учитываем вес линии, если она не может быть реализована без реализации смежной линии,
        //например в ситуации ххх--- не не надо учитывать вес линии хх---
        //поскольку она не может быть реализована без реализации смежной линии
        //в ситуации х----х учитываем вес только одной линии х----
        if (prev_player == player && last_player != player ||
            next_player == player && first_player != player)
        {
          playerWgtDelta = enemyWgtDelta = 0;
        }
        else if (counts[player] == 1)
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
        //если эта линия хотя бы с одной стороны продолжается
        //реализованным ходом противника, то ее вес не учитывается,
        //поскольку она не может быть реализована без реализации смежной линии
        if (prev_player == !player || next_player == !player)
        {
          playerWgtDelta = enemyWgtDelta = 0;
        }
        else
        {
          //оставшиеся кандидаты игрока больше не блокируют линию противника
          playerWgtDelta = - getBlockingMoveWgt(counts[!player]);
          //кандидаты противника больше не развивают свою линию
          enemyWgtDelta = - getFurtherMoveWgt(counts[!player]);
        }
      }

      if (counts[player] >= 3 || playerWgtDelta != 0 || enemyWgtDelta != 0)
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
            last_move_data.addLine5Move();
          }

          if (counts[player] == 3)
            empty_points[empty_count - 1] = p;

          addWgt(p, player, playerWgtDelta);
          addWgt(p, !player, enemyWgtDelta);

          if (--empty_count == 0)
            break;
        }
        if (counts[player] == 3)
          addLine4Moves(player, empty_points[0], empty_points[1], last_move_data);
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

void Gomoku::updateOpen3(const GVector& v1)
{
  //Рассматриваем позицию
  //753х2468
  //Символом х отмечен последний ход
  //В пару к нему должен быть реализован один и только один из ходов 2, 4, 6
  //Тогда некоторые из оставшихся ходов при реализации возможно образуют открытую тройку

  const GPoint& p1 = lastCell();

  //Во всех случаях ход 3 должен быть валидным и пустым
  GPoint p3 = p1 - v1;
  if (!isValidCell(p3) || !isEmptyCell(p3))
    return;

  //Во всех случаях ходы 2 и 4 должны быть валидными
  GPoint p2 = p1 + v1;
  if (!isValidCell(p2))
    return;
  GPoint p4 = p2 + v1;
  if (!isValidCell(p4))
    return;

  GPlayer player1 = get(p1).player;
  GPlayer player2 = get(p2).player;

  if (player2 == !player1)
    return;

  GPlayer player4 = get(p4).player;

  if (player2 == player1)
  {
    //Реализован ход 2
    //75_хх468
    //Во всех этих случаях ход 4 должен быть пустым
    if (player4 != G_EMPTY)
      return;
    updateOpen3_Xxx(p3, v1);
    updateOpen3_Xxx(p4, -v1);
    updateOpen3_X_xx(p3 - v1, v1);
    updateOpen3_X_xx(p4 + v1, -v1);
    return;
  }

  assert(player2 == G_EMPTY);

  //Ход 6 должен быть валидным
  GPoint p6 = p4 + v1;
  if (!isValidCell(p6))
    return;

  if (player4 == player1)
  {
    //Реализован ход 4
    //75_х_х68
    //Во всех этих случаях ход 6 должен быть пустым
    if (get(p6).player != G_EMPTY)
      return;
    updateOpen3_xXx(p2, v1);
    updateOpen3_Xx_x(p3, v1);
    updateOpen3_Xx_x(p6, -v1);
    return;
  }

  //В оставшихся случаях ход 4 должен быть пустым
  if (player4 != G_EMPTY)
    return;

  if (get(p6).player == player1)
  {
    //Реализован ход 6
    //75_х__х8
    //Ход 8 должен быть валидным и пустым
    GPoint p8 = p6 + v1;
    if (!isValidCell(p8) || !isEmptyCell(p8))
      return;
    addOpen3(p2);
    addOpen3(p4);
  }
}

void Gomoku::updateOpen3_Xxx(const GPoint& p3, const GVector &v1)
{
  //рассматриваем позицию
  //7531246
  //75_хх_6
  //ход 3 является полушахом (открытой тройкой) в случаях
  //__3хх__
  //о_3хх__
  //__3хх_о
  //то есть
  //1. точка 5 валидна и пуста
  //2. точка 6 валидна и пуста или точка 7 валидна и пуста
  //3. точка 6 не занята игроком и точка 7 не занята игроком
  GPoint p5 = p3 - v1;
  if (!isValidCell(p5) || !isEmptyCell(p5))
    return;
  GPoint p6 = p3 + v1 * 4;
  GPoint p7 = p5 - v1;
  if ((!isValidCell(p6) || !isEmptyCell(p6)) && (!isValidCell(p7) || !isEmptyCell(p7)))
    return;
  GPlayer player = lastMovePlayer();
  if (isValidCell(p6) && get(p6).player == player || isValidCell(p7) && get(p7).player == player)
    return;
  addOpen3(p3);
}

void Gomoku::updateOpen3_X_xx(const GPoint& p5, const GVector &v1)
{
  //рассматриваем позицию
  //7531246
  //75_хх_6
  //ход 5 является полушахом (открытой тройкой) в случае
  //_5_хх_
  //то есть
  //точки 5, 7 должны быть валидными и пустыми
  if (!isValidCell(p5) || !isEmptyCell(p5))
    return;
  GPoint p7 = p5 - v1;
  if (!isValidCell(p7) || !isEmptyCell(p7))
    return;
  addOpen3(p5);
}

void Gomoku::updateOpen3_xXx(const GPoint &p2, const GVector &v1)
{
  //рассматриваем позицию
  //75312468
  //75_x_х_8
  //ход 2 является полушахом (открытой тройкой) в случаях
  //__х2х__
  //о_х2х__
  //__х2х_о
  //то есть
  //1. точка 5 валидна и пуста или точка 8 валидна и пуста
  //2. точка 5 не занята игроком и точка 8 не занята игроком

  GPoint p5 = p2 - v1 * 3;
  GPoint p8 = p2 + v1 * 3;
  if ((!isValidCell(p5) || !isEmptyCell(p5)) && (!isValidCell(p8) || !isEmptyCell(p8)))
    return;
  GPlayer player = lastMovePlayer();
  if (isValidCell(p5) && get(p5).player == player || isValidCell(p8) && get(p8).player == player)
    return;
  addOpen3(p2);
}

void Gomoku::updateOpen3_Xx_x(const GPoint &p3, const GVector &v1)
{
  //рассматриваем позицию
  //531246
  //5_x_x_
  //ход 3 является полушахом (открытой тройкой) в случае
  //_3х_х_
  //то есть
  //точка 5 валидна и пуста
  GPoint p5 = p3 - v1;
  if (!isValidCell(p5) || !isEmptyCell(p5))
    return;
  addOpen3(p3);
}

void Gomoku::addOpen3(const GPoint &move)
{
  auto& move_data = dangerMoves(lastMovePlayer())[move];
  if (!move_data)
    move_data = std::make_unique<GDangerMoveData>(width(), height());
  if (move_data->m_open3)
    return;
  move_data->m_open3 = true;
  ref(lastCell()).pushOpen3(move);
}

void Gomoku::undoOpen3(GMoveData& source)
{
  auto& danger_moves = dangerMoves(source.player);

  GPoint* open3_move;
  while (!source.m_open3_moves.empty())
  {
    source.popOpen3(open3_move);
    auto& open3_data = danger_moves[*open3_move];
    assert(open3_data && open3_data->m_open3);
    open3_data->m_open3 = false;
    if (open3_data->empty())
    {
      assert(danger_moves.cells().back() == *open3_move);
      danger_moves.pop();
    }
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
  GPoint p = lastCell();
  GMoveData& last_move_data = ref(p);

  undoOpen3(last_move_data);

  for (; last_move_data.m_moves5_count > 0; --last_move_data.m_moves5_count)
    removeLine5Move(last_move_data.player);

  undoLine4Moves(last_move_data);

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

//  for (; last_move_data.line5_moves_count > 0; --last_move_data.line5_moves_count)
//    removeLine5Move(last_move_data.player);

//  undoLine4Moves(last_move_data);

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

int Gomoku::getFurtherMoveWgt(int line_len)
{
  assert(line_len > 0 && line_len < 5);
  return getLineWgt(line_len + 1) - getLineWgt(line_len);
}

int Gomoku::getBlockingMoveWgt(int line_len)
{
  assert(line_len > 0 && line_len < 5);
  return getLineWgt(line_len);
}

int Gomoku::getLineWgt(int line_len)
{
  assert(line_len > 0 && line_len <= 5);
  return (line_len >= 4) ? 6  : (line_len * 2 - 1);
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
