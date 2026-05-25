// main.c
// Dungeon Maze Adventure - Real-time Mode + Score + Cross Attack Effect + Spawn Enemy Patrol + Key Door + Free Attack Cooldown
// Compile: gcc main.c -O2 -o dungeon.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <time.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#define MAX_H 300
#define MAX_W 600
#define MAX_ENEMY 100 
#define MAX_SPAWNER 10

#define DEFAULT_SPAWN_INTERVAL 25
#define DEFAULT_CHASE_RANGE 10
#define DEFAULT_PATROL_RANGE 8
#define DEFAULT_SAFE_TURNS 3
#define DEFAULT_ENEMY_DAMAGE 100
#define DEFAULT_GAME_TICK_MS 40
#define DEFAULT_ENEMY_MOVE_INTERVAL 2
#define DEFAULT_FREE_ATTACK_COOLDOWN 5
#define DEFAULT_PLAYER_HP 1000
#define DEFAULT_ENEMY_LIMIT 20

#define ENEMY_NORMAL 0
#define ENEMY_SPAWNED 1
#define AXIS_HORIZONTAL 0
#define AXIS_VERTICAL 1
#define SPAWN_PATROL_MIN_LENGTH 5

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD  "\033[1m"

static int g_spawnInterval = DEFAULT_SPAWN_INTERVAL;                 // 敵人生成間隔，數字越小生成越快
static int g_chaseRange = DEFAULT_CHASE_RANGE;                       // 敵人偵測玩家範圍
static int g_patrolRange = DEFAULT_PATROL_RANGE;                     // 敵人巡邏範圍
static int g_safeTurns = DEFAULT_SAFE_TURNS;                         // 時間暫停秒數
static int g_enemyDamage = DEFAULT_ENEMY_DAMAGE;                     // 敵人碰撞傷害
static int g_gameTickMs = DEFAULT_GAME_TICK_MS;                      // 遊戲刷新速度，數字越小越快
static int g_enemyMoveInterval = DEFAULT_ENEMY_MOVE_INTERVAL;        // 敵人移動間隔，數字越小敵人越快
static int g_freeAttackCooldown = DEFAULT_FREE_ATTACK_COOLDOWN;      // 免費十字攻擊冷卻秒數
static int g_playerHP = DEFAULT_PLAYER_HP;                           // 玩家初始血量
static int g_enemyLimit = DEFAULT_ENEMY_LIMIT;						 // 敵人生成數量上限

typedef struct {
	int x;
	int y;
	int homeX;
	int homeY;
	int alive;
	int type;
	int dir;
	int axis;
	int wasChasing;
} Enemy;

typedef struct {
	int x;
	int y;
	int dir;
	int axis;
} Spawner;

static void sleep_ms(int ms) {
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	usleep((useconds_t)ms * 1000);
#endif
}

static void clear_screen(void) {
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD pos = {0, 0};
	SetConsoleCursorPosition(hOut, pos);
#else
	printf("\033[H");
#endif
}

#ifdef _WIN32
static void enable_virtual_terminal(void) {
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (hOut == INVALID_HANDLE_VALUE) {
		return;
	}

	DWORD dwMode = 0;

	if (!GetConsoleMode(hOut, &dwMode)) {
		return;
	}

	dwMode |= 0x0004;
	SetConsoleMode(hOut, dwMode);
}
#endif

static void pause_exit(void) {
#ifdef _WIN32
	system("pause");
#else
	printf("Press Enter to exit...");
	getchar();
#endif
}

static void trim_newline(char s[]) {
	int len = (int)strlen(s);

	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
		s[len - 1] = '\0';
		len--;
	}
}

static int ends_with_txt(const char filename[]) {
	int len = (int)strlen(filename);

	if (len < 4) {
		return 0;
	}

	return (
		(filename[len - 4] == '.') &&
		(filename[len - 3] == 't' || filename[len - 3] == 'T') &&
		(filename[len - 2] == 'x' || filename[len - 2] == 'X') &&
		(filename[len - 1] == 't' || filename[len - 1] == 'T')
	);
}

static void normalize_filename(char filename[], const char defaultName[]) {
	trim_newline(filename);

	if (strlen(filename) == 0) {
		strcpy(filename, defaultName);
		return;
	}

	if (!ends_with_txt(filename)) {
		strcat(filename, ".txt");
	}
}

static int file_exists(const char filename[]) {
	FILE *fp = fopen(filename, "r");

	if (fp) {
		fclose(fp);
		return 1;
	}

	return 0;
}

static int read_maze(const char *filename, char maze[MAX_H][MAX_W], int *H, int *W) {
	FILE *fp = fopen(filename, "r");

	if (!fp) {
		return 0;
	}

	char line[MAX_W + 5];
	int h = 0;
	int w = -1;

	while (fgets(line, sizeof(line), fp)) {
		size_t len = strlen(line);

		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}

		if (len == 0) {
			continue;
		}

		if (h >= MAX_H) {
			fclose(fp);
			return 0;
		}

		if (w == -1) {
			w = (int)len;
		}

		if ((int)len != w) {
			fclose(fp);
			return 0;
		}

		if (w >= MAX_W) {
			fclose(fp);
			return 0;
		}

		for (int j = 0; j < w; j++) {
			maze[h][j] = line[j];
		}

		maze[h][w] = '\0';
		h++;
	}

	fclose(fp);

	if (h <= 0 || w <= 0) {
		return 0;
	}

	*H = h;
	*W = w;

	return 1;
}

static int can_go(char maze[MAX_H][MAX_W], int H, int W, int i, int j) {
	if (i < 0 || i >= H || j < 0 || j >= W) {
		return 0;
	}

	return maze[i][j] != '1' && maze[i][j] != 'D';
}

