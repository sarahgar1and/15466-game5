#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

void Player::Controls::send_controls_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 5;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b) {
		if (b.downs & 0x80) {
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t( (b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f) ) );
	};

	send_button(left);
	send_button(right);
	send_button(up);
	send_button(down);
	send_button(jump);
}

bool Player::Controls::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	//expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size != 5) throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 5!");
	
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	auto recv_button = [](uint8_t byte, Button *button) {
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255) {
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	recv_button(recv_buffer[4+0], &left);
	recv_button(recv_buffer[4+1], &right);
	recv_button(recv_buffer[4+2], &up);
	recv_button(recv_buffer[4+3], &down);
	recv_button(recv_buffer[4+4], &jump);

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}


//-----------------------------------------

Game::Game() : mt(0x15466666) {
}

Player *Game::spawn_player() {
	// Pair players into opponents
	int opponent_number = 0;
	if (next_player_number % 2 == 0){ 
		// Opponent is previous player
		opponent_number = next_player_number - 1;
	}
	else {
		// Needs to wait for another player to join
		opponent_number = next_player_number + 1;
	}
	players.emplace_back();
	Player &player = players.back();

	player.opponent_name = "Player " + std::to_string(opponent_number);

	//random point in the middle area of the arena:
	player.position.x = glm::mix(ArenaMin.x + 2.0f * PlayerRadius, ArenaMax.x - 2.0f * PlayerRadius, 0.4f + 0.2f * mt() / float(mt.max()));
	player.position.y = glm::mix(ArenaMin.y + 2.0f * PlayerRadius, ArenaMax.y - 2.0f * PlayerRadius, 0.4f + 0.2f * mt() / float(mt.max()));

	player.name = "Player " + std::to_string(next_player_number++);

	std::cout << player.name << "'s opponent is " << player.opponent_name << std::endl;

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed) {
	//position/velocity update:
	for (auto &p : players) {
		glm::vec2 move = glm::vec2(0.0f);

		if (p.controls.left.pressed && !p.controls.right.pressed && p.dir.x != 1.0f){
			p.dir.x = -1.0f;
			p.dir.y = 0.0f;
		} 
		if (!p.controls.left.pressed && p.controls.right.pressed && p.dir.x != -1.0f){ 
			p.dir.x = 1.0f;
			p.dir.y = 0.0f;
		}
		if (p.controls.down.pressed && !p.controls.up.pressed && p.dir.y != 1.0f){
			p.dir.x = 0.0f;
			p.dir.y = -1.0f;
		}
		if (!p.controls.down.pressed && p.controls.up.pressed && p.dir.y != -1.0f){
			p.dir.x = 0.0f;
			p.dir.y = 1.0f;
		} 

		move = p.dir * PlayerSpeed * elapsed;

		p.position.x += move.x;
		p.position.y += move.y;

		//reset 'downs' since controls have been handled:
		p.controls.left.downs = 0;
		p.controls.right.downs = 0;
		p.controls.up.downs = 0;
		p.controls.down.downs = 0;
		p.controls.jump.downs = 0;
	}

	//collision resolution:
	for (auto &p1 : players) {
		// player/player collisions:

		// for (auto &p2 : players) {
		// 	if (&p1 == &p2) break;
		// 	if (p1.opponent_name == p2.name){
		// 		assert(p2.opponent_name == p1.name);

		// 		glm::vec2 p12 = p2.position - p1.position;
		// 		float len2 = glm::length2(p12);
		// 		if (len2 > (2.0f * PlayerRadius) * (2.0f * PlayerRadius)) continue;
		// 		if (len2 == 0.0f) continue;
		// 		std::cout << p1.name << " collided with opponent " << p2.name << std::endl;
		// 	}
			
		// }

		// player/arena collisions:
		if (p1.position.x < ArenaMin.x + PlayerRadius) {
			p1.position.x = ArenaMin.x + PlayerRadius;
		}
		if (p1.position.x > ArenaMax.x - PlayerRadius) {
			p1.position.x = ArenaMax.x - PlayerRadius;
		}
		if (p1.position.y < ArenaMin.y + PlayerRadius) {
			p1.position.y = ArenaMin.y + PlayerRadius;
		}
		if (p1.position.y > ArenaMax.y - PlayerRadius) {
			p1.position.y = ArenaMax.y - PlayerRadius;
		}

		// player/crumb collision:
		glm::vec2 p1c = crumb_pos - p1.position;
		float len2 = glm::length2(p1c);
		if (len2 > (2.0f * PlayerRadius) * (2.0f * CrumbRadius)) continue;
		if (len2 == 0.0f) continue;
		 
		// std::cout << p1.name << " collided with the crumb!" << std::endl;

		p1.crumbs += 1;

		// Generate new crumb position
		crumb_pos.x = glm::mix(ArenaMin.x + 2.0f * CrumbRadius, ArenaMax.x - 2.0f * CrumbRadius, mt() / float(mt.max()));
		crumb_pos.y = glm::mix(ArenaMin.y + 2.0f * CrumbRadius, ArenaMax.y - 2.0f * CrumbRadius, mt() / float(mt.max()));

	}

}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	//send player info helper:
	auto send_player = [&](Player const &player) {
		connection.send(player.position);
		connection.send(player.dir);
		//NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
		//effectively: truncates player name to 255 chars

		uint8_t opponent_len = uint8_t(std::min< size_t >(255, player.opponent_name.size()));
		connection.send(opponent_len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.opponent_name.begin(), player.opponent_name.begin() + opponent_len);

		uint8_t len = uint8_t(std::min< size_t >(255, player.name.size()));
		connection.send(len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		if (&player == connection_player) continue;
		send_player(player);
	}

	// crumb position
	connection.send(crumb_pos);

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//copy bytes from buffer and advance position:
	auto read = [&](auto *val) {
		if (at + sizeof(*val) > size) {
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read(&player.position);
		read(&player.dir);
		
		uint8_t opponent_name_len;
		read(&opponent_name_len);
		player.opponent_name = "";
		for (uint8_t n = 0; n < opponent_name_len; ++n) {
			char c;
			read(&c);
			player.opponent_name += c;
		}

		uint8_t name_len;
		read(&name_len);
		//n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
		player.name = "";
		for (uint8_t n = 0; n < name_len; ++n) {
			char c;
			read(&c);
			player.name += c;
		}
	}

	read(&crumb_pos);

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
