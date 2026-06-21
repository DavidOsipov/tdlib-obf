// SPDX-FileCopyrightText: Copyright 2026 telemt community
// SPDX-License-Identifier: MIT
// telemt: https://github.com/telemt
// telemt: https://t.me/telemtrs
//

#include "td/telegram/BackportTestSeams.h"
#include "td/telegram/Dependencies.h"

#include "td/utils/tests.h"

namespace {

TEST(ManagedBotCreatedDependencyRuntime, ActionContentRegistersBotUserIdDependencyAtRuntime) {
  td::Dependencies dependencies;
  td::add_managed_bot_created_dependencies(dependencies, td::UserId(static_cast<td::int64>(777001)));

  ASSERT_TRUE(dependencies.get_user_ids().count(td::UserId(static_cast<td::int64>(777001))) == 1);
}

TEST(ManagedBotCreatedDependencyRuntime, MinUserIdsExposeManagedBotReferenceAtRuntime) {
  auto user_ids = td::get_managed_bot_created_min_user_ids(td::UserId(static_cast<td::int64>(777002)));
  ASSERT_EQ(1u, user_ids.size());
  ASSERT_EQ(td::UserId(static_cast<td::int64>(777002)), user_ids[0]);
}

TEST(ManagedBotCreatedDependencyRuntime, InvalidBotUserIdIsIgnoredByDependencySeam) {
  td::Dependencies dependencies;
  td::add_managed_bot_created_dependencies(dependencies, td::UserId());

  ASSERT_TRUE(dependencies.get_user_ids().empty());
}

TEST(ManagedBotCreatedDependencyRuntime, DuplicateBotUserIdCollapsesToSingleDependency) {
  td::Dependencies dependencies;
  auto bot_user_id = td::UserId(static_cast<td::int64>(777003));

  td::add_managed_bot_created_dependencies(dependencies, bot_user_id);
  td::add_managed_bot_created_dependencies(dependencies, bot_user_id);

  ASSERT_EQ(1u, dependencies.get_user_ids().size());
  ASSERT_TRUE(dependencies.get_user_ids().count(bot_user_id) == 1);
}

}  // namespace