static int distance(int x1, int y1, int x2, int y2) {
	return abs(x1 - x2) + abs(y1 - y2);
}

static int is_enemy_position(Enemy enemies[], int enemyCount, int x, int y) {
	for (int i = 0; i < enemyCount; i++) {
		if (enemies[i].alive == 1 && enemies[i].x == x && enemies[i].y == y) {
			return 1;
		}
	}

	return 0;
}

static int is_enemy_position_except(Enemy enemies[], int enemyCount, int enemyIndex, int x, int y) {
	for (int i = 0; i < enemyCount; i++) {
		if (i != enemyIndex && enemies[i].alive == 1 && enemies[i].x == x && enemies[i].y == y) {
			return 1;
		}
	}

	return 0;
}

static int hit_enemy_and_damage_player(Enemy enemies[], int enemyCount, int playerX, int playerY, int *HP) {
	for (int i = 0; i < enemyCount; i++) {
		if (enemies[i].alive == 1 && enemies[i].x == playerX && enemies[i].y == playerY) {
			enemies[i].alive = 0;
			*HP -= g_enemyDamage;
			return 1;
		}
	}

	return 0;
}

static int count_alive_enemies(Enemy enemies[], int enemyCount) {
	int count = 0;

	for (int i = 0; i < enemyCount; i++) {
		if (enemies[i].alive == 1) {
			count++;
		}
	}

	return count;
}

static int count_line_space(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	int x,
	int y,
	int dx,
	int dy
) {
	int count = 0;
	int nextX = x + dx;
	int nextY = y + dy;

	while (can_go(maze, H, W, nextX, nextY)) {
		count++;
		nextX += dx;
		nextY += dy;
	}

	return count;
}

static void setup_spawner_patrol(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Spawner *spawner
) {
	int leftSpace = count_line_space(maze, H, W, spawner->x, spawner->y, 0, -1);
	int rightSpace = count_line_space(maze, H, W, spawner->x, spawner->y, 0, 1);
	int upSpace = count_line_space(maze, H, W, spawner->x, spawner->y, -1, 0);
	int downSpace = count_line_space(maze, H, W, spawner->x, spawner->y, 1, 0);

	int horizontalSpace = leftSpace + rightSpace;
	int verticalSpace = upSpace + downSpace;

	if (horizontalSpace >= SPAWN_PATROL_MIN_LENGTH || horizontalSpace >= verticalSpace) {
		spawner->axis = AXIS_HORIZONTAL;

		if (spawner->y < W / 2) {
			spawner->dir = 1;
		}
		else {
			spawner->dir = -1;
		}
	}
	else {
		spawner->axis = AXIS_VERTICAL;

		if (spawner->x < H / 2) {
			spawner->dir = 1;
		}
		else {
			spawner->dir = -1;
		}
	}
}

static int get_safe_remaining(time_t safeEndTime) {
	time_t now = time(NULL);

	if (safeEndTime <= now) {
		return 0;
	}

	return (int)(safeEndTime - now);
}

static int get_free_attack_cooldown_remaining(time_t freeAttackEndTime) {
	time_t now = time(NULL);

	if (freeAttackEndTime <= now) {
		return 0;
	}

	return (int)(freeAttackEndTime - now);
}

static void set_all_enemy_home_to_current(Enemy enemies[], int enemyCount) {
	for (int i = 0; i < enemyCount; i++) {
		if (enemies[i].alive == 1) {
			enemies[i].homeX = enemies[i].x;
			enemies[i].homeY = enemies[i].y;
			enemies[i].wasChasing = 0;
		}
	}
}

static int spawn_enemy(
	Enemy enemies[],
	int *enemyCount,
	Spawner spawners[],
	int spawnerCount,
	int playerX,
	int playerY
) {
	if (spawnerCount == 0) {
		return 0;
	}

	if (count_alive_enemies(enemies, *enemyCount) >= g_enemyLimit) {
		return 0;
	}

	int start = rand() % spawnerCount;

	for (int t = 0; t < spawnerCount; t++) {
		int index = (start + t) % spawnerCount;
		int sx = spawners[index].x;
		int sy = spawners[index].y;

		if (sx == playerX && sy == playerY) {
			continue;
		}

		if (is_enemy_position(enemies, *enemyCount, sx, sy)) {
			continue;
		}

		int slot = -1;

		for (int i = 0; i < *enemyCount; i++) {
			if (enemies[i].alive == 0) {
				slot = i;
				break;
			}
		}

		if (slot == -1) {
			if (*enemyCount >= MAX_ENEMY) {
				return 0;
			}

			slot = *enemyCount;
			(*enemyCount)++;
		}

		enemies[slot].x = sx;
		enemies[slot].y = sy;
		enemies[slot].homeX = sx;
		enemies[slot].homeY = sy;
		enemies[slot].alive = 1;
		enemies[slot].type = ENEMY_SPAWNED;
		enemies[slot].dir = spawners[index].dir;
		enemies[slot].axis = spawners[index].axis;
		enemies[slot].wasChasing = 0;

		return 1;
	}

	return 0;
}

static void clear_effect(char effect[MAX_H][MAX_W], int H, int W) {
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			effect[i][j] = '0';
		}
	}
}

