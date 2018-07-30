#include "eosdayeosday.hpp"

uint64_t price_config[11][2] = {{10000, 100},
                                {10000 * 10, 200},
                                {10000 * 20, 400},
                                {10000 * 40, 800},
                                {10000 * 80, 1600},
                                {10000 * 160, 3200},
                                {10000 * 320, 6400},
                                {10000 * 640, 12800},
                                {10000 * 1280, 25600},
                                {10000 * 1300, 1000 * 10000},
                                {10000 * 100000, 10000 * 10000}};

uint64_t get_new_price(uint64_t sold_keys) {
  for (int i = 0; i < 9; i++) {
    if (sold_keys < price_config[i][0]) {
      return price_config[i][1];
    }
  }
  return 0;
}

uint64_t get_same_level_max_keys(uint64_t sold_keys) {
  for (int i = 0; i < 9; i++) {
    if (sold_keys < price_config[i][0]) {
      return price_config[i][0] - sold_keys;
    }
  }
  return 0;
}

uint64_t get_new_key_num(uint64_t amount, uint64_t sold_keys) {
  uint64_t price = get_new_price(sold_keys);
  eosio_assert(price > 0, "price must be great than 0");
  uint64_t max_keys = get_same_level_max_keys(sold_keys);
  eosio_assert(max_keys > 0, "max keys must be great than 0");
  uint64_t same_level_max_amount = max_keys * price;
  if (amount <= same_level_max_amount) {
    return amount / price;
  } else {
    return max_keys + get_new_key_num(amount - same_level_max_amount,
                                      sold_keys + max_keys);
  }
}

void win_diamond::withdraw(const account_name account, asset quantity) {

  // find user
  auto itr = _balance.find(account);
  eosio_assert(itr != _balance.end(), "user does not exist");

  // set quantity
  eosio_assert(quantity.amount + itr->balance > quantity.amount,
               "integer overflow adding withdraw balance");
  quantity.amount += itr->balance;

  // clear balance
  _balance.modify(itr, _this_contract, [&](auto &p) { p.balance = 0; });

  // withdraw
  action(permission_level{_this_contract, N(active)}, N(eosio.token),
         N(transfer), std::make_tuple(_this_contract, account, quantity,
                                      std::string("from eosday.io")))
      .send();
}

