#include "gomoku.h"
#include "grandom.h"

namespace nsg
{

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
  while (v1 == GVector{0, 0});
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

Gomoku::Gomoku() :
  m_ai_level(0),
  m_line5({-1, -1}, {0, 0})
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
  GPoint p{x, y};
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

  if (isMove5(player, move))
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
  if (hintMove5(player, move))
    return move;

  //Блокировка финального хода противника
  if (hintBlock5(player, move))
    return move;

  //Выигрышная цепочка шахов
  GPoint victory_move;
  if (hintShortestVictoryMove4Chain(player, victory_move, getAiLevel()))
    return victory_move;

  GStack<gridSize()> defense_variants;

  //Блокировка выигрышной цепочки шахов противника
  if (hintShortestVictoryMove4Chain(!player, victory_move, getAiLevel(), &defense_variants))
    return hintBestDefense(player, victory_move, defense_variants, 2, getAiLevel());

  //Выигрышная цепочка шахов и полушахов
//  if (hintShortestVictoryChain(player, victory_move, getAiLevel()))
//    return victory_move;

  return hintBestAttack(player);
}

GPoint Gomoku::hintSecondMove() const
{
  assert(cells().size() == 1);

  const GPoint& first_move = cells()[0];

  GPoint center{width() / 2, height() / 2};

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

bool Gomoku::hintMove5(GPlayer player, GPoint &point) const
{
  const auto& line5Moves = m_moves5[player].cells();
  for (const auto& move: line5Moves)
  {
    if (!isEmptyCell(move))
      continue;
    point = move;
    return true;
  }
  return false;
}

bool Gomoku::hintBlock5(GPlayer player, GPoint &point) const
{
  return hintMove5(!player, point);
}

bool Gomoku::hintShortestVictoryMove4Chain(
    GPlayer player,
    GPoint &move,
    uint max_depth,
    GBaseStack *defense_variants)
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
  GBaseStack* defense_variants
  )
{
  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (!findVictoryMove4Chain(player, move4, depth, defense_variants))
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

GPoint Gomoku::hintBestAttack(GPlayer player)
{
  GStack<gridSize()> max_wgt_moves;

  int max_wgt = calcMaxAttackWgt(player, 0, &max_wgt_moves);

  if (max_wgt == WGT_DEFEAT)
  {
    assert(max_wgt_moves.empty());
    //Все ходы проигрышные
    //Перекрываем лучший выигрышный ход противника
    calcMaxWgt(!player, 0, &max_wgt_moves);
  }

  assert(!max_wgt_moves.empty());
  return max_wgt_moves[random(0, max_wgt_moves.size())];
}

GPoint Gomoku::hintBestDefense(
  GPlayer player,
  const GPoint& enemy_move4,
  const GBaseStack& variants,
  uint depth,
  uint enemy_move4_chain_depth)
{
  GStack<gridSize()> max_wgt_moves;

  if (calcMaxDefenseWgt(player, variants, depth, enemy_move4_chain_depth, true, true, &max_wgt_moves) == WGT_DEFEAT)
    return enemy_move4;
  assert(!max_wgt_moves.empty());
  return max_wgt_moves[random(0, max_wgt_moves.size())];
}

int Gomoku::calcMaxWgt(GPlayer player, uint depth, GBaseStack* max_wgt_moves)
{
  assert(!max_wgt_moves || max_wgt_moves->empty());
  int max_wgt = WGT_DEFEAT;

  GPoint move{0, 0};
  do
  {
    if (!isEmptyCell(move))
      continue;
    int wgt = calcWgt(player, move, depth);
    if (updateMaxWgt(move, wgt, max_wgt_moves, max_wgt) == WGT_VICTORY)
      return WGT_VICTORY;
  }
  while (next(move));

  return max_wgt;
}

int Gomoku::calcWgt(GPlayer player, const GPoint &move, uint depth)
{
  assert(isEmptyCell(move));

  int wgt = get(move).wgt[player];

  if (depth >= 2)
    return wgt;

  GMoveMaker gmm(this, player, move);
  const GMoveData& move_data = get(move);

  assert(move_data.m_moves5_count <= 1);

  GStack<gridSize()> defense_variants;

  GPoint tmp_move4;

  int enemy_wgt;
  if (move_data.m_moves5_count == 1)
  {
    //Этот шах не может быть началом выигрышной цепочки шахов
    //(проверено на предыдущем уровне)
    //У противника единственный вариант - блокировать ход 5
    enemy_wgt = calcWgt(!player, m_moves5[player].lastCell(), depth + 1);
  }
  else if (depth <= 1 && hintShortestVictoryMove4Chain(player, tmp_move4, (depth == 0) ? (getAiLevel() / 2) : 0, &defense_variants))
    //Противник должен блокировать выигрышную цепочку шахов
    enemy_wgt = calcMaxDefenseWgt(!player, defense_variants, depth + 1, getAiLevel() / 2, true, false);
  else
    //Ищем максимальный вес противника
    enemy_wgt = calcMaxWgt(!player, depth + 1);

  if (isSpecialWgt(enemy_wgt))
    return -enemy_wgt;

  //для вычисления веса лучшей последовательности ходов
  //из веса головы последовательности вычитается вес хвоста последовательности
  return wgt - enemy_wgt;
}

bool Gomoku::findVictoryMove4Chain(GPlayer player, uint depth, GBaseStack* defense_variants)
{
  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (findVictoryMove4Chain(player, move4, depth, defense_variants))
      return true;
  }

  return false;
}