static int cross_attack(
	char maze[MAX_H][MAX_W],
	char effect[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int playerX,
	int playerY
) {
	int killCount = 0;

	int dx[4] = {-1, 1, 0, 0};
	int dy[4] = {0, 0, -1, 1};

	clear_effect(effect, H, W);

	for (int dir = 0; dir < 4; dir++) {
		int x = playerX + dx[dir];
		int y = playerY + dy[dir];

		while (can_go(maze, H, W, x, y)) {
			effect[x][y] = '*';

			for (int i = 0; i < enemyCount; i++) {
				if (enemies[i].alive == 1 && enemies[i].x == x && enemies[i].y == y) {
					enemies[i].alive = 0;
					killCount++;
				}
			}

			x += dx[dir];
			y += dy[dir];
		}
	}

	return killCount;
}

static void move_one_enemy_to_target(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int enemyIndex,
	int targetX,
	int targetY
) {
	if (enemies[enemyIndex].alive == 0) {
		return;
	}

	int bestX = enemies[enemyIndex].x;
	int bestY = enemies[enemyIndex].y;
	int bestDist = distance(enemies[enemyIndex].x, enemies[enemyIndex].y, targetX, targetY);

	int dx[4] = {-1, 1, 0, 0};
	int dy[4] = {0, 0, -1, 1};

	for (int k = 0; k < 4; k++) {
		int nextX = enemies[enemyIndex].x + dx[k];
		int nextY = enemies[enemyIndex].y + dy[k];

		if (can_go(maze, H, W, nextX, nextY)) {
			if (!is_enemy_position_except(enemies, enemyCount, enemyIndex, nextX, nextY)) {
				int d = distance(nextX, nextY, targetX, targetY);

				if (d < bestDist) {
					bestDist = d;
					bestX = nextX;
					bestY = nextY;
				}
			}
		}
	}

	enemies[enemyIndex].x = bestX;
	enemies[enemyIndex].y = bestY;
}

static void move_one_enemy_patrol(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int enemyIndex
) {
	if (enemies[enemyIndex].alive == 0) {
		return;
	}

	int possible[4][2];
	int count = 0;

	int dx[4] = {-1, 1, 0, 0};
	int dy[4] = {0, 0, -1, 1};

	for (int k = 0; k < 4; k++) {
		int nextX = enemies[enemyIndex].x + dx[k];
		int nextY = enemies[enemyIndex].y + dy[k];

		if (can_go(maze, H, W, nextX, nextY)) {
			if (!is_enemy_position_except(enemies, enemyCount, enemyIndex, nextX, nextY)) {
				if (distance(nextX, nextY, enemies[enemyIndex].homeX, enemies[enemyIndex].homeY) <= g_patrolRange) {
					possible[count][0] = nextX;
					possible[count][1] = nextY;
					count++;
				}
			}
		}
	}

	if (count > 0) {
		int index = rand() % count;
		enemies[enemyIndex].x = possible[index][0];
		enemies[enemyIndex].y = possible[index][1];
	}
}

static void move_one_spawned_enemy_patrol(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int enemyIndex
) {
	if (enemies[enemyIndex].alive == 0) {
		return;
	}

	int nextX = enemies[enemyIndex].x;
	int nextY = enemies[enemyIndex].y;

	if (enemies[enemyIndex].axis == AXIS_HORIZONTAL) {
		nextY += enemies[enemyIndex].dir;
	}
	else {
		nextX += enemies[enemyIndex].dir;
	}

	if (can_go(maze, H, W, nextX, nextY)) {
		if (!is_enemy_position_except(enemies, enemyCount, enemyIndex, nextX, nextY)) {
			enemies[enemyIndex].x = nextX;
			enemies[enemyIndex].y = nextY;
			return;
		}
	}

	enemies[enemyIndex].dir *= -1;

	nextX = enemies[enemyIndex].x;
	nextY = enemies[enemyIndex].y;

	if (enemies[enemyIndex].axis == AXIS_HORIZONTAL) {
		nextY += enemies[enemyIndex].dir;
	}
	else {
		nextX += enemies[enemyIndex].dir;
	}

	if (can_go(maze, H, W, nextX, nextY)) {
		if (!is_enemy_position_except(enemies, enemyCount, enemyIndex, nextX, nextY)) {
			enemies[enemyIndex].x = nextX;
			enemies[enemyIndex].y = nextY;
		}
	}
}

static void move_all_enemies_ai(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int playerX,
	int playerY
) {
	for (int i = 0; i < enemyCount; i++) {
		if (enemies[i].alive == 1) {
			if (distance(enemies[i].x, enemies[i].y, playerX, playerY) <= g_chaseRange) {
				enemies[i].wasChasing = 1;
				move_one_enemy_to_target(maze, H, W, enemies, enemyCount, i, playerX, playerY);
			}
			else {
				if (enemies[i].wasChasing == 1) {
					enemies[i].homeX = enemies[i].x;
					enemies[i].homeY = enemies[i].y;
					enemies[i].wasChasing = 0;

					if (enemies[i].type == ENEMY_SPAWNED) {
						enemies[i].type = ENEMY_NORMAL;
						enemies[i].dir = 0;
						enemies[i].axis = AXIS_HORIZONTAL;
					}
				}

				if (enemies[i].type == ENEMY_SPAWNED) {
					move_one_spawned_enemy_patrol(maze, H, W, enemies, enemyCount, i);
				}
				else {
					move_one_enemy_patrol(maze, H, W, enemies, enemyCount, i);
				}
			}
		}
	}
}

static void generate_new_safe_zone(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int playerX,
	int playerY
) {
	int emptyCells[MAX_H * MAX_W][2];
	int count = 0;

	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			if (maze[i][j] == '0') {
				if (!(i == playerX && j == playerY) && !is_enemy_position(enemies, enemyCount, i, j)) {
					emptyCells[count][0] = i;
					emptyCells[count][1] = j;
					count++;
				}
			}
		}
	}

	if (count > 0) {
		int index = rand() % count;
		int zX = emptyCells[index][0];
		int zY = emptyCells[index][1];

		maze[zX][zY] = 'Z';
	}
}

