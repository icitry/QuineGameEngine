#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>

short is_running = 1;

typedef enum {
	KEY_UP = 'w',
	KEY_DOWN = 's',
	KEY_LEFT = 'a',
	KEY_RIGHT = 'd',
	KEY_QUIT = 'q'
} key_t;

typedef enum {
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
	DIR_NONE,
	NUM_DIRS
} dir_t;

typedef enum {
	GHOST_BLINKY,
	GHOST_PINKY,
	GHOST_INKY,
	GHOST_CLYDE,
	NUM_GHOSTS
} ghost_type_t;

typedef enum {
	STATE_NONE,
	STATE_CHASE,
	STATE_SCATTER,
	STATE_FRIGHTENED,
	NUM_STATES
} ghost_state_t;

typedef enum {
	TILE_EMPTY,
	TILE_GHOST_BLINKY,
	TILE_GHOST_PINKY,
	TILE_GHOST_INKY,
	TILE_GHOST_CLYDE,
	TILE_PACMAN,
	TILE_WALL,
	TILE_POINT,
	TILE_ENERGIZER,
	TILE_HEART
} tile_type_t;

typedef struct {
	int tick;
} event_t;

typedef struct {
	short x;
	short y;
} vector_2d_t;

typedef struct {
	tile_type_t type;
	tile_type_t default_type;
	short is_active;
} tile_t;

typedef struct {
	vector_2d_t dir;
	vector_2d_t pos;
} entity_state_t;

typedef struct {
	entity_state_t entity_state;
	ghost_type_t type;
	ghost_state_t state;

	vector_2d_t target;

	int release_threshold;

	event_t last_frightened;
	event_t last_eaten;

	event_t last_chase;
	event_t last_scatter;

	short state_cycles_completed;
} ghost_t;

typedef struct {
	entity_state_t entity_state;
} pacman_t;

static struct {
	struct {
		int current_tick;
		event_t next_tick;
	} time;

	struct {
		int score;
		short num_lives;
		short level;
		float level_multiplier;

		ghost_t ghosts[NUM_GHOSTS];
		pacman_t pacman;

		tile_t** tiles;
		tile_t** heart_tiles;
		vector_2d_t* heart_tiles_pos;
		vector_2d_t* pending_tile_updates;
		int num_pending_tile_updates;

		short remaining_point_tiles;

		int xorshift;
	} state;

	struct {
		short window_width;
		short window_height;

		short ticks_per_second;
		short skip_ticks;

		short pacman_max_lives;
		short total_point_tiles;

		vector_2d_t pacman_start_pos;
		vector_2d_t ghost_start_pos[NUM_GHOSTS];
		vector_2d_t ghost_scatter_target_pos[NUM_GHOSTS];

		vector_2d_t dirs[NUM_DIRS];
	} def_vals;
} game;

static int xorshift32(void) {
	int x = game.state.xorshift;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return game.state.xorshift = x;
}

static vector_2d_t vector_2d_add(vector_2d_t a, vector_2d_t b) {
	return (vector_2d_t) { .x = a.x + b.x, .y = a.y + b.y };
}

static vector_2d_t vector_2d_sub(vector_2d_t a, vector_2d_t b) {
	return (vector_2d_t) { .x = a.x - b.x, .y = a.y - b.y };
}

static short vector_2d_eq(vector_2d_t a, vector_2d_t b) {
	return a.x == b.x && a.y == b.y;
}