bool Gomoku::findVictoryMove4Chain(GPlayer player, const GPoint& move4, uint depth, GBaseStack* defense_variants)
{
  if (!isEmptyCell(move4))
    return false;
  const auto& move4_data = dangerMoves(player).get(move4);
  if (!move4_data)
    return false;
  const auto& moves5 = move4_data->m_moves5/*.cells()*/;
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
    return false;
  if (move5_count >= 2)
  {
    //вилка 4х4 (выигрыш)
    if (defense_variants)
    {
      assert(defense_variants->empty());
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
    return true;
  }

  if (depth > 0)
  {
    GMoveMaker gmm(this, player, move4);

    const GPoint& enemy_block = m_moves5[player].lastCell();
    if (isDefeatBlock5(!player, enemy_block, depth, defense_variants))
    {
      if (defense_variants)
      {
        //Как сам шах, так и ответ на него может быть блокирующим ходом
        defense_variants->push() = move4;
      }
      return true;
    }
  }

  return false;
}

bool Gomoku::isDefeatBlock5(GPlayer player, const GPoint &block, uint depth, GBaseStack *defense_variants)
{
  assert(isEmptyCell(block));
  assert(depth > 0);

  GMoveMaker gmm(this, player, block);
  const auto& block_data = get(block);

  if (block_data.m_moves5_count > 1)
    //Блокирующий ход реализует вилку 4х4, поэтому является выигрышным
    return false;

  //По возможности продолжаем цепочку шахов противника
  bool is_victory_enemy_move4;
  if (block_data.m_moves5_count == 1)
  {
    //Блокирующий ход является контршахом
    const GPoint& enemy_move = m_moves5[player].lastCell();
    //Блокирующий ход противника тоже может быть контршахом,
    //в этом случае имеем возможность продолжить цепочку шахов противника
    is_victory_enemy_move4 = findVictoryMove4Chain(!player, enemy_move, depth - 1, defense_variants);
  }
  else
    is_victory_enemy_move4 = findVictoryMove4Chain(!player, depth - 1, defense_variants);

  if (!is_victory_enemy_move4)
    return false;

  if (defense_variants)
  {
    //Если блокировка реализует линию 3 и есть ход, который развивает ее до шаха,
    //то такой ход нужно отнести к защитным вариантам,
    //поскольку противник будет вынужден блокировать контршах вместо продолжения атаки
    for (uint i = 0; i < block_data.m_moves4.size(); )
    {
      const GPoint& move1 = block_data.m_moves4[i++];
      assert(i < block_data.m_moves4.size());
      const GPoint& move2 = block_data.m_moves4[i++];
      assert(isEmptyCell(move1) && isEmptyCell(move2));
      //Если один из ходов пары является ходом 5,
      //значит блокирующий ход уже является контршахом,
      //и оба хода пары не дают никакой дополнительной защиты
      if (!m_moves5[player].isEmptyCell(move1) || !m_moves5[player].isEmptyCell(move2))
      {
        assert(isMove5(player, move1) || isMove5(player, move2));
        assert(block_data.m_moves5_count == 1);
        continue;
      }
      defense_variants->push() = move1;
      defense_variants->push() = move2;
    }
    //Сама блокировка также является защитным вариантом
    defense_variants->push() = block;
  }
  return true;
}

bool Gomoku::isVictoryMove(GPlayer player, const GPoint &move, bool forced, uint depth)
{
  if (!isEmptyCell(move))
    return false;

  const auto& danger_move_data = dangerMoves(player).get(move);

  bool is_danger_move4 = false;
  if (danger_move_data != nullptr)
  {
    const auto& moves5 = danger_move_data->m_moves5/*.cells()*/;
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

    return isDefeatMove(!player, m_moves5[player].lastCell(), depth);
  }

  //Продолжаем атаку, если ход является открытой тройкой,
  //или ход является вынужденным и от предыдущего хода осталась незаблокированная открытая тройка
  if (!danger_move_data->m_open3 &&
      (!forced || !findVictoryMove4Chain(player, 0)))
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
    const GPoint& player_move = m_moves5[!player].lastCell();
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
    if (findVictoryMove4Chain(!player, 0, &def_vars))
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

  const auto& moves5 = move4_data->m_moves5/*.cells()*/;
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

  return !isVictoryForcedMove(!player, m_moves5[player].lastCell(), depth, defense_move4_chain_depth);
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
    const GPoint& enemy_block = m_moves5[player].lastCell();
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

int Gomoku::calcMaxAttackWgt(GPlayer player, uint depth, GBaseStack* max_wgt_moves)
{
  auto attack_moves = findAttackMoves(player);

//  if (attack_moves.empty())
//    return calcMaxWgt(player, depth, max_wgt_moves);

  int max_wgt = WGT_DEFEAT;

  for (const auto& attack_move: attack_moves)
  {
    int wgt = calcAttackWgt(player, attack_move, depth);
    if (updateMaxWgt(attack_move, wgt, max_wgt_moves, max_wgt) == WGT_VICTORY)
      return WGT_VICTORY;
  }

  if (max_wgt == WGT_NEAR_VICTORY)
    return max_wgt;

  //Рассматриваем
  return max_wgt;
}

int Gomoku::calcAttackWgt(GPlayer player, const GAttackMove &move, uint depth)
{
  int wgt = get(move).wgt[player];

  GMoveMaker gmm(this, player, move);

  int enemy_wgt = calcMaxDefenseWgt(!player, move.defense_variants, depth + 1, getReasonableMove4ChainDepth(depth), false, true);

  if (isSpecialWgt(enemy_wgt))
    return -enemy_wgt;

  return wgt - enemy_wgt;
}

std::list<GAttackMove> Gomoku::findAttackMoves(GPlayer player)
{
  std::list<GAttackMove> attack_moves;

  //Добавляем полушахи
  auto& danger_moves = dangerMoves(player);
  for (const auto& danger_move: danger_moves.cells())
  {
    auto& attack_move = attack_moves.emplace_back();
    if (!isDangerOpen3(player, danger_move, attack_move.defense_variants))
      attack_moves.pop_back();
  }

  return attack_moves;
}

int Gomoku::calcMaxDefenseWgt(
  GPlayer player,
  const GBaseStack& variants,
  uint depth,
  uint enemy_move4_chain_depth,
  bool is_player_forced,
  bool is_enemy_forced,
  GBaseStack* max_wgt_moves)
{
  assert(!max_wgt_moves || max_wgt_moves->empty());

  int max_wgt = WGT_DEFEAT;

  //Множество с быстрым поиском для исключения дублирования рассматриваемых вариантов
  GPointStack& grid = getGrid(depth);
  //grid.clear();

  //Пробуем варианты, заданные на входе
  for (uint i = 0; i < variants.size(); ++i)
  {
    const GPoint& move = variants[i];
    if (!grid.isEmptyCell(move))
      continue;
    grid.push(move) = true;
    int wgt = calcDefenseWgt(player, move, depth, enemy_move4_chain_depth, is_player_forced, is_enemy_forced);
    if (updateMaxWgt(move, wgt, max_wgt_moves, max_wgt) == WGT_VICTORY)
      return WGT_VICTORY;
  }

  //Пробуем контршахи
  const auto& moves4 = dangerMoves(player);
  for (const auto& move4: moves4.cells())
  {
    if (!grid.isEmptyCell(move4))
      continue;
    if (!isDangerMove4(player, move4))
      continue;
    //Если уровень сложности не позволяет рассматривать контршахи, возвращаем почти проигрыш
    if (depth >= getAiLevel())
    {
      updateMaxWgt(move4, WGT_NEAR_DEFEAT, max_wgt_moves, max_wgt);
      return WGT_NEAR_DEFEAT;
    }
    int wgt = calcDefenseMove4Wgt(player, move4, variants, depth, enemy_move4_chain_depth, is_player_forced, is_enemy_forced);
    if (updateMaxWgt(move4, wgt, max_wgt_moves, max_wgt) == WGT_VICTORY)
      return WGT_VICTORY;
  }

  return max_wgt;
}

int Gomoku::calcDefenseMove4Wgt(
  GPlayer player,
  const GPoint &move4,
  const GBaseStack& variants,
  uint depth,
  uint enemy_move4_chain_depth,
  bool is_player_forced,
  bool is_enemy_forced)
{
  int wgt = get(move4).wgt[player];

  GMoveMaker gmm(this, player, move4);
  const auto& move4_data = get(move4);

  if (move4_data.m_moves5_count > 1)
    return WGT_VICTORY;

  assert(move4_data.m_moves5_count == 1);
  const GPoint& enemy_move = m_moves5[player].lastCell();
  int enemy_wgt = get(enemy_move).wgt[!player];

  {
    GMoveMaker gmm(this, !player, enemy_move);
    const auto& enemy_move_data = get(enemy_move);

    if (enemy_move_data.m_moves5_count > 1)
      return  WGT_DEFEAT;

    int player_wgt;

    GStack<gridSize()> defense_variants;
    if (enemy_move_data.m_moves5_count == 1)
    {
      const auto& player_move = m_moves5[!player].lastCell();
      player_wgt = calcDefenseWgt(player, player_move, depth + 2, enemy_move4_chain_depth, is_player_forced, is_enemy_forced);
    }
    else
      player_wgt = calcMaxDefenseWgt(player, variants, depth + 2, enemy_move4_chain_depth, is_player_forced, is_enemy_forced);

    if (isSpecialWgt(player_wgt))
      return -player_wgt;
    return wgt - enemy_wgt + player_wgt;
  }
}

int Gomoku::calcDefenseWgt(
  GPlayer player,
  const GPoint& move,
  uint depth,
  uint enemy_move4_chain_depth,
  bool is_player_forced,
  bool is_enemy_forced)
{
  assert(isEmptyCell(move));
  assert(is_player_forced || is_enemy_forced);
  int wgt = get(move).wgt[player];
  int enemy_wgt;

  GMoveMaker pmm(this, player, move);

  const auto& move_data = get(move);
  if (move_data.m_moves5_count > 1)
    return WGT_VICTORY;

  bool is_move4 = (move_data.m_moves5_count == 1);

  GStack<gridSize()> defense_variants;

  if (is_move4)
  {
    //Закрываем шах
    const GPoint& enemy_block = m_moves5[player].lastCell();

    enemy_wgt = get(enemy_block).wgt[!player];

    GMoveMaker gmm(this, !player, enemy_block);

    const auto& enemy_block_data = get(enemy_block);
    if (enemy_block_data.m_moves5_count > 1)
      return WGT_DEFEAT;

    int player_wgt;

    //Если блокирующий ход противника является контршахом,
    //продолжаем рекурсивно
    if (enemy_block_data.m_moves5_count == 1)
    {
      const GPoint& player_block = m_moves5[!player].lastCell();
      player_wgt = calcDefenseWgt(player, player_block, depth + 2, enemy_move4_chain_depth, is_player_forced, is_enemy_forced);
    }
    //Если выигрышная цепочка противника не заблокирована,
    //рекурсивно ищем лучший защитный вариант
    else if (findVictoryMove4Chain(!player, enemy_move4_chain_depth, &defense_variants))
      player_wgt = calcMaxDefenseWgt(player, defense_variants, depth + 2, enemy_move4_chain_depth, is_player_forced, is_enemy_forced);
    //Если все ходы противника в рассмотренной цепочке вынужденные,
    //и уровень сложности позволяет погружаться дальше,
    //рассматриваем атаку противника
    else if ((depth + 1) < getAiLevel() && is_enemy_forced)
      player_wgt = calcMaxAttackWgt(player, depth + 2);
    //Если игрок на этой глубине совпадает с игроком, для которого исходно подбираем ход,
    //то учитываем максимальный хранимый вес его следующего хода
    else if (isAiDepth(depth))
      player_wgt = calcMaxWgt(player, depth + 2);
    //Иначе не учитываем следующий ход игрока
    else
      player_wgt = 0;

    if (isSpecialWgt(player_wgt))
      return -player_wgt;
    return wgt - enemy_wgt + player_wgt;
  }

  //Если выигрышная цепочка противника не заблокирована,
  //значит ход является проигрышным
  if (findVictoryMove4Chain(!player, enemy_move4_chain_depth))
    return WGT_DEFEAT;

  if (depth >= getAiLevel())
  {
    //Последним в цепочке должен быть ход игрока, для которого исходно подбирается ход
    return isAiDepth(depth) ? wgt : (wgt - calcMaxWgt(!player, depth + 1));
  }

  //Защитный ход может одновременно быть контратакующим
  GPoint tmp_move4;
  defense_variants.clear();
  uint player_move4_chain_depth = getReasonableMove4ChainDepth(depth);
  if (hintShortestVictoryMove4Chain(player, tmp_move4, player_move4_chain_depth, &defense_variants))
    enemy_wgt = calcMaxDefenseWgt(!player, defense_variants, depth + 1, player_move4_chain_depth, is_enemy_forced, is_player_forced);
  else
    enemy_wgt = calcMaxAttackWgt(!player, depth + 1);

  if (isSpecialWgt(enemy_wgt))
    return -enemy_wgt;
  return wgt - enemy_wgt;
}

bool Gomoku::isGameOver() const
{
  return getLine5() || cells().size() == width() * height();
}

const GLine* Gomoku::getLine5() const
{
  return isValidCell(m_line5.start) ? &m_line5 : 0;
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

bool Gomoku::isAiDepth(uint depth) const
{
  //ходы ai на четной глубине
  return (depth & 1) == 0;
}

void Gomoku::addMove5(GPlayer player, const GPoint& p)
{
  m_moves5[player].push(p) = true;
}

void Gomoku::removeMove5(GPlayer player)
{
  assert(!m_moves5[player].cells().empty());
  m_moves5[player].pop();
}

bool Gomoku::isMove5(GPlayer player, const GPoint& move) const
{
  assert(isValidCell(move));
  return !m_moves5[player].isEmptyCell(move);
}

void Gomoku::addMoves4(GPlayer player, const GPoint& move1, const GPoint& move2, GMoveData& source)
{
  auto& danger_moves = dangerMoves(player);

  auto& data1 = danger_moves[move1];
  if (!data1)
    data1 = std::make_unique<GDangerMoveData>();
  //Одна и та же пара ходов линии 4 может встретиться при одновременной реализации нескольких линий 3
  //(ситуации ххх__х, xx_x_x, xx__xx, x_xx_x)
  //Нужно избежать дублирования при добавлении
  else if (/*!data1->m_moves5.isEmptyCell(move2)*/std::find(data1->m_moves5.begin(), data1->m_moves5.end(), move2) != data1->m_moves5.end())
  {
    assert(
      danger_moves[move2] &&
      std::find(danger_moves[move2]->m_moves5.begin(), danger_moves[move2]->m_moves5.end(), move1) != danger_moves[move2]->m_moves5.end()
      /*!danger_moves[move2]->m_moves5.isEmptyCell(move1)*/);
    return;
  }
  //data1->m_moves5.push(move2) = true;
  data1->m_moves5.push() = move2;
  auto& data2 = danger_moves[move2];
  if (!data2)
    data2 = std::make_unique<GDangerMoveData>();
  assert(/*data2->m_moves5.isEmptyCell(move1)*/std::find(data2->m_moves5.begin(), data2->m_moves5.end(), move1) == data2->m_moves5.end());
  //data2->m_moves5.push(move1) = true;
  data2->m_moves5.push() = move1;
  source.pushLine4Moves(move1, move2);
}

void Gomoku::undoMoves4(GMoveData& source)
{
  auto& danger_moves = dangerMoves(source.player);
  const GPoint* move1, * move2;
  while (!source.emptyLine4Moves())
  {
    source.popLine4Moves(move1, move2);
    auto& data2 = danger_moves[*move2];
    assert(data2 && !data2->m_moves5/*.cells()*/.empty() && data2->m_moves5/*.cells()*/.back() == *move1);
    data2->m_moves5.pop();
    if (data2->empty())
    {
      assert(danger_moves.cells().back() == *move2);
      danger_moves.pop();
    }
    auto& data1 = danger_moves[*move1];
    assert(data1 && !data1->m_moves5/*.cells()*/.empty() && data1->m_moves5/*.cells()*/.back() == *move2);
    data1->m_moves5.pop();
    if (data1->empty())
    {
      assert(danger_moves.cells().back() == *move1);
      danger_moves.pop();
    }
  }
}

bool Gomoku::isDangerMove4(GPlayer player, const GPoint& move) const
{
  if (!isEmptyCell(move))
    return false;
  const auto& dangerMoveData = m_danger_moves[player].get(move);
  if (!dangerMoveData)
    return false;
  //ход является ходом линии 4, если для него заданы парные и хотя бы один из них не занят
  const auto& moves5 = dangerMoveData->m_moves5/*.cells()*/;
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

void Gomoku::initMovesWgt()
{
  GPoint lineStart{0, 0};
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
          assert(enemyWgtDelta == 0);
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
        int empty_count = 5 - counts[0] - counts[1];
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
            if (isMove5(player, p))
              break;
            addMove5(player, p);
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
          addMoves4(player, empty_points[0], empty_points[1], last_move_data);
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
    move_data = std::make_unique<GDangerMoveData>();
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

bool Gomoku::isDangerOpen3(GPlayer player, const GPoint &move, GBaseStack &defense_variants)
{
  if (!isEmptyCell(move))
    return false;
  const auto& danger_move_data = dangerMoves(player).get(move);
  if (!danger_move_data || !danger_move_data->m_open3)
    return false;
  GMoveMaker gmm(this, player, move);
  return findVictoryMove4Chain(player, 0, &defense_variants);
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
    removeMove5(last_move_data.player);

  undoMoves4(last_move_data);

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

int Gomoku::updateMaxWgt(const GPoint& move, int wgt, GBaseStack* max_wgt_moves, int& max_wgt)
{
  if (wgt < max_wgt)
    return wgt;
  if (wgt > max_wgt)
  {
    max_wgt = wgt;
    if (max_wgt_moves)
      max_wgt_moves->clear();
  }
  if (max_wgt_moves && max_wgt > WGT_DEFEAT)
    max_wgt_moves->push() = move;
  return wgt;
}

} //namespace nsg