static void open_all_doors(char maze[MAX_H][MAX_W], int H, int W) {
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			if (maze[i][j] == 'D') {
				maze[i][j] = '0';
			}
		}
	}
}

static void print_cell_colored(char c) {
	switch (c) {
		case '1':
			printf("\033[100m ");
			break;

		case '0':
			printf("\033[40m ");
			break;

		case '#':
			printf("\033[42m ");
			break;

		case '$':
			printf("\033[40;33m$");
			break;

		case '*':
			printf("\033[40;35m*");
			break;

		case 'Z':
			printf("\033[46m ");
			break;

		case '@':
			printf("\033[48;5;208m ");
			break;

		case 'K':
			printf("\033[43m ");
			break;

		case 'D':
			printf("\033[48;5;94m ");
			break;

		default:
			printf("%c", c);
			break;
	}
}

static void draw_maze(
	char maze[MAX_H][MAX_W],
	char effect[MAX_H][MAX_W],
	int H,
	int W,
	int playerX,
	int playerY,
	Enemy enemies[],
	int enemyCount,
	int HP,
	int attackCount,
	int score,
	int keyCount,
	int totalKeys,
	int freeAttackCooldown
) {
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			if (i == playerX && j == playerY) {
				printf("\033[41m ");
			}
			else if (is_enemy_position(enemies, enemyCount, i, j)) {
				printf("\033[45m ");
			}
			else if (effect[i][j] == '*') {
				printf("\033[35m*");
			}
			else {
				print_cell_colored(maze[i][j]);
			}
		}

		printf(ANSI_RESET "\n");
	}

	printf(ANSI_RESET ANSI_BOLD "HP: %-5d\n" ANSI_RESET, HP);
	printf(ANSI_RESET ANSI_BOLD "Score: %-6d\n" ANSI_RESET, score);
	printf("Attack Item(*): %-3d\n", attackCount);
	printf("Free Attack CD: %-3d sec\n", freeAttackCooldown);
	printf("Key(K): %-3d / %-3d\n", keyCount, totalKeys);
	printf("Enemy Alive: %-3d / %-3d\n", count_alive_enemies(enemies, enemyCount), g_enemyLimit);
}

static void draw_result_screen(
	int win,
	int score,
	int HP,
	int killedEnemy,
	int aliveEnemy,
	int totalEnemy
) {
	system("cls");

	printf("====================================\n");

	if (win) {
		printf("              YOU WIN!\n");
		printf("            恭喜到達出口！\n");
	}
	else {
		printf("             GAME OVER\n");
		printf("             你失敗了\n");
	}

	printf("====================================\n");
	printf("Final Score : %d\n", score);
	printf("Remain HP   : %d\n", HP);
	printf("Killed Enemy: %d\n", killedEnemy);
	printf("Alive Enemy : %d / %d\n", aliveEnemy, MAX_ENEMY);
	printf("====================================\n");
}

static void handle_item(
	char maze[MAX_H][MAX_W],
	int H,
	int W,
	Enemy enemies[],
	int enemyCount,
	int playerX,
	int playerY,
	int *HP,
	int *safeTurns,
	time_t *safeEndTime,
	int *attackCount,
	int *keyCount,
	int totalKeys
) {
	if (maze[playerX][playerY] == '$') {
		*HP += 100;
		maze[playerX][playerY] = '0';
	}
	else if (maze[playerX][playerY] == '*') {
		(*attackCount)++;
		maze[playerX][playerY] = '0';
	}
	else if (maze[playerX][playerY] == 'Z') {
		*safeTurns = g_safeTurns;
		*safeEndTime = time(NULL) + g_safeTurns;
		maze[playerX][playerY] = '0';
		generate_new_safe_zone(maze, H, W, enemies, enemyCount, playerX, playerY);
	}
	else if (maze[playerX][playerY] == 'K') {
		(*keyCount)++;
		maze[playerX][playerY] = '0';

		if (*keyCount >= totalKeys) {
			open_all_doors(maze, H, W);
		}
	}
}

