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

  return hintMaxWgt(player);
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

GPoint Gomoku::hintMaxWgt(GPlayer player)
{
  GStack<gridSize()> max_wgt_moves;

  m_enemy_attack_wgt = calcMaxAttackWgt(!player, 0, true);

  int max_wgt = calcMaxWgt(player, 0, &max_wgt_moves);

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

int Gomoku::calcMaxWgt(GPlayer player, uint depth, GBaseStack* max_wgt_moves)
{
  assert(!max_wgt_moves || max_wgt_moves->empty());
  int max_wgt = WGT_DEFEAT;

  GPoint move{0, 0};
  do
  {
    if (!isEmptyCell(move))
      continue;
    if (updateMaxWgt(move, calcWgt(player, move, depth, false), max_wgt_moves, max_wgt) == WGT_VICTORY)
      return WGT_VICTORY;
  }
  while (next(move));

  return max_wgt;
}

int Gomoku::calcWgt(
  GPlayer player,
  const GPoint& move,
  uint depth,
  bool forced)
{
  assert(isEmptyCell(move));

  if (depth >= 2)
    return getStoredWgt(player, move);

  int wgt;
  if (depth == 0 || m_enemy_attack_wgt > WGT_MIN_LONG_ATTACK)
  {
    if (forced)
      wgt = calcForcedAttackWgt(player, move, 0, true);
    else
      wgt = calcAttackWgt(player, move, 0);
    if (wgt > WGT_MIN_LONG_ATTACK)
      return wgt;
  }

  GMoveMaker gmm(this, player, move);
  const GMoveData& move_data = get(move);

  assert(move_data.m_moves5_count <= 1);

  if (move_data.m_moves5_count == 1)
    wgt = -calcWgt(!player, m_moves5[player].lastCell(), depth + 1, true);
  else
    wgt = -calcMaxWgt(!player, depth + 1);
  if (!isVictoryOrDefeat(wgt))
    wgt += getStoredWgt(player, move);
  return wgt;
}

bool Gomoku::findVictoryMove4Chain(GPlayer player, GBaseStack* defense_variants, GPoint* victory_move)
{
  GMove4Tree root;
  if (addChildMoves4(root, player, dangerMoves(player).cells(), defense_variants, victory_move))
    return true;
  while (!root.empty())
  {
    if (processChildMoves4(root, player, defense_variants, victory_move))
      return true;
  }
  return false;
}

bool Gomoku::processChildMoves4(
  GMove4Tree& tree,
  GPlayer player,
  GBaseStack* defense_variants,
  GPoint* victory_move)
{
  assert(!tree.empty());

  auto child = tree.begin();
  decltype(child) next_child;
  for (; child != tree.end(); child = next_child)
  {
    next_child = std::next(child);
    if (findMove4Chain(*child, player, defense_variants))
    {
      if (victory_move)
        *victory_move = child->move4;
      return true;
    }
    if (child->vertex().empty())
      tree.erase(child);
  }
}

bool Gomoku::findMove4Chain(
  GMove4Edge& edge,
  GPlayer player,
  GBaseStack* defense_variants)
{
  GMoveMaker gmm(this, player, edge.move4);
  assert(get(edge.move4).m_moves5_count == 1);
  GMoveMaker emm(this, player, edge.block);

  bool victory;
  if (edge.vertex().empty())
  {
    const GMoveData& block_data = get(edge.block);
    assert(block_data.m_moves5_count <= 1);
    if (block_data.m_moves5_count == 1)
    {
      //У противника контршах
      //У игрока единственный вариант, который случайно тоже может оказаться шахом
      victory = addChildMove4(edge.vertex(), player, m_moves5[!player].cells().back(), &edge.unforced_move4, defense_variants);
    }
    else
    {
      //продолжаем цепочку от последнего невынужденного шаха
      GStack<32> chain_moves;
      getChainMoves(player, edge.unforced_move4, chain_moves);
      victory = addChildMoves4(edge.vertex(), player, chain_moves, defense_variants);
    }
  }
  else
    victory = processChildMoves4(edge.vertex(), player, defense_variants);

  if (victory && defense_variants)
  {
    defense_variants->push() = edge.move4;
    defense_variants->push() = edge.block;
    //Если блокировка реализует линию 3 и есть ход, который развивает ее до шаха,
    //то такой ход нужно отнести к защитным вариантам,
    //поскольку игрок будет вынужден блокировать контршах вместо продолжения атаки
    const GMoveData& block_data = get(edge.block);
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
  }

  return victory;
}

bool Gomoku::addChildMoves4(
  GMove4Tree &leaf,
  GPlayer player,
  const GBaseStack& variants,
  GBaseStack *defense_variants,
  GPoint *victory_move)
{
  assert(leaf.empty());

  for (const GPoint& variant: variants)
  {
    if (addChildMove4(leaf, player, variant, 0, defense_variants))
    {
      if (victory_move)
        *victory_move = variant;
      return true;
    }
  }
  return false;
}

bool Gomoku::addChildMove4(
  GMove4Tree &leaf,
  GPlayer player,
  const GPoint& variant,
  GPoint* unforced_move4,
  GBaseStack *defense_variants)
{
  if (!isEmptyCell(variant))
    return false;
  //  const auto& move4_data = dangerMoves(player).get(move4);
  //  const auto& moves5 = move4_data.m_moves5;
  //  uint move5_count = 0;
  //  uint move5_pair[2];
  //  for (uint i = 0; i < moves5.size(); ++i)
  //  {
  //    if (!isEmptyCell(moves5[i]))
  //      continue;
  //    if (++move5_count > 2)
  //      break;
  //    move5_pair[move5_count - 1] = i;
  //  }
  //  if (move5_count == 0)
  //    //Все дополнения до линии 5 заблокированы
  //    return false;
  //  if (move5_count >= 2)
  //  {
  //    //вилка 4х4 (выигрыш)
  //    if (defense_variants)
  //    {
  //      //assert(defense_variants->empty());
  //      defense_variants->clear();
  //      //Если у вилки ровно два завершения, то любое из них может быть блокирующим ходом
  //      //Иначе ни одно из них не может быть блокирующим ходом
  //      if (move5_count == 2)
  //      {
  //        defense_variants->push() = moves5[move5_pair[0]];
  //        defense_variants->push() = moves5[move5_pair[1]];
  //      }
  //      //Исходный шах в любом случае может быть блокирующим ходом
  //      defense_variants->push() = move4;
  //    }
  //    return true;
  //  }
}
//bool Gomoku::findVictoryMove4Chain(GPlayer player, uint depth, GBaseStack* defense_variants, GPoint* victory_move)
//{
//  return findVictoryMove4Chain(player, dangerMoves(player).cells(), depth, defense_variants, victory_move);
//}

//bool Gomoku::completeVictoryMove4Chain(GPlayer player, uint depth, GBaseStack* defense_variants, GPoint* victory_move)
//{
//  GStack<32> chain_moves;
//  getChainMoves(chain_moves);
//  return findVictoryMove4Chain(player, chain_moves, depth, defense_variants, victory_move);
//}

//bool Gomoku::findVictoryMove4Chain(GPlayer player, const GBaseStack &variants, uint depth, GBaseStack *defense_variants, GPoint *victory_move)
//{
//  for (const auto& move4: variants)
//  {
//    if (findVictoryMove4Chain(player, move4, depth, defense_variants))
//    {
//      if (victory_move)
//        *victory_move = move4;
//      return true;
//    }
//  }
//  return false;
//}

//bool Gomoku::findVictoryMove4Chain(GPlayer player, const GPoint& move4, uint depth, GBaseStack* defense_variants)
//{
//  if (!isEmptyCell(move4))
//    return false;
//  const auto& move4_data = dangerMoves(player).get(move4);
//  const auto& moves5 = move4_data.m_moves5;
//  uint move5_count = 0;
//  uint move5_pair[2];
//  for (uint i = 0; i < moves5.size(); ++i)
//  {
//    if (!isEmptyCell(moves5[i]))
//      continue;
//    if (++move5_count > 2)
//      break;
//    move5_pair[move5_count - 1] = i;
//  }
//  if (move5_count == 0)
//    //Все дополнения до линии 5 заблокированы
//    return false;
//  if (move5_count >= 2)
//  {
//    //вилка 4х4 (выигрыш)
//    if (defense_variants)
//    {
//      //assert(defense_variants->empty());
//      defense_variants->clear();
//      //Если у вилки ровно два завершения, то любое из них может быть блокирующим ходом
//      //Иначе ни одно из них не может быть блокирующим ходом
//      if (move5_count == 2)
//      {
//        defense_variants->push() = moves5[move5_pair[0]];
//        defense_variants->push() = moves5[move5_pair[1]];
//      }
//      //Исходный шах в любом случае может быть блокирующим ходом
//      defense_variants->push() = move4;
//    }
//    return true;
//  }

//  if (depth > 0)
//  {
//    GMoveMaker gmm(this, player, move4);

//    const GPoint& enemy_block = m_moves5[player].lastCell();
//    if (isDefeatBlock5(!player, enemy_block, depth, defense_variants))
//    {
//      if (defense_variants)
//      {
//        //Как сам шах, так и ответ на него может быть блокирующим ходом
//        defense_variants->push() = move4;
//      }
//      return true;
//    }
//  }

//  return false;
//}

//bool Gomoku::isDefeatBlock5(GPlayer player, const GPoint &block, uint depth, GBaseStack *defense_variants)
//{
//  assert(isEmptyCell(block));
//  assert(depth > 0);

//  GMoveMaker gmm(this, player, block);
//  const auto& block_data = get(block);

//  if (block_data.m_moves5_count > 1)
//    //Блокирующий ход реализует вилку 4х4, поэтому является выигрышным
//    return false;

//  //По возможности продолжаем цепочку шахов противника
//  bool is_victory_enemy_move4;
//  if (block_data.m_moves5_count == 1)
//  {
//    //Блокирующий ход является контршахом
//    const GPoint& enemy_move = m_moves5[player].lastCell();
//    //Блокирующий ход противника тоже может быть контршахом,
//    //в этом случае имеем возможность продолжить цепочку шахов противника
//    is_victory_enemy_move4 = findVictoryMove4Chain(!player, enemy_move, depth - 1, defense_variants);
//  }
//  else
//    is_victory_enemy_move4 = completeVictoryMove4Chain(!player, depth - 1, defense_variants);

//  if (!is_victory_enemy_move4)
//    return false;

//  if (defense_variants)
//  {
//    //Если блокировка реализует линию 3 и есть ход, который развивает ее до шаха,
//    //то такой ход нужно отнести к защитным вариантам,
//    //поскольку противник будет вынужден блокировать контршах вместо продолжения атаки
//    for (uint i = 0; i < block_data.m_moves4.size(); )
//    {
//      const GPoint& move1 = block_data.m_moves4[i++];
//      assert(i < block_data.m_moves4.size());
//      const GPoint& move2 = block_data.m_moves4[i++];
//      assert(isEmptyCell(move1) && isEmptyCell(move2));
//      //Если один из ходов пары является ходом 5,
//      //значит блокирующий ход уже является контршахом,
//      //и оба хода пары не дают никакой дополнительной защиты
//      if (!m_moves5[player].isEmptyCell(move1) || !m_moves5[player].isEmptyCell(move2))
//      {
//        assert(isMove5(player, move1) || isMove5(player, move2));
//        assert(block_data.m_moves5_count == 1);
//        continue;
//      }
//      defense_variants->push() = move1;
//      defense_variants->push() = move2;
//    }
//    //Сама блокировка также является защитным вариантом
//    defense_variants->push() = block;
//  }
//  return true;
//}

int Gomoku::calcMaxAttackWgt(uint depth)
{
  const auto& enemy_move_data = get(lastCell());
  if (enemy_move_data.m_moves5_count > 1)
    return WGT_DEFEAT;
  if (enemy_move_data.m_moves5_count == 1)
  {
    //Ответ на контршах рассматривается на любой глубине
    m_last_attack_move_status = BLOCKING;
    return calcAttackWgt(m_moves5[enemy_move_data.player].lastCell());
  }

  if (depth > maxAttackDepth())
  {
    //Если есть угроза противника, длинная атака не фиксируется
    if (findVictoryMove4Chain(enemy_move_data.player, 100))
    {
      if (findVictoryMove4Chain(!enemy_move_data.player, 100))
        return WGT_VICTORY;
      return 0;
    }
    return WGT_LONG_ATTACK;
  }

  int max_wgt = WGT_DEFEAT;
  GStack<gridSize()> variants;
  if (findVictoryMove4Chain(enemy_move_data.player, 100, &variants))
  {
    bool last_attack_move_status = m_last_attack_move_status;
    m_last_attack_move_status = BLOCKING;

    //Рассматриваем блокирующие ходы
    int wgt = calcMaxAttackWgt(!enemy_move_data.player, variants, depth);
    if (wgt == WGT_VICTORY)
      return WGT_VICTORY;
    if (wgt > max_wgt)
      max_wgt = wgt;

    GPointStack& processed_variants = getAttackGrid(depth);
    for (const GPoint& variant: variants)
      processed_variants.push(variant) = true;

    m_last_attack_move_status = COUNTER_SHAH;
    //В зависимости от статуса предыдущего хода рассматриваем все неблокирующие контршахи или
    //только продолжения цепочки
    variants.clear();
    if (last_attack_move_status == COUNTER_SHAH)
    {
      getChainMoves(variants);
      src_set = &variants;
    }
    else
      src_set
  }

}

//int Gomoku::calcMaxAttackWgt(GPlayer player, uint depth, bool new_attack_chain)
//{
//  if (depth > maxAttackDepth())
//  {
//    bool is_victory = new_attack_chain ?
//      findVictoryMove4Chain(player, 100) :
//      completeVictoryMove4Chain(player, 100);
//    if (is_victory)
//      return WGT_VICTORY;
//    //Если есть угроза выигрышной цепочки шахов противника,
//    //значит продолжить атаку нет возможности
//    if (findVictoryMove4Chain(!player, 100))
//      return 0;
//    return WGT_LONG_ATTACK;
//  }

//  if (new_attack_chain)
//    return calcMaxAttackWgt(player, dangerMoves(player).cells(), depth/*, max_wgt_moves*/);

//  GStack<32> chain_moves;
//  getChainMoves(chain_moves);
//  return calcMaxAttackWgt(player, chain_moves, depth);
//}

int Gomoku::calcMaxAttackWgt(GPlayer player, const GBaseStack& attack_moves, uint depth/*, GBaseStack* max_wgt_moves*/)
{
  int max_wgt = WGT_DEFEAT;

  for (const GPoint* attack_move = attack_moves.end(); attack_move != attack_moves.begin(); )
  {
    --attack_move;
    //Если на предельной глубине атаки получили длинную атаку,
    //для оставшихся вариантов проверяем только, не реализуют ли они выигрышную цепочку шахов,
    //поскольку открытые тройки уже не дадут выигрыш
    if (depth >= maxAttackDepth() && max_wgt > WGT_MIN_LONG_ATTACK)
    {
      if (findVictoryMove4Chain(player, *attack_move, 100))
        return WGT_VICTORY;
    }
    else
    {
      if (!isEmptyCell(*attack_move))
        continue;
      int wgt = calcAttackWgt(player, *attack_move, depth);
      if (updateMaxWgt(*attack_move, wgt, 0, max_wgt) == WGT_VICTORY)
        return WGT_VICTORY;
    }
  }
  return max_wgt;
}

int Gomoku::calcAttackWgt(GPlayer player, const GPoint &move, uint depth)
{
  assert(isEmptyCell(move));

  const auto& move_data = dangerMoves(player).get(move);
  const auto& moves5 = move_data.m_moves5;

  uint moves5_count = 0;
  for (uint i = 0; i < moves5.size(); ++i)
  {
    if (!isEmptyCell(moves5[i]))
      continue;
    if (++moves5_count == 2)
      return WGT_VICTORY;
  }
  if (moves5_count == 0 && !move_data.m_open3)  //ход не является ни шахом, ни полушахом
    return WGT_DEFEAT;

  GMoveMaker gmm(this, player, move);

  int wgt;
  if (moves5_count == 1) //шах
    wgt = -calcDefenseWgt(!player, m_moves5[player].lastCell(), depth, false, true);
  else
  {
    const GMoveData& md = get(move);
    //Опасная открытая тройка должна породить как минимум две пары ходов 4,
    //и среди них хотя бы один должен встретиться дважды
    if (md.m_moves4.size() < 4)
      return WGT_DEFEAT;

    GStack<4> defense_variants;
    for (uint i = 0; i < md.m_moves4.size(); ++i)
    {
      if (findVictoryMove4Chain(player, md.m_moves4[i], 0, &defense_variants))
        break;
    }
    if (defense_variants.empty())
      return WGT_DEFEAT;

    wgt = -calcMaxDefenseWgt(!player, defense_variants, depth, false, true);
  }
  if (wgt == WGT_VICTORY)
    return WGT_VICTORY;
  //Корректируем вес длинной атаки весом ситуации, чтобы выбрать лучший ход из тех,
  //которые порождают длинную атаку
  if (wgt > WGT_MIN_LONG_ATTACK)
    wgt += getStoredWgt(player, move);
  return wgt;
}

int Gomoku::calcForcedAttackWgt(GPlayer player, const GPoint &move, uint depth, bool new_counter_shah_chain)
{
  GMoveMaker gmm(this, player, move);

  const auto& move_data = get(move);
  if (move_data.m_moves5_count > 1)
    return WGT_VICTORY;

  int wgt;
  if (move_data.m_moves5_count == 1)
  {
    const GPoint& enemy_block = m_moves5[player].lastCell();
    //Атакующая цепочка прервана, поэтому передаем false
    wgt = -calcDefenseWgt(!player, enemy_block, depth, true, true);
  }
  else
  {
    GStack<4> defense_variants;
    if (findVictoryMove4Chain(player, 0, &defense_variants))
      wgt = -calcMaxDefenseWgt(!player, defense_variants, depth, true, new_counter_shah_chain);
    else
      wgt = 0;
  }
  if (!isVictoryOrDefeat(wgt))
    wgt += getStoredWgt(player, move);
  return wgt;
}

int Gomoku::calcMaxDefenseWgt(
  GPlayer player,
  const GBaseStack &variants,
  uint depth,
  bool new_attack_chain,
  bool new_counter_shah_chain)
{
  int max_wgt = WGT_DEFEAT;

  //Множество с быстрым поиском для исключения дублирования рассматриваемых вариантов
  GPointStack& processed_variants = getDefenseGrid(depth);

  //Пробуем варианты, заданные на входе
  for (uint i = 0; i < variants.size(); ++i)
  {
    const GPoint& move = variants[i];
    if (!isEmptyCell(move))
      continue;
    if (!processed_variants.isEmptyCell(move))
      continue;
    processed_variants.push(move) = true;
    int wgt = calcDefenseWgt(player, move, depth, new_attack_chain, true);
    //Если ход позволяет блокировать длинную атаку, остальные можно не смотреть
    if (updateMaxWgt(move, wgt, 0, max_wgt) > WGT_MIN_LONG_DEFENSE)
      return wgt;
  }

  //Пробуем контршахи (новая цепочка или продолжение цепочки)
  if (new_counter_shah_chain)
    return calcMaxCounterShahWgt(player, dangerMoves(player).cells(), depth, max_wgt, new_attack_chain, processed_variants);
  else
  {
    GStack<32> chain_moves;
    getChainMoves(chain_moves);
    return calcMaxCounterShahWgt(player, chain_moves, depth, max_wgt, new_attack_chain, processed_variants);
  }
}

int Gomoku::calcMaxCounterShahWgt(
  GPlayer player,
  const GBaseStack &variants,
  uint depth,
  int& max_wgt,
  bool new_attack_chain,
  GPointStack& processed_moves)
{
  for (const GPoint* move = variants.end(); move != variants.begin(); )
  {
    --move;
    if (!processed_moves.isEmptyCell(*move))
      continue;
    if (!isDangerMove4(player, *move))
      continue;
    int wgt = calcDefenseWgt(player, *move, depth, new_attack_chain, false);
    if (updateMaxWgt(*move, wgt, 0, max_wgt) > WGT_MIN_LONG_DEFENSE)
      return max_wgt;
    if (move == variants.begin())
      break;
  }
  return max_wgt;
}

int Gomoku::calcDefenseWgt(
  GPlayer player,
  const GPoint& move,
  uint depth,
  bool new_attack_chain,
  bool new_counter_shah_chain)
{
  assert(isEmptyCell(move));

  GMoveMaker gmm(this, player, move);

  uint moves5_count = get(move).m_moves5_count;

  if (moves5_count > 1)
    return WGT_VICTORY;
  int wgt;
  if (moves5_count == 1)
    wgt = -calcForcedAttackWgt(!player, m_moves5[player].lastCell(), depth + 1, new_counter_shah_chain);
  else
  {
    if (!new_attack_chain)
    {
      //Если защитный ход создает угрозу,
      //атакующую цепочку надо начинать заново
      if (findVictoryMove4Chain(player, 100))
        new_attack_chain = true;
    }
    wgt = -calcMaxAttackWgt(!player, depth + 1, new_attack_chain);
  }
  if (wgt == WGT_DEFEAT)
    return WGT_DEFEAT;
  if (wgt < WGT_MIN_LONG_DEFENSE)
    wgt += get(move).wgt[player];
  return wgt;
}

void Gomoku::getChainMoves(GStack<32>& chain_moves)
{
  assert(chain_moves.empty());
  //Рассматриваем ходы, лежащие на одной линии с предыдущим в заданном направлении
  GPlayer player = curPlayer();
  GPoint move = cells()[cells().size() - 2];
  //Функция вызывается при игре в уме,
  //поэтому есть уверенность, что два последних хода принадлежат разным игрокам
  assert(get(move).player == player);
  for (int i = 0; i < 4; ++i)
  {
    //На направлении должно быть достаточно места для построения пятерки
    if (!isSpace5(player, move, vecs1[i]))
      continue;
    getChainMoves(player, move, vecs1[i], chain_moves);
    getChainMoves(player, move, -vecs1[i], chain_moves);
  }
}

void Gomoku::getChainMoves(GPlayer player, GPoint move, const GVector &v1, GStack<32> &chain_moves)
{
  assert(cells().size() >= 2);
  for (int i = 0; i < 4; ++i)
  {
    move += v1;
    if (!isValidCell(move))
      return;
    int move_player = get(move).player;
    if (move_player == !player)
      return;
    if (move_player == G_EMPTY)
      chain_moves.push() = move;
  }
}

bool Gomoku::isSpace5(GPlayer player, const GPoint& move, const GVector &v1)
{
  int space = 1;
  getSpace(player, move, v1, space);
  getSpace(player, move, -v1, space);
  return space == 5;
}

void Gomoku::getSpace(GPlayer player, GPoint move, const GVector &v1, int &space)
{
  while (space < 5)
  {
    move += v1;
    if (!isValidCell(move) || get(move).player == !player)
      return;
    ++space;
  }
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

  auto& moves5_1 = danger_moves[move1].m_moves5;
  //Одна и та же пара ходов линии 4 может встретиться при одновременной реализации нескольких линий 3
  //(ситуации ххх__х, xx_x_x, xx__xx, x_xx_x)
  //Нужно избежать дублирования при добавлении
  if (std::find(moves5_1.begin(), moves5_1.end(), move2) != moves5_1.end())
  {
    assert(
      std::find(danger_moves[move2].m_moves5.begin(), danger_moves[move2].m_moves5.end(), move1) !=
      danger_moves[move2].m_moves5.end());
    return;
  }
  moves5_1.push() = move2;
  auto& moves5_2 = danger_moves[move2].m_moves5;
  assert(std::find(moves5_2.begin(), moves5_2.end(), move1) == moves5_2.end());
  moves5_2.push() = move1;
  source.pushLine4Moves(move1, move2);
}

void Gomoku::undoMoves4(GMoveData& source)
{
  auto& danger_moves = dangerMoves(source.player);
  GPoint move1, move2;
  while (!source.emptyLine4Moves())
  {
    source.popLine4Moves(move1, move2);
    auto& data2 = danger_moves[move2];
    assert(!data2.m_moves5.empty() && data2.m_moves5.back() == move1);
    data2.m_moves5.pop();
    if (data2.empty())
    {
      assert(danger_moves.cells().back() == move2);
      danger_moves.pop();
    }
    auto& data1 = danger_moves[move1];
    assert(!data1.m_moves5.empty() && data1.m_moves5.back() == move2);
    data1.m_moves5.pop();
    if (data1.empty())
    {
      assert(danger_moves.cells().back() == move1);
      danger_moves.pop();
    }
  }
}

bool Gomoku::isDangerMove4(GPlayer player, const GPoint& move) const
{
  if (!isEmptyCell(move))
    return false;
  const auto& dangerMoveData = m_danger_moves[player].get(move);
  //ход является ходом линии 4, если для него заданы парные и хотя бы один из них не занят
  const auto& moves5 = dangerMoveData.m_moves5;
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
  if (move_data.m_open3)
    return;
  move_data.m_open3 = true;
  ref(lastCell()).pushOpen3(move);
}

void Gomoku::undoOpen3(GMoveData& source)
{
  auto& danger_moves = dangerMoves(source.player);

  GPoint open3_move;
  while (!source.m_open3_moves.empty())
  {
    source.popOpen3(open3_move);
    auto& open3_data = danger_moves[open3_move];
    assert(open3_data.m_open3);
    open3_data.m_open3 = false;
    if (open3_data.empty())
    {
      assert(danger_moves.cells().back() == open3_move);
      danger_moves.pop();
    }
  }
}

bool Gomoku::isDangerOpen3(GPlayer player, const GPoint& move, GBaseStack* defense_variants)
{
  if (!isEmptyCell(move))
    return false;
  const auto& danger_move_data = dangerMoves(player).get(move);
  if (!danger_move_data.m_open3)
    return false;
  GMoveMaker gmm(this, player, move);

  const GMoveData& md = get(move);
  //Опасная открытая тройка должна породить как минимум две пары ходов 4,
  //и среди них хотя бы один должен встретиться дважды
  if (md.m_moves4.size() < 4)
    return false;
  for (uint i = 0; i < md.m_moves4.size(); ++i)
  {
    if (findVictoryMove4Chain(player, md.m_moves4[i], 0, defense_variants))
      return true;
  }
  return false;
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

GPoint Gomoku::randomMove(const GBaseStack& moves)
{
  return moves[random(0, moves.size())];
}

} //namespace nsg
