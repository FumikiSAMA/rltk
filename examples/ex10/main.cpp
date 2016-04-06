/* RLTK (RogueLike Tool Kit) 1.00
 * Copyright (c) 2016-Present, Bracket Productions.
 * Licensed under the LGPL - see LICENSE file.
 *
 * Example 10: Not really an example yet, playing with getting the ECS working.
 */

// You need to include the RLTK header
#include "../../rltk/rltk.hpp"

// We're using a stringstream to build the hello world message.
#include <sstream>
#include <algorithm>

// For convenience, import the whole rltk namespace. You may not want to do this
// in larger projects, to avoid naming collisions.
using namespace rltk;
using namespace rltk::colors;
using std::size_t;

constexpr int map_width = 100;
constexpr int map_height = 100;
constexpr int map_size = map_width * map_height;
int map_idx(const int x, const int y) { return (y * map_width) + x; }
std::vector<int> map_tiles;
std::vector<bool> visible;
std::vector<bool> revealed;
random_number_generator rng;

size_t player_id;

struct position { 
	int x, y; 
	void bounds_check() {
		if (x < 0) x = 0;
		if (x > map_width) x = map_width;
		if (y < 0) y = 0;
		if (y > map_height) y = map_height;
	}
};
struct renderable { int glyph; color_t fg=colors::WHITE; color_t bg=colors::BLACK; };

struct navigator_helper {
	static int get_x(const position &loc) { return loc.x; }
	static int get_y(const position &loc) { return loc.y; }
	static position get_xy(const int &x, const int &y) { return position{x,y}; }
};

// Clipping info
int left_x, right_x, top_y, bottom_y;

struct camera_system : public base_system {
	virtual void configure() override {
		// Create the player
		auto player = create_entity()
			->assign(position{map_width/2, map_height/2})
			->assign(renderable{'@', colors::YELLOW});
		player_id = player->id;
	}

	virtual void update(const double duration_ms) override {
		// In this case, we just want to print "Hello World" in white on black.
		if (console->dirty) {
			console->clear();

			// Find the camera
			position * camera_loc = entity(player_id)->component<position>();
			left_x = camera_loc->x - (console->term_width / 2);
			right_x = camera_loc->x + (console->term_width / 2);
			top_y = camera_loc->y - (console->term_height/2);
			bottom_y = camera_loc->y + (console->term_height/2)+1;

			for (int y=top_y; y<bottom_y; ++y) {
				for (int x=left_x; x<right_x; ++x) {
					if (x >= 0 && x < map_width && y >= 0 && y < map_height) {
						vchar map_char{'.', colors::DARKEST_GREY, colors::BLACK};
						const int map_index = map_idx(x,y);

						if (revealed[map_index]) {
							switch (map_tiles[map_index]) {
								case 0 : map_char.glyph = '.'; break;
								case 1 : map_char.glyph = '#'; break;
								default : map_char.glyph = 'E'; // This shouldn't happen
							}
							map_char.foreground = colors::GREY;
						}
						if (visible[map_index]) map_char.foreground = colors::WHITE;
						console->set_char(x-left_x, y-top_y, map_char);
					}
				}
			}
		}		
	}
};

struct actor_render_system : public base_system {
	virtual void update(const double duration_ms) override {
		each<position, renderable>([] (entity_t &entity, position &pos, renderable &render) {
			const int map_index = map_idx(pos.x, pos.y);
			if (visible[map_index]) {
				console->set_char(pos.x-left_x, pos.y-top_y, vchar{ render.glyph, render.fg, render.bg });
			}
		});
	}
};

struct player_system : public base_system {
	double time_since_press = 100.0;
	bool moved = true;

	virtual void update(const double duration_ms) override {
		time_since_press += duration_ms;
		position * camera_loc = entity(player_id)->component<position>();
		position camera_loc_deref = *camera_loc;

		// Add a pause between key presses
		if (time_since_press > 2.0) {
			// The whole "moved" system is destined to become a message-based system
			if (moved) {
				std::fill(visible.begin(), visible.end(), false);
				visibility_sweep_2d<position, navigator_helper>(camera_loc_deref, 10, 
					[] (position reveal) {
						reveal.bounds_check();
						visible[map_idx(reveal.x, reveal.y)] = true;
						revealed[map_idx(reveal.x, reveal.y)] = true;
					}, 
					[] (position visibility_check) { 
						visibility_check.bounds_check();
						return (map_tiles[map_idx(visibility_check.x, visibility_check.y)] == 0);
				});				

				moved = false;
			}

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left) && map_tiles[map_idx(camera_loc->x-1, camera_loc->y)]==0 ) {
				if (camera_loc->x > 0) camera_loc->x--;
				console->dirty = true; // To be replaced with a message when we have them
				time_since_press = 0.0;
				moved = true; // Also to be a message!
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right) && map_tiles[map_idx(camera_loc->x+1, camera_loc->y)]==0 ) {
				if (camera_loc->x < map_width) camera_loc->x++;
				console->dirty = true;
				time_since_press = 0.0;
				moved = true; // Also to be a message!
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up) && map_tiles[map_idx(camera_loc->x, camera_loc->y-1)]==0 ) {
				if (camera_loc->y > 0) camera_loc->y--;
				console->dirty = true;
				time_since_press = 0.0;
				moved = true; // Also to be a message!
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down) && map_tiles[map_idx(camera_loc->x, camera_loc->y+1)]==0 ) {
				if (camera_loc->y < map_width) camera_loc->y++;
				console->dirty = true;
				time_since_press = 0.0;
				moved = true; // Also to be a message!
			}
		}
	}
};

// Tick is called every frame. The parameter specifies how many ms have elapsed
// since the last time it was called.
void tick(double duration_ms) {
	ecs_tick(duration_ms);
}

// Your main function
int main()
{
	// Initialize the library. Here, we are providing plenty of into so you can see what is
	// available. There's also the option to use config_simple_px to configure by pixels
	// rather than characters.
	// The first parameter is the path to the font files.
	// The second and third parameters specify the desired console size in screen characters (80x25).
	// The fourth parameter is the window title.
	// The final parameter says that we'd like the default console to use an 8x16 VGA font. Not so great for games, but easy to read!
	init(config_simple("../assets", 80, 50, "RLTK Hello World", "8x8"));
	
	// Zero the map other than the edges
	map_tiles.resize(map_size);
	visible.resize(map_size);
	revealed.resize(map_size);
	std::fill(map_tiles.begin(), map_tiles.end(), 0);
	std::fill(visible.begin(), visible.end(), false);
	std::fill(revealed.begin(), revealed.end(), false);
	for (int i=0; i<map_width; ++i) {
		map_tiles[map_idx(i, 0)] = 1;
		map_tiles[map_idx(i, map_height-1)] = 1;
	}
	for (int i=0; i<map_width; ++i) {
		map_tiles[map_idx(0, i)] = 1;
		map_tiles[map_idx(map_width-1, i)] = 1;
	}
	// Random debris
	for (int y=1; y<map_height-1; ++y) {
		for (int x=1; x<map_width-1; ++x) {
			if (rng.roll_dice(1,4)==1 && (x!=map_width/2 || y!=map_height/2)) map_tiles[map_idx(x,y)]=1;
		}
	}

	// Create our systems
	add_system(std::make_unique<player_system>());
	add_system(std::make_unique<camera_system>());
	add_system(std::make_unique<actor_render_system>());

	// Enter the main loop. "tick" is the function we wrote above.
	ecs_configure();
	run(tick);

    return 0;
}