static int play(const char *filename) {
	char maze[MAX_H][MAX_W];
	char effect[MAX_H][MAX_W];

	int H = 0;
	int W = 0;

	int playerX = -1;
	int playerY = -1;

	Enemy enemies[MAX_ENEMY];
	int enemyCount = 0;
	Spawner spawners[MAX_SPAWNER];
	int spawnerCount = 0;

	int HP = g_playerHP;
	int safeTurns = 0;
	int lastSafeTurns = 0;
	time_t safeEndTime = 0;
	time_t freeAttackEndTime = 0;
	int freeAttackCooldown = 0;
	int attackCount = 0;
	int enemyMoveCounter = 0;
	int spawnCounter = 0;
	int score = 0;
	int keyCount = 0;
	int totalKeys = 0;

	if (!read_maze(filename, maze, &H, &W)) {
		printf("找不到檔案或迷宮格式錯誤: %s\n", filename);
		return 0;
	}

	clear_effect(effect, H, W);

	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			if (maze[i][j] == 'S') {
				playerX = i;
				playerY = j;
				maze[i][j] = '0';
			}
			else if (maze[i][j] == 'E') {
				if (enemyCount < MAX_ENEMY) {
					enemies[enemyCount].x = i;
					enemies[enemyCount].y = j;
					enemies[enemyCount].homeX = i;
					enemies[enemyCount].homeY = j;
					enemies[enemyCount].alive = 1;
					enemies[enemyCount].type = ENEMY_NORMAL;
					enemies[enemyCount].dir = 0;
					enemies[enemyCount].axis = AXIS_HORIZONTAL;
					enemies[enemyCount].wasChasing = 0;
					enemyCount++;
				}

				maze[i][j] = '0';
			}
			else if (maze[i][j] == '@') {
				if (spawnerCount < MAX_SPAWNER) {
					spawners[spawnerCount].x = i;
					spawners[spawnerCount].y = j;

					setup_spawner_patrol(maze, H, W, &spawners[spawnerCount]);

					spawnerCount++;
				}
			}
			else if (maze[i][j] == 'K') {
				totalKeys++;
			}
		}
	}

	if (playerX == -1 || playerY == -1) {
		printf("地圖沒有設定玩家起點 S\n");
		return 0;
	}

	if (enemyCount == 0) {
		printf("地圖沒有設定敵人 E\n");
		return 0;
	}

	while (HP > 0 && maze[playerX][playerY] != '#') {
		safeTurns = get_safe_remaining(safeEndTime);
		freeAttackCooldown = get_free_attack_cooldown_remaining(freeAttackEndTime);

		if (lastSafeTurns > 0 && safeTurns == 0) {
			set_all_enemy_home_to_current(enemies, enemyCount);
		}

		lastSafeTurns = safeTurns;

		clear_screen();

		draw_maze(maze, effect, H, W, playerX, playerY, enemies, enemyCount, HP, attackCount, score, keyCount, totalKeys, freeAttackCooldown);

		printf("Time Stop: %-3d sec\n", safeTurns);

		clear_effect(effect, H, W);

		if (kbhit()) {
			char input = 0;

			while (kbhit()) {
				input = getch();
			}

			if (input == 'f' || input == 'F') {
				int canAttack = 0;
				int useItemAttack = 0;

				if (attackCount > 0) {
					canAttack = 1;
					useItemAttack = 1;
				}
				else if (freeAttackCooldown == 0) {
					canAttack = 1;
					useItemAttack = 0;
				}

				if (canAttack == 1) {
					int killCount = cross_attack(maze, effect, H, W, enemies, enemyCount, playerX, playerY);
					score += killCount * 100;

					if (useItemAttack == 1) {
						attackCount--;
					}
					else {
						freeAttackEndTime = time(NULL) + g_freeAttackCooldown;
					}

					freeAttackCooldown = get_free_attack_cooldown_remaining(freeAttackEndTime);

					clear_screen();
					draw_maze(maze, effect, H, W, playerX, playerY, enemies, enemyCount, HP, attackCount, score, keyCount, totalKeys, freeAttackCooldown);
					printf("Time Stop: %-3d sec\n", safeTurns);

					printf("\n本次十字攻擊擊倒 %d 隻敵人！\n", killCount);

					sleep_ms(300);

					clear_effect(effect, H, W);
				}
			}
			else {
				int nextX = playerX;
				int nextY = playerY;

				if (input == 'w' || input == 'W') {
					nextX--;
				}
				else if (input == 's' || input == 'S') {
					nextX++;
				}
				else if (input == 'a' || input == 'A') {
					nextY--;
				}
				else if (input == 'd' || input == 'D') {
					nextY++;
				}

				if (can_go(maze, H, W, nextX, nextY)) {
					playerX = nextX;
					playerY = nextY;
				}

				handle_item(maze, H, W, enemies, enemyCount, playerX, playerY, &HP, &safeTurns, &safeEndTime, &attackCount, &keyCount, totalKeys);
			}
		}

		safeTurns = get_safe_remaining(safeEndTime);

		hit_enemy_and_damage_player(enemies, enemyCount, playerX, playerY, &HP);

		if (maze[playerX][playerY] == '#') {
			break;
		}

		enemyMoveCounter++;

		if (enemyMoveCounter >= g_enemyMoveInterval) {
			if (safeTurns == 0) {
				move_all_enemies_ai(maze, H, W, enemies, enemyCount, playerX, playerY);
			}

			enemyMoveCounter = 0;
		}

		spawnCounter++;

		if (spawnCounter >= g_spawnInterval) {
			if (safeTurns == 0) {
				spawn_enemy(enemies, &enemyCount, spawners, spawnerCount, playerX, playerY);
			}

			spawnCounter = 0;
		}

		hit_enemy_and_damage_player(enemies, enemyCount, playerX, playerY, &HP);

		sleep_ms(g_gameTickMs);
	}

	{
		int aliveEnemy = count_alive_enemies(enemies, enemyCount);
		int killedEnemy = score / 100;

		if (maze[playerX][playerY] == '#') {
			draw_result_screen(1, score, HP, killedEnemy, aliveEnemy, enemyCount);
			return 1;
		}
		else {
			draw_result_screen(0, score, HP, killedEnemy, aliveEnemy, enemyCount);
			return 0;
		}
	}
}

static void print_editor_cell(char c) {
	switch (c) {
		case '1':
			printf("\033[100m ");
			break;

		case '0':
			printf("\033[40m ");
			break;

		case 'S':
			printf("\033[41m ");
			break;

		case '#':
			printf("\033[42m ");
			break;

		case 'E':
			printf("\033[45m ");
			break;

		case '@':
			printf("\033[48;5;208m ");
			break;

		case 'K':
			printf("\033[43m ");
			break;

		case 'D':
			printf("\033[48;5;94m ");
			break;

		case 'Z':
			printf("\033[46m ");
			break;

		case '*':
			printf("\033[40;35m*");
			break;

		case '$':
			printf("\033[40;33m$");
			break;

		default:
			printf("%c", c);
			break;
	}
}