static int vector_2d_euclidean_distance(vector_2d_t a, vector_2d_t b) {
	return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

static vector_2d_t vector_2d_mul_scalar(vector_2d_t a, short scalar) {
	return (vector_2d_t) { .x = a.x * scalar, .y = a.y * scalar };
}

static vector_2d_t reverse_dir(vector_2d_t dir) {
	return vector_2d_mul_scalar(dir, -1);
}

static vector_2d_t clamp_vector_2d(vector_2d_t a, int min_val_x, int max_val_x, int min_val_y, int max_val_y) {
	return (vector_2d_t) { .x = a.x < min_val_x ? max_val_x : a.x > max_val_x ? min_val_x : a.x, .y = a.y < min_val_y ? max_val_y : a.y > max_val_y ? min_val_y : a.y };
}

static vector_2d_t gen_random_target() {
	return (vector_2d_t) {
		.x = xorshift32() % game.def_vals.window_width, .y = xorshift32() % game.def_vals.window_height
	};
}

void cleanup() {
	for (int i = 0; i < game.def_vals.window_height; i++) free(game.state.tiles[i]);
	free(game.state.tiles);
	free(game.state.heart_tiles);
	free(game.state.heart_tiles_pos);
	free(game.state.pending_tile_updates);
}

static float calculate_level_multiplier(short level) {
	return 1 + level * 0.1;
}

static void clear_screen() {
	system("cls");
}

static void init_def_vals() {
	game.def_vals.window_width = 28;
	game.def_vals.window_height = 36;
	game.def_vals.ticks_per_second = 60;
	game.def_vals.skip_ticks = 16;
	game.def_vals.pacman_max_lives = 3;

	game.def_vals.pacman_start_pos = (vector_2d_t){ .x = 13, .y = game.def_vals.window_height - 10 };

	game.def_vals.ghost_start_pos[GHOST_BLINKY] = (vector_2d_t){ .x = 13, .y = 16 };
	game.def_vals.ghost_start_pos[GHOST_PINKY] = (vector_2d_t){ .x = 13, .y = 17 };
	game.def_vals.ghost_start_pos[GHOST_INKY] = (vector_2d_t){ .x = 11, .y = 17 };
	game.def_vals.ghost_start_pos[GHOST_CLYDE] = (vector_2d_t){ .x = 15, .y = 17 };

	game.def_vals.ghost_scatter_target_pos[GHOST_BLINKY] = (vector_2d_t){ .x = game.def_vals.window_width - 3, .y = 0 };
	game.def_vals.ghost_scatter_target_pos[GHOST_PINKY] = (vector_2d_t){ .x = 2, .y = 0 };
	game.def_vals.ghost_scatter_target_pos[GHOST_INKY] = (vector_2d_t){ .x = game.def_vals.window_width - 1, .y = game.def_vals.window_height - 2 };
	game.def_vals.ghost_scatter_target_pos[GHOST_CLYDE] = (vector_2d_t){ .x = 0, .y = game.def_vals.window_height - 2 };

	game.def_vals.dirs[DIR_NONE] = (vector_2d_t){ .x = 0, .y = 0 };
	game.def_vals.dirs[DIR_UP] = (vector_2d_t){ .x = 0, .y = -1 };
	game.def_vals.dirs[DIR_DOWN] = (vector_2d_t){ .x = 0, .y = 1 };
	game.def_vals.dirs[DIR_LEFT] = (vector_2d_t){ .x = -1, .y = 0 };
	game.def_vals.dirs[DIR_RIGHT] = (vector_2d_t){ .x = 1, .y = 0 };
}

static void init_ghosts() {
	game.state.ghosts[GHOST_BLINKY] = (ghost_t){
		.entity_state = (entity_state_t) {.pos = (vector_2d_t){.x = 13, .y = 14 }, .dir = game.def_vals.dirs[DIR_LEFT] },
		.type = GHOST_BLINKY,
		.state = STATE_NONE,
		.target = game.def_vals.ghost_scatter_target_pos[GHOST_BLINKY],
		.release_threshold = game.state.score,
		.last_frightened.tick = -1,
		.last_eaten.tick = -1,
		.last_chase.tick = -1,
		.last_scatter.tick = -1,
		.state_cycles_completed = 0
	};
	game.state.ghosts[GHOST_PINKY] = (ghost_t){
		.entity_state = (entity_state_t) {.pos = game.def_vals.ghost_start_pos[GHOST_PINKY], .dir = game.def_vals.dirs[DIR_NONE] },
		.type = GHOST_PINKY,
		.state = STATE_NONE,
		.target = game.def_vals.ghost_scatter_target_pos[GHOST_PINKY],
		.release_threshold = game.state.score,
		.last_frightened.tick = -1,
		.last_eaten.tick = -1,
		.last_chase.tick = -1,
		.last_scatter.tick = -1,
		.state_cycles_completed = 0
	};
	game.state.ghosts[GHOST_INKY] = (ghost_t){
		.entity_state = (entity_state_t) {.pos = game.def_vals.ghost_start_pos[GHOST_INKY], .dir = game.def_vals.dirs[DIR_NONE] },
		.type = GHOST_INKY,
		.state = STATE_NONE,
		.target = game.def_vals.ghost_scatter_target_pos[GHOST_INKY],
		.release_threshold = 30 / game.state.level_multiplier + game.state.score,
		.last_frightened.tick = -1,
		.last_eaten.tick = -1,
		.last_chase.tick = -1,
		.last_scatter.tick = -1,
		.state_cycles_completed = 0
	};
	game.state.ghosts[GHOST_CLYDE] = (ghost_t){
		.entity_state = (entity_state_t) {.pos = game.def_vals.ghost_start_pos[GHOST_CLYDE], .dir = game.def_vals.dirs[DIR_NONE] },
		.type = GHOST_CLYDE,
		.state = STATE_NONE,
		.target = game.def_vals.ghost_scatter_target_pos[GHOST_CLYDE],
		.release_threshold = 60 / game.state.level_multiplier + game.state.score,
		.last_frightened.tick = -1,
		.last_eaten.tick = -1,
		.last_chase.tick = -1,
		.last_scatter.tick = -1,
		.state_cycles_completed = 0
	};
}

static void init_pacman() {
	game.state.pacman = (pacman_t){
		.entity_state = (entity_state_t) {.pos = game.def_vals.pacman_start_pos, .dir = game.def_vals.dirs[DIR_NONE] }
	};
}

static void set_tile(char c, tile_t* tile, short x, short y) {
	tile->type = TILE_EMPTY;
	tile->default_type = TILE_EMPTY;
	tile->is_active = 1;

	switch (c)
	{
	case '#':
		tile->type = TILE_WALL;
		tile->default_type = TILE_WALL;
		break;
	case '.':
		tile->type = TILE_POINT;
		tile->default_type = TILE_POINT;
		game.def_vals.total_point_tiles++;
		break;
	case '@':
		tile->type = TILE_ENERGIZER;
		tile->default_type = TILE_ENERGIZER;
		break;
	case 'o':
		if (game.state.num_lives >= game.def_vals.pacman_max_lives) break;
		tile->type = TILE_HEART;
		tile->default_type = TILE_HEART;

		game.state.heart_tiles[game.state.num_lives] = tile;
		game.state.heart_tiles_pos[game.state.num_lives] = (vector_2d_t){.x = x, .y = y};
		game.state.num_lives++;
		break;
	default:
		break;
	}
}

static void init_tiles() {
	const char* tilemap =
		"                            "
		"                            "
		"                            "
		"############################"
		"#............##............#"
		"#.####.#####.##.#####.####.#"
		"#@#  #.#   #.##.#   #.#  #@#"
		"#.####.#####.##.#####.####.#"
		"#..........................#"
		"#.####.##.########.##.####.#"
		"#.####.##.########.##.####.#"
		"#......##....##....##......#"
		"######.##### ## #####.######"
		"     #.##### ## #####.#     "
		"     #.##          ##.#     "
		"     #.## ###  ### ##.#     "
		"######.## #      # ##.######"
		"      .   #      #   .      "
		"######.## #      # ##.######"
		"     #.## ######## ##.#     "
		"     #.##          ##.#     "
		"     #.## ######## ##.#     "
		"######.## ######## ##.######"
		"#............##............#"
		"#.####.#####.##.#####.####.#"
		"#.####.#####.##.#####.####.#"
		"#@..##.......  .......##..@#"
		"###.##.##.########.##.##.###"
		"###.##.##.########.##.##.###"
		"#......##....##....##......#"
		"#.##########.##.##########.#"
		"#.##########.##.##########.#"
		"#..........................#"
		"############################"
		" ooo                        "
		"                            ";

	game.state.tiles = (tile_t**)malloc(game.def_vals.window_height * sizeof(tile_t*));
	game.state.heart_tiles = (tile_t**)malloc(game.def_vals.pacman_max_lives * sizeof(tile_t*));
	game.state.heart_tiles_pos = (vector_2d_t*)malloc(game.def_vals.pacman_max_lives * sizeof(vector_2d_t));
	for (int i = 0; i < game.def_vals.window_height; i++) game.state.tiles[i] = (tile_t*)malloc(game.def_vals.window_width * sizeof(tile_t));
	game.state.pending_tile_updates = (vector_2d_t*)malloc((game.def_vals.window_width * game.def_vals.window_height) * sizeof(vector_2d_t));
	game.state.num_pending_tile_updates = 0;

	if (game.state.tiles == NULL || game.state.pending_tile_updates == NULL) {
		cleanup();
		exit(-1);
	}

	game.def_vals.total_point_tiles = 0;
	for (int i = 0; i < game.def_vals.window_height; i++) {
		for (int j = 0; j < game.def_vals.window_width; j++) {	
			int tile_idx = i * game.def_vals.window_width + j;
			set_tile(tilemap[tile_idx], &game.state.tiles[i][j], j, i);
		}
	}
}

static void reset_tiles() {
	game.state.num_pending_tile_updates = 0;

	for (int i = 0; i < game.def_vals.window_height; i++) {
		for (int j = 0; j < game.def_vals.window_width; j++) {
			tile_t* tile = &game.state.tiles[i][j];
			tile->type = tile->default_type;
			tile->is_active = 1;
		}
	}
}

static void init_level(short level) {
	game.state.level = level;
	game.state.level_multiplier = calculate_level_multiplier(game.state.level);

	if (level == 0) {
		init_def_vals();

		game.time.current_tick = 0;
		game.time.next_tick.tick = 0;
		game.state.score = 0;
		game.state.num_lives = 0;
		game.state.xorshift = 0x12345678;

		init_tiles();
	}
	else {
		reset_tiles();
	}

	game.state.remaining_point_tiles = game.def_vals.total_point_tiles;

	init_ghosts();
	init_pacman();
}

static char get_tile_repr(tile_t* tile) {
	switch (tile->type)
	{
	case TILE_EMPTY:
		return ' ';
		break;
	case TILE_GHOST_BLINKY:
		return 'B';
		break;
	case TILE_GHOST_PINKY:
		return 'P';
		break;
	case TILE_GHOST_INKY:
		return 'I';
		break;
	case TILE_GHOST_CLYDE:
		return 'C';
		break;
	case TILE_PACMAN:
		return 'O';
		break;
	case TILE_WALL:
		return '#';
		break;
	case TILE_POINT:
		if (tile->is_active) return '.';
		else return ' ';
		break;
	case TILE_ENERGIZER:
		if (tile->is_active) return '@';
		else return ' ';
		break;
	case TILE_HEART:
		if (tile->is_active) return 'o';
		else return ' ';
		break;
	default:
		return ' ';
		break;
	}
}

static void render_tiles() {
	for (int i = 0; i < game.def_vals.window_height; i++) {
		for (int j = 0; j < game.def_vals.window_width; j++) {
			printf("%c", get_tile_repr(&game.state.tiles[i][j]));
		}
		printf("\n");
	}
}

static void frighten_ghosts() {
	for (ghost_type_t type = GHOST_BLINKY; type < NUM_GHOSTS; type++) {
		if(game.state.ghosts[type].state == STATE_NONE) continue;
		if (game.state.ghosts[type].state == STATE_SCATTER || game.state.ghosts[type].state == STATE_CHASE)
			game.state.ghosts[type].entity_state.dir = reverse_dir(game.state.ghosts[type].entity_state.dir);

		game.state.ghosts[type].state = STATE_FRIGHTENED;
		game.state.ghosts[type].last_frightened.tick = game.time.current_tick;
	}
}

static void eat_ghost(ghost_type_t type) {
	game.state.ghosts[type].state = STATE_NONE;
	game.state.ghosts[type].entity_state.dir = game.def_vals.dirs[DIR_NONE];
	game.state.ghosts[type].entity_state.pos = game.def_vals.ghost_start_pos[type];
	game.state.ghosts[type].last_eaten.tick = game.time.current_tick;
	game.state.score += 10;
}

static short check_pacman_collisions(vector_2d_t new_pos) {
	switch (game.state.tiles[new_pos.y][new_pos.x].type)
	{
	case TILE_WALL:
		return 2;
		break;
	case TILE_POINT:
		if (game.state.tiles[new_pos.y][new_pos.x].is_active) {
			game.state.tiles[new_pos.y][new_pos.x].is_active = 0;
			return 1;
		}
		break;
	case TILE_ENERGIZER:
		if (game.state.tiles[new_pos.y][new_pos.x].is_active) {
			game.state.tiles[new_pos.y][new_pos.x].is_active = 0;
			frighten_ghosts();
			return 1;
		}
		break;
	case TILE_GHOST_BLINKY:
		if (game.state.ghosts[GHOST_BLINKY].state == STATE_FRIGHTENED) {
			eat_ghost(GHOST_BLINKY);
			return 1;
		}
		else return -1;
		break;
	case TILE_GHOST_PINKY:
		if (game.state.ghosts[GHOST_PINKY].state == STATE_FRIGHTENED) {
			eat_ghost(GHOST_PINKY);
			return 1;
		}
		else return -1;
		break;
	case TILE_GHOST_INKY:
		if (game.state.ghosts[GHOST_INKY].state == STATE_FRIGHTENED) {
			eat_ghost(GHOST_INKY);
			return 1;
		}
		else return -1;
		break;
	case TILE_GHOST_CLYDE:
		if (game.state.ghosts[GHOST_CLYDE].state == STATE_FRIGHTENED) {
			eat_ghost(GHOST_CLYDE);
			return 1;
		}
		else return -1;
		break;
	default:
		break;
	}

	return 0;
}

static void update_pacman_pos(vector_2d_t new_pos) {
	int p_x = game.state.pacman.entity_state.pos.x, p_y = game.state.pacman.entity_state.pos.y;

	game.state.tiles[p_y][p_x].type = game.state.tiles[p_y][p_x].default_type;
	game.state.pending_tile_updates[game.state.num_pending_tile_updates++] = (vector_2d_t){ .x = p_x, .y = p_y };

	game.state.pacman.entity_state.pos = new_pos;
	p_x = game.state.pacman.entity_state.pos.x, p_y = game.state.pacman.entity_state.pos.y;

	game.state.tiles[p_y][p_x].type = TILE_PACMAN;
	game.state.pending_tile_updates[game.state.num_pending_tile_updates++] = (vector_2d_t){ .x = p_x, .y = p_y };
}

static void pacman_lose_life() {
	if (game.state.num_lives == 0) {
		is_running = 0;
		return;
	}

	game.state.num_lives--;
	game.state.heart_tiles[game.state.num_lives]->is_active = 0;
	vector_2d_t tile_pos = game.state.heart_tiles_pos[game.state.num_lives];
	game.state.pending_tile_updates[game.state.num_pending_tile_updates++] = (vector_2d_t){ .x = tile_pos.x, .y = tile_pos.y };

	if (game.state.num_lives == 0) {
		is_running = 0;
		return;
	}

	update_pacman_pos(game.def_vals.pacman_start_pos);
}

static void update_pacman() {
	int p_x = game.state.pacman.entity_state.pos.x, p_y = game.state.pacman.entity_state.pos.y;
	vector_2d_t new_pos = vector_2d_add(game.state.pacman.entity_state.pos, game.state.pacman.entity_state.dir);
	new_pos = clamp_vector_2d(new_pos, 0, game.def_vals.window_width - 1, 0, game.def_vals.window_height - 1);

	short collision_result = check_pacman_collisions(new_pos);

	if (collision_result == 2) {
		return;
	}
	else if (collision_result == 1) {
		game.state.score++;
		game.state.remaining_point_tiles--;

		if (game.state.remaining_point_tiles == 0) {
			init_level(game.state.level + 1);
			clear_screen();
			render_tiles();
			return;
		}
	}
	else if (collision_result == -1) {
		pacman_lose_life();
		return;
	}
	
	update_pacman_pos(new_pos);
}

static void navigate_ghost_state_cycle(ghost_t* ghost, short scatter_duration_s, short chase_duration_s) {
	if (scatter_duration_s > 0) {
		if (ghost->state == STATE_SCATTER && game.time.current_tick - ghost->last_scatter.tick >= scatter_duration_s * game.def_vals.ticks_per_second) {
			ghost->state = STATE_CHASE;
			ghost->last_chase.tick = game.time.current_tick;
		}
	}
	if (chase_duration_s > 0) {
		if (ghost->state == STATE_CHASE && game.time.current_tick - ghost->last_chase.tick >= chase_duration_s * game.def_vals.ticks_per_second) {
			ghost->state = STATE_SCATTER;
			ghost->last_scatter.tick = game.time.current_tick;
			ghost->state_cycles_completed++;
		}
	}
}

static void update_ghost_state(ghost_t* ghost) {
	if (ghost->release_threshold > game.state.score) return; 

	ghost_state_t old_state = ghost->state;

	if (ghost->state == STATE_NONE) {
		if (ghost->last_eaten.tick == -1 || game.time.current_tick - ghost->last_eaten.tick > 6 * game.def_vals.ticks_per_second) {
			ghost->state = STATE_SCATTER;
			ghost->last_scatter.tick = game.time.current_tick;
		}
		return;
	}

	if (ghost->state == STATE_FRIGHTENED) {
		if (game.time.current_tick - ghost->last_frightened.tick > 6 * game.def_vals.ticks_per_second) {
			ghost->last_scatter.tick += game.time.current_tick - ghost->last_frightened.tick;
			ghost->last_chase.tick += game.time.current_tick - ghost->last_frightened.tick;
		}
		else return;
	}

	switch (ghost->state_cycles_completed)
	{
	case 0:
	case 1:
		navigate_ghost_state_cycle(ghost, 7 / game.state.level_multiplier, 20);
		break;
	case 2:
		navigate_ghost_state_cycle(ghost, 5 / game.state.level_multiplier, 20);
		break;
	case 3:
		navigate_ghost_state_cycle(ghost, 5 / game.state.level_multiplier, -1);
		break;
	default:
		break;
	}

	if (old_state != ghost->state && (old_state == STATE_SCATTER || old_state == STATE_CHASE))
		ghost->entity_state.dir = reverse_dir(ghost->entity_state.dir);
}

static void update_ghost_target(ghost_t* ghost) {
	entity_state_t pacman_state = game.state.pacman.entity_state;

	vector_2d_t curr_pos = ghost->entity_state.pos;

	if (curr_pos.x >= 13 && curr_pos.x <= 14 && curr_pos.y >= 16 && curr_pos.y <= 18) {
		ghost->target = (vector_2d_t){.x = 13, .y=15};
		return;
	}

	switch (ghost->state)
	{
	case STATE_SCATTER:
		ghost->target = game.def_vals.ghost_scatter_target_pos[ghost->type];
		break;
	case STATE_CHASE:
		switch (ghost->type) {
		case GHOST_BLINKY:
			ghost->target = pacman_state.pos;
			break;
		case GHOST_PINKY:
			ghost->target = vector_2d_add(pacman_state.pos, vector_2d_mul_scalar(pacman_state.dir, 4));
			break;
		case GHOST_INKY:
		{
			vector_2d_t blinky_pos = game.state.ghosts[GHOST_BLINKY].entity_state.pos;
			vector_2d_t p = vector_2d_add(pacman_state.pos, vector_2d_mul_scalar(pacman_state.dir, 2));
			vector_2d_t d = vector_2d_sub(p, blinky_pos);
			ghost->target = vector_2d_add(blinky_pos, vector_2d_mul_scalar(d, 4));
		}
		break;
		case GHOST_CLYDE:
			if (vector_2d_euclidean_distance(ghost->entity_state.pos, pacman_state.pos) > 64)
				ghost->target = pacman_state.pos;
			else
				ghost->target = game.def_vals.ghost_scatter_target_pos[GHOST_CLYDE];
		default:
			break;
		}
	case STATE_FRIGHTENED:
		ghost->target = gen_random_target();
		break;
	default:
		break;
	}
}

static short check_ghost_collisions(ghost_t* ghost, vector_2d_t new_pos) {
	switch (game.state.tiles[new_pos.y][new_pos.x].type) {
	case TILE_WALL:
		return 2;
		break;
	case TILE_PACMAN:
		if (ghost->state == STATE_FRIGHTENED) {
			eat_ghost(ghost->type);
			return -1;
		}
		else {
			pacman_lose_life();
			return 1;
		}
	default:
		break;
	}
	return 0;
}

static short is_redzone(vector_2d_t pos) {
	return pos.x >= 10 && pos.x <= 17 && (pos.y == 14 || pos.y == 26);
}

static void update_ghost_pos(ghost_t* ghost) {
	if (ghost->state == STATE_NONE) return;

	update_ghost_target(ghost);

	vector_2d_t new_pos = vector_2d_add(ghost->entity_state.pos, vector_2d_mul_scalar(ghost->entity_state.dir, game.state.level_multiplier));
	new_pos = clamp_vector_2d(new_pos, 0, game.def_vals.window_width - 1, 0, game.def_vals.window_height - 1);

	short collision_result = check_ghost_collisions(ghost, new_pos);

	if (collision_result == -1 || collision_result == 2) return;

	ghost->entity_state.pos = new_pos;

	int min_dist = 100000;
	int dist = 0;
	vector_2d_t original_dir_reverse = reverse_dir(ghost->entity_state.dir);

	for (int i = 0; i < NUM_DIRS; i++) {
		if (i == DIR_NONE) continue;
		vector_2d_t dir = game.def_vals.dirs[i];
		if (vector_2d_eq(original_dir_reverse, dir)) continue;
		if (is_redzone(ghost->entity_state.pos) && (i == DIR_UP)) continue;

		vector_2d_t test_pos = vector_2d_add(ghost->entity_state.pos, vector_2d_mul_scalar(dir, game.state.level_multiplier));
		test_pos = clamp_vector_2d(test_pos, 0, game.def_vals.window_width - 1, 0, game.def_vals.window_height - 1);

		short test_collision_result = check_ghost_collisions(ghost, test_pos);

		if (test_collision_result != -1 && test_collision_result != 2) {
			if ((dist = vector_2d_euclidean_distance(test_pos, ghost->target)) < min_dist) {
				min_dist = dist;
				ghost->entity_state.dir = dir;
			}
		}
	}
}

static void update_ghost_repr(ghost_t* ghost, vector_2d_t old_pos) {
	short pos_x = ghost->entity_state.pos.x, pos_y = ghost->entity_state.pos.y;

	game.state.tiles[old_pos.y][old_pos.x].type = game.state.tiles[old_pos.y][old_pos.x].default_type;
	game.state.pending_tile_updates[game.state.num_pending_tile_updates++] = (vector_2d_t){ .x = old_pos.x, .y = old_pos.y };

	switch (ghost->type)
	{
	case GHOST_BLINKY:
		game.state.tiles[pos_y][pos_x].type = TILE_GHOST_BLINKY;
		break;
	case GHOST_PINKY:
		game.state.tiles[pos_y][pos_x].type = TILE_GHOST_PINKY;
		break;
	case GHOST_INKY:
		game.state.tiles[pos_y][pos_x].type = TILE_GHOST_INKY;
		break;
	case GHOST_CLYDE:
		game.state.tiles[pos_y][pos_x].type = TILE_GHOST_CLYDE;
		break;
	default:
		break;
	}

	game.state.pending_tile_updates[game.state.num_pending_tile_updates++] = (vector_2d_t){ .x = ghost->entity_state.pos.x, .y = ghost->entity_state.pos.y };
}

static void update_ghost(ghost_type_t type) {
	ghost_t* ghost = &game.state.ghosts[type];
	vector_2d_t old_pos = ghost->entity_state.pos;

	update_ghost_state(ghost);
	update_ghost_pos(ghost);
	update_ghost_repr(ghost, old_pos);
}

static void update_ghosts() {
	update_ghost(GHOST_BLINKY);
	update_ghost(GHOST_PINKY);
	update_ghost(GHOST_INKY);
	update_ghost(GHOST_CLYDE);
}

static void on_game_tick() {
	update_ghosts();
	update_pacman();
}

static void on_frame_render() {
	printf("\033[0;0H");
	for (int i = 0; i < game.state.num_pending_tile_updates; i++) {
		short pos_x = game.state.pending_tile_updates[i].x, pos_y = game.state.pending_tile_updates[i].y;
		printf("\033[%d;%dH%c", pos_y + 1, pos_x + 1, get_tile_repr(&game.state.tiles[pos_y][pos_x]));
	}
	game.state.num_pending_tile_updates = 0;
}

static DWORD WINAPI input_thread_main(LPVOID lpParam) {
	char ch;
	while (is_running) {
		ch = _getch();
		switch (ch)
		{
		case KEY_QUIT:
			is_running = 0;
			break;
		case KEY_UP:
			game.state.pacman.entity_state.dir = game.def_vals.dirs[DIR_UP];
			break;
		case KEY_DOWN:
			game.state.pacman.entity_state.dir = game.def_vals.dirs[DIR_DOWN];
			break;
		case KEY_LEFT:
			game.state.pacman.entity_state.dir = game.def_vals.dirs[DIR_LEFT];
			break;
		case KEY_RIGHT:
			game.state.pacman.entity_state.dir = game.def_vals.dirs[DIR_RIGHT];
			break;
		default:
			break;
		}
	}
	return 0;
}

static void run() {
	HANDLE thread;
	DWORD thread_id;

	thread = CreateThread(NULL, 0, input_thread_main, NULL, 0, &thread_id);

	if (thread == NULL) {
		printf("Error creating input thread: %d\n", GetLastError());
		cleanup();
		exit(-1);
	}

	int sleep_time = 0;

	clear_screen();
	render_tiles();

	while (is_running) {
		game.time.current_tick++;
		while (game.time.current_tick > game.time.next_tick.tick) {
			on_game_tick();
			game.time.next_tick.tick += game.def_vals.skip_ticks;
		}
		on_frame_render();
		sleep_time = game.time.next_tick.tick - game.time.current_tick;
		if (sleep_time >= 0) {
			Sleep(sleep_time);
		}
	}

	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);

	clear_screen();
	cleanup();
	printf("FINAL SCORE: %d\n", game.state.score);
}

int main(void)
{
	init_level(0);
	run();
}
