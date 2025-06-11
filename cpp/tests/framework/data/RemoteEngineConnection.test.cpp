// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include <catch2/catch_all.hpp>

using namespace robotick;

TEST_CASE("Unit|Framework|Data|RemoteEngineConnection|Initial state and construction", "[RemoteEngineConnection]")
{
	RemoteEngineConnection conn("ip:localhost:3456", RemoteEngineConnection::Mode::Proactive);

	REQUIRE_FALSE(conn.is_connected());
	REQUIRE_FALSE(conn.is_ready_for_tick());
}