static void draw_editor(
	char editorMap[MAX_H][MAX_W],
	int H,
	int W,
	int cursorX,
	int cursorY,
	char selected
) {
	clear_screen();

	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			if (i == cursorX && j == cursorY) {
				printf("\033[47m ");
			}
			else {
				print_editor_cell(editorMap[i][j]);
			}
		}

		printf(ANSI_RESET "\n");
	}

	printf(ANSI_RESET "\n");
	printf("地圖編輯器\n");
	printf("目前游標位置: (%d, %d)\n", cursorX, cursorY);
	printf("目前選擇物件: %c\n", selected);
	printf("\n");
	printf("W / A / S / D：移動游標\n");
	printf("0 / 1 / E / K / Z / @ / * / $ / #：選擇要放置的物件\n");
	printf("Shift + S：選擇玩家起點 S\n");
	printf("Shift + D：選擇門 D\n");
	printf("空白鍵：在目前游標位置放置物件\n");
	printf("B：油漆桶填滿，目前只支援 0 或 1\n");
	printf("L：讀取地圖檔\n");
	printf("P：儲存地圖\n");
	printf("Q：離開地圖編輯器\n");
}

static void create_empty_map(char editorMap[MAX_H][MAX_W], int H, int W) {
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			if (i == 0 || i == H - 1 || j == 0 || j == W - 1) {
				editorMap[i][j] = '1';
			}
			else {
				editorMap[i][j] = '0';
			}
		}

		editorMap[i][W] = '\0';
	}
}

static void flood_fill_01(
	char editorMap[MAX_H][MAX_W],
	int H,
	int W,
	int startX,
	int startY,
	char fillChar
) {
	static int queueX[MAX_H * MAX_W];
	static int queueY[MAX_H * MAX_W];

	char target = editorMap[startX][startY];

	if (fillChar != '0' && fillChar != '1') {
		return;
	}

	if (target == fillChar) {
		return;
	}

	int front = 0;
	int rear = 0;

	queueX[rear] = startX;
	queueY[rear] = startY;
	rear++;

	while (front < rear) {
		int x = queueX[front];
		int y = queueY[front];
		front++;

		if (x < 0 || x >= H || y < 0 || y >= W) {
			continue;
		}

		if (editorMap[x][y] != target) {
			continue;
		}

		editorMap[x][y] = fillChar;

		if (rear + 4 < MAX_H * MAX_W) {
			queueX[rear] = x + 1;
			queueY[rear] = y;
			rear++;

			queueX[rear] = x - 1;
			queueY[rear] = y;
			rear++;

			queueX[rear] = x;
			queueY[rear] = y + 1;
			rear++;

			queueX[rear] = x;
			queueY[rear] = y - 1;
			rear++;
		}
	}
}

static int save_editor_map(char editorMap[MAX_H][MAX_W], int H, int W) {
	char filename[256];
	char choice;

	system("cls");

	printf("請輸入要儲存的檔名: ");
	fgets(filename, sizeof(filename), stdin);
	normalize_filename(filename, "maze.txt");

	if (file_exists(filename)) {
		printf("檔案已存在: %s\n", filename);
		printf("是否覆蓋? Y/N: ");
		choice = getch();
		printf("%c\n", choice);

		if (choice != 'y' && choice != 'Y') {
			printf("已取消儲存。\n");
			system("pause");
			return 0;
		}
	}

	FILE *fp = fopen(filename, "w");

	if (!fp) {
		printf("儲存失敗。\n");
		system("pause");
		return 0;
	}

	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			fputc(editorMap[i][j], fp);
		}

		fputc('\n', fp);
	}

	fclose(fp);

	printf("已儲存成: %s\n", filename);
	system("pause");

	return 1;
}

