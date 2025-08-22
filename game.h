#ifndef GAME_H
#define GAME_H

#define TILE_SIZE 24
#define MAP_WIDTH 160
#define MAP_HEIGHT 50
#define MAX_MONSTERS 20
#define MAX_ROOMS 20
#define MONSTER_DETECTION_RANGE 8

#define HUNGER_STARVING 200
#define PASSIVE_REGEN_INTERVAL 5
#define HUNGER_TURN_THRESHOLD 20
#define REST_TURNS_REQUIRED 5

// Player attributes
typedef struct {
    int x, y;
    int hp;
    int maxHp;
    int mana;
    int maxMana;
    int intelligence;
    int score; // Player's score
    int healthPotions; // Number of health potions
    int foodInInventory; // Food in inventory
    int level; // Player's level
    int xp;    // Player's experience points
    int xpToNextLevel; // XP required for the next level
    int hunger; // Hunger level
    int visibilityRadius; // Radius of visibility
    char causeOfDeath[30]; // What killed the player
    int isStarving; // Flag to prevent repeated starving messages/sounds
    int turnsToHunger; // How many turns before hunger increases
} Player;

// Monster attributes
typedef struct {
    int x, y;
    int hp;
    char symbol;
    char name[20];
    int active; // 1 if active, 0 if defeated
    int speed;  // How many tiles it moves per turn
    int points; // Points awarded for defeating this monster
    int rangedAttack; // 1 if monster has ranged attack, 0 otherwise
} Monster;

// Room attributes
typedef struct {
    int x, y;
    int width, height;
} Room;

// Game states
typedef enum {
    STATE_PLAYING,
    STATE_HELP,
    STATE_GAMEOVER,
    STATE_WIN,
    STATE_LEVELUP
} GameState;

#endif // GAME_H