void win_diamond::on(const currency::transfer &t, account_name code) {

  ping();

  // return if transfer is from current contract
  if (t.from == _this_contract)
    return;

  // transfer must be EOS token from eosio.token
  eosio_assert(code == N(eosio.token), "transfer not from eosio.token");
  eosio_assert(t.to == _this_contract, "transfer not made to this contract");

  // if transfer amount is 0.0001 EOS then withdraw
  if (t.quantity.amount == 1) {
    withdraw(t.from, t.quantity);
    return;
  }

  symbol_type symbol{S(4, EOS)};
  eosio_assert(t.quantity.symbol == symbol, "asset must be EOS");
  eosio_assert(t.quantity.is_valid(), "invalid quantity");

  auto user = t.from;
  auto amount = t.quantity.amount;

  // retrieve counter
  auto counter = _counters.begin();
  const auto sold_keys = counter->sold_keys;

  // new keys and price
  const uint64_t new_keys = get_new_key_num(amount, sold_keys);
  eosio_assert(new_keys > 0, "new keys must be one or more");
  const uint64_t price = get_new_price(sold_keys + new_keys);
  eosio_assert(price > 0, "price must be great than 0");

  // pay dividend
  const uint64_t dividend =
      _players.begin() == _players.end() ? 0 : (amount * DIVIDEND_SIZE);
  for (auto itr = _players.begin(); itr != _players.end(); itr++) {

    auto share = dividend * ((double)itr->keys / (double)counter->sold_keys);
    auto useritr = _balance.find(itr->player_name);

    if (useritr == _balance.end()) {
      useritr = _balance.emplace(
          _this_contract, [&](auto &p) { p.username = itr->player_name; });
    }

    _balance.modify(useritr, _this_contract, [&](auto &p) {
      eosio_assert(p.total + share > p.total,
                   "integer overflow adding user balance total");
      p.total += share;
      eosio_assert(p.balance + share > p.balance,
                   "integer overflow adding user balance balance");
      p.balance += share;
    });
  }

  // pay team reward
  const uint64_t team_reward = amount * TEAM_REWARD_SIZE;
  auto team = _balance.find(team_account);
  if (team == _balance.end()) {
    team = _balance.emplace(_this_contract,
                            [&](auto &p) { p.username = team_account; });
  }
  _balance.modify(team, _this_contract, [&](auto &p) {
    eosio_assert(p.total + team_reward > p.total,
                 "integer overflow adding team reward total");
    p.total += team_reward;
    eosio_assert(p.balance + team_reward > p.balance,
                 "integer overflow adding team reward balance");
    p.balance += team_reward;
  });

  // find user
  auto itr = _players.find(user);
  if (itr == _players.end()) {
    itr = _players.emplace(_this_contract,
                           [&](auto &p) { p.player_name = user; });
  }

  // update tables
  _players.modify(itr, _this_contract, [&](auto &p) {
    eosio_assert(p.keys + new_keys > p.keys,
                 "integer overflow adding new keys");
    p.keys += new_keys;
    eosio_assert(p.amount + t.quantity.amount > p.amount,
                 "integer overflow adding player amount");
    p.amount += t.quantity.amount;
  });

  _counters.modify(counter, _this_contract, [&](auto &p) {
    p.last_buyer = user;
    p.last_buy_time = now();
    p.end_time =
        std::min(p.end_time + TIME_INC, p.last_buy_time + MAX_TIME_INC);

    eosio_assert(p.balance + t.quantity.amount > p.balance,
                 "integer overflow adding counter balance");
    p.balance += t.quantity.amount;

    eosio_assert(p.pot + t.quantity.amount - dividend - team_reward > p.pot,
                 "integer overflow adding pot");
    p.pot += t.quantity.amount - dividend - team_reward;

    eosio_assert(p.sold_keys + new_keys > p.sold_keys,
                 "integer overflow adding sold_keys");
    p.sold_keys += new_keys;

    p.key_price = price;
  });
}

void win_diamond::ping() {
  auto counter = _counters.begin();

  // game ends
  if (counter->end_time <= now()) {

    // get winner account
    auto winner = counter->last_buyer;

    // find winner entry in _balance
    auto witr = _balance.find(winner);
    if (witr == _balance.end()) {
      witr = _balance.emplace(_this_contract,
                              [&](auto &p) { p.username = winner; });
    }

    // update winner entry in _balance
    _balance.modify(witr, _this_contract, [&](auto &p) {
      eosio_assert(p.total + counter->pot > p.total,
                   "integer overflow adding winner total");
      p.total += counter->pot;
      eosio_assert(p.balance + counter->pot > p.balance,
                   "integer overflow adding winner balance");
      p.balance += counter->pot;
    });

    // erase tables
    for (auto itr = _counters.begin(); itr != _counters.end();)
      itr = _counters.erase(itr);
    for (auto itr = _players.begin(); itr != _players.end();)
      itr = _players.erase(itr);

    // start new game
    _counters.emplace(_this_contract, [&](auto &p) {
      p.owner = _this_contract;
      p.last_buyer = _this_contract;
    });
  }
}

void win_diamond::hi(account_name user) {
  // user auth
  require_auth(user);
}

void win_diamond::erase() {

  // user auth
  require_auth(_this_contract);

  // erase tables
  for (auto itr = _counters.begin(); itr != _counters.end();)
    itr = _counters.erase(itr);
  for (auto itr = _players.begin(); itr != _players.end();)
    itr = _players.erase(itr);
  for (auto itr = _balance.begin(); itr != _balance.end();)
    itr = _balance.erase(itr);
}

void win_diamond::maintain() {

  // user auth
  require_auth(_this_contract);
}

void win_diamond::apply(account_name contract, account_name act) {

  if (act == N(transfer)) {
    on(unpack_action_data<currency::transfer>(), contract);
    return;
  }

  if (contract != _this_contract)
    return;

  auto &thiscontract = *this;
  switch (act) { EOSIO_API(win_diamond, (ping)(hi)(erase)(maintain)); };
}

extern "C" {
[[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  win_diamond gtb(receiver);
  gtb.apply(code, action);
  eosio_exit(0);
}
}