static void map_editor(void) {
	char editorMap[MAX_H][MAX_W];
	char filename[256];

	int H = 0;
	int W = 0;
	int cursorX = 1;
	int cursorY = 1;
	char selected = '1';
	int running = 1;
	int mode = 0;
	int loaded = 0;

	system("cls");
	printf("地圖編輯器設定\n");
	printf("1. 新建地圖\n");
	printf("2. 讀取現有地圖\n");
	printf("請輸入選項: ");
	scanf("%d", &mode);
	getchar();

	if (mode == 2) {
		printf("請輸入要讀取的地圖檔名，直接按 Enter 使用 maze.txt: ");
		fgets(filename, sizeof(filename), stdin);
		normalize_filename(filename, "maze.txt");

		if (read_maze(filename, editorMap, &H, &W)) {
			printf("讀取完成: %s\n", filename);
			loaded = 1;
			system("pause");
		}
		else {
			printf("讀取失敗，改為新建地圖。\n");
			system("pause");
		}
	}

	if (loaded == 0) {
		system("cls");
		printf("地圖編輯器設定\n");
		printf("建議大小: 高度 10~25、寬度 35~120\n");
		printf("請輸入地圖高度: ");
		scanf("%d", &H);
		printf("請輸入地圖寬度: ");
		scanf("%d", &W);
		getchar();

		if (H < 5) {
			H = 5;
		}
		if (W < 5) {
			W = 5;
		}
		if (H > MAX_H) {
			H = MAX_H;
		}
		if (W > MAX_W - 1) {
			W = MAX_W - 1;
		}

		create_empty_map(editorMap, H, W);
	}

	if (cursorX >= H) {
		cursorX = H - 1;
	}
	if (cursorY >= W) {
		cursorY = W - 1;
	}

	while (running) {
		draw_editor(editorMap, H, W, cursorX, cursorY, selected);

		char input = getch();

		if (input == 'w' || input == 'W') {
			if (cursorX > 0) {
				cursorX--;
			}
		}
		else if (input == 'a' || input == 'A') {
			if (cursorY > 0) {
				cursorY--;
			}
		}
		else if (input == 's') {
			if (cursorX < H - 1) {
				cursorX++;
			}
		}
		else if (input == 'd') {
			if (cursorY < W - 1) {
				cursorY++;
			}
		}
		else if (input == '0') {
			selected = '0';
		}
		else if (input == '1') {
			selected = '1';
		}
		else if (input == 'S') {
			selected = 'S';
		}
		else if (input == 'E' || input == 'e') {
			selected = 'E';
		}
		else if (input == 'K' || input == 'k') {
			selected = 'K';
		}
		else if (input == 'D') {
			selected = 'D';
		}
		else if (input == 'Z' || input == 'z') {
			selected = 'Z';
		}
		else if (input == '@') {
			selected = '@';
		}
		else if (input == '*') {
			selected = '*';
		}
		else if (input == '$') {
			selected = '$';
		}
		else if (input == '#') {
			selected = '#';
		}
		else if (input == ' ') {
			editorMap[cursorX][cursorY] = selected;
		}
		else if (input == 'b' || input == 'B') {
			if (selected == '0' || selected == '1') {
				flood_fill_01(editorMap, H, W, cursorX, cursorY, selected);
			}
			else {
				printf("\n油漆桶目前只支援 0 或 1。\n");
				sleep_ms(700);
			}
		}
		else if (input == 'l' || input == 'L') {
			system("cls");
			printf("請輸入要讀取的地圖檔名，直接按 Enter 使用 maze.txt: ");
			fgets(filename, sizeof(filename), stdin);
			normalize_filename(filename, "maze.txt");

			if (read_maze(filename, editorMap, &H, &W)) {
				cursorX = 1;
				cursorY = 1;

				if (cursorX >= H) {
					cursorX = 0;
				}
				if (cursorY >= W) {
					cursorY = 0;
				}

				printf("讀取完成: %s\n", filename);
			}
			else {
				printf("讀取失敗，保留目前正在編輯的地圖。\n");
			}

			system("pause");
		}
		else if (input == 'p' || input == 'P') {
			save_editor_map(editorMap, H, W);
		}
		else if (input == 'q' || input == 'Q') {
			running = 0;
		}
	}
}

static void show_instruction(void) {
	system("cls");

	printf("====================================\n");
	printf("              遊戲說明\n");
	printf("====================================\n");
	printf("備註1.開始遊戲前，記得先把畫面切成全螢幕\n");
	printf("備註2.開始遊戲後，記得切換成英文輸入法\n\n");

	printf("操作方式：\n");
	printf("W / A / S / D：移動玩家\n");
	printf("F：使用十字攻擊\n");
	printf("\n");

	printf("地圖顏色說明：\n");
	printf("紅色方塊：玩家\n");
	printf("灰色方塊：牆壁\n");
	printf("黑色方塊：地板\n");
	printf("綠色方塊：終點\n");
	printf("黃色方塊：鑰匙\n");
	printf("褐色方塊：門\n");
	printf("紫色方塊：敵人，玩家碰到敵人HP-%d\n", g_enemyDamage);
	printf("橘色方塊：敵人重生點\n");
	printf("藍色方塊：時間暫停\n");
	printf("\n");

	printf("遊戲目標：\n");
	printf("收集所有鑰匙打開終點前的門，最後抵達終點\n");
	printf("\n");

	printf("特殊道具：\n");
	printf("藍色方塊：時間暫停，使敵人暫時停止行動\n");
	printf("*：取得一次道具十字攻擊，按 F 使用，不受冷卻限制\n");
	printf("沒有 * 時，按 F 可使用免費十字攻擊，冷卻時間 %d 秒\n", g_freeAttackCooldown);
	printf("$：HP+100\n");
	printf("\n");

	printf("地圖編輯器操作：\n");
	printf("W / A / S / D：移動游標\n");
	printf("空白鍵：放置目前選擇的物件\n");
	printf("B：油漆桶填滿，目前只支援 0 或 1\n");
	printf("L：讀取地圖檔\n");
	printf("P：儲存地圖\n");
	printf("Q：離開地圖編輯器\n");
	printf("\n");

	printf("====================================\n");
	printf("按任意鍵返回主選單...");
	getch();
}

static int input_range_int(const char *name, int oldValue, int minValue, int maxValue) {
	int value;

	printf("%s 目前數值: %d\n", name, oldValue);
	printf("請輸入新數值 (%d ~ %d)，輸入 0 表示不修改: ", minValue, maxValue);
	scanf("%d", &value);
	getchar();

	if (value == 0) {
		return oldValue;
	}

	if (value < minValue) {
		printf("數值太小，已自動改成最小值 %d。\n", minValue);
		return minValue;
	}

	if (value > maxValue) {
		printf("數值太大，已自動改成最大值 %d。\n", maxValue);
		return maxValue;
	}

	return value;
}

static void reset_game_settings(void) {
	g_spawnInterval = DEFAULT_SPAWN_INTERVAL;
	g_chaseRange = DEFAULT_CHASE_RANGE;
	g_patrolRange = DEFAULT_PATROL_RANGE;
	g_safeTurns = DEFAULT_SAFE_TURNS;
	g_enemyDamage = DEFAULT_ENEMY_DAMAGE;
	g_gameTickMs = DEFAULT_GAME_TICK_MS;
	g_enemyMoveInterval = DEFAULT_ENEMY_MOVE_INTERVAL;
	g_freeAttackCooldown = DEFAULT_FREE_ATTACK_COOLDOWN;
	g_playerHP = DEFAULT_PLAYER_HP;
	g_enemyLimit = DEFAULT_ENEMY_LIMIT;
}

static void show_setting_menu(void) {
	int choice;
	int running = 1;

	while (running) {
		system("cls");

		printf("====================================\n");
		printf("              參數設定\n");
		printf("====================================\n");
		printf("1. 敵人生成間隔 Spawn Interval       : %d\n", g_spawnInterval);
		printf("2. 敵人偵測玩家範圍 Chase Range      : %d\n", g_chaseRange);
		printf("3. 敵人巡邏範圍 Patrol Range         : %d\n", g_patrolRange);
		printf("4. 敵人移動間隔 Enemy Move Interval  : %d\n", g_enemyMoveInterval);
		printf("5. 敵人碰撞傷害 Enemy Damage         : %d\n", g_enemyDamage);
		printf("6. 遊戲刷新速度 Game Tick(ms)        : %d\n", g_gameTickMs);
		printf("7. 時間暫停道具秒數 Time Stop(sec)       : %d\n", g_safeTurns);
		printf("8. 免費十字攻擊冷卻 Free Attack CD(sec)  : %d\n", g_freeAttackCooldown);
		printf("9. 玩家初始血量 Player HP            : %d\n", g_playerHP);
		printf("10. 敵人數量上限 Enemy Limit         : %d\n", g_enemyLimit);
		printf("11. 恢復預設值\n");
		printf("12. 返回主選單\n");
		printf("====================================\n");
		printf("說明：\n");
		printf("Spawn Interval 越小，敵人生成越快。\n");
		printf("Enemy Move Interval 越小，敵人移動越快。\n");
		printf("Game Tick 越小，遊戲刷新越快，但太小可能變卡。\n");
		printf("====================================\n");
		printf("請輸入選項: ");

		scanf("%d", &choice);
		getchar();

		system("cls");

		if (choice == 1) {
			g_spawnInterval = input_range_int("敵人生成間隔 Spawn Interval", g_spawnInterval, 5, 200);
			system("pause");
		}
		else if (choice == 2) {
			g_chaseRange = input_range_int("敵人偵測玩家範圍 Chase Range", g_chaseRange, 1, 50);
			system("pause");
		}
		else if (choice == 3) {
			g_patrolRange = input_range_int("敵人巡邏範圍 Patrol Range", g_patrolRange, 1, 50);
			system("pause");
		}
		else if (choice == 4) {
			g_enemyMoveInterval = input_range_int("敵人移動間隔 Enemy Move Interval", g_enemyMoveInterval, 1, 20);
			system("pause");
		}
		else if (choice == 5) {
			g_enemyDamage = input_range_int("敵人碰撞傷害 Enemy Damage", g_enemyDamage, 1, 1000);
			system("pause");
		}
		else if (choice == 6) {
			g_gameTickMs = input_range_int("遊戲刷新速度 Game Tick", g_gameTickMs, 10, 300);
			system("pause");
		}
		else if (choice == 7) {
			g_safeTurns = input_range_int("時間暫停秒數 Time Stop", g_safeTurns, 1, 30);
			system("pause");
		}
		else if (choice == 8) {
			g_freeAttackCooldown = input_range_int("免費攻擊冷卻 Free Attack CD", g_freeAttackCooldown, 1, 60);
			system("pause");
		}
		else if (choice == 9) {
			g_playerHP = input_range_int("玩家初始血量 Player HP", g_playerHP, 100, 9999);
			system("pause");
		}
		else if (choice == 10) {
			g_enemyLimit = input_range_int("敵人數量上限 Enemy Limit", g_enemyLimit, 1, MAX_ENEMY);
			system("pause");
		}
		else if (choice == 11) {
			reset_game_settings();
			printf("已恢復預設值。\n");
			system("pause");
		}
		else if (choice == 12) {
			running = 0;
		}
		else {
			printf("請輸入正確選項。\n");
			system("pause");
		}
	}
}

static int show_main_menu(void) {
	int choice;

	while (1) {
		system("cls");

		printf("====================================\n");
		printf("        Dungeon Maze Adventure\n");
		printf("====================================\n");
		printf("1. 開始遊戲\n");
		printf("2. 遊戲說明\n");
		printf("3. 地圖編輯器\n");
		printf("4. 參數設定\n");
		printf("5. 離開遊戲\n");
		printf("====================================\n");
		printf("請輸入選項：");

		scanf("%d", &choice);
		getchar();

		if (choice == 1 || choice == 2 || choice == 3 || choice == 4 || choice == 5) {
			return choice;
		}

		printf("請輸入正確選項 1、2、3、4 或 5。\n");
		system("pause");
	}
}

int main(void) {
	srand((unsigned int)time(NULL));

#ifdef _WIN32
	enable_virtual_terminal();
#endif

	while (1) {
		int menuChoice = show_main_menu();

		if (menuChoice == 1) {
			char filename[256];

			system("cls");
			printf("請輸入地圖檔名，直接按 Enter 使用 maze.txt: ");
			fgets(filename, sizeof(filename), stdin);
			normalize_filename(filename, "maze.txt");

			system("cls");
			play(filename);

			printf("\n按任意鍵返回主選單...");
			getch();
		}
		else if (menuChoice == 2) {
			show_instruction();
		}
		else if (menuChoice == 3) {
			map_editor();
		}
		else if (menuChoice == 4) {
			show_setting_menu();
		}
		else if (menuChoice == 5) {
			printf("離開遊戲。\n");
			pause_exit();
			return 0;
		}
	}

	return 0;
}
