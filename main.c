#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "font.h"
#include "game.h"

// Screen dimensions (will be set at runtime)
int SCREEN_WIDTH;
int SCREEN_HEIGHT;

// Monster templates with scoring
Monster monsterTemplates[] = {
    {0, 0, 5, 'g', "Goblin", 1, 2, 10, 0},
    {0, 0, 15, 'O', "Ogre", 1, 1, 50, 0},
    {0, 0, 10, 'o', "Orc", 1, 1, 20, 0},
    {0, 0, 8, 's', "Snake", 1, 3, 15, 0},
    {0, 0, 25, 'D', "Dragon", 1, 1, 100, 0},
    {0, 0, 12, 'E', "Poisonous Eye", 1, 2, 40, 1}
};

Monster finalBossTemplate = {0, 0, 100, 'L', "Lich Lord", 1, 1, 500, 1};

#define NUM_MONSTER_TYPES (sizeof(monsterTemplates) / sizeof(Monster))


// Game state variables
Player player;
Monster monsters[MAX_MONSTERS];
Room rooms[MAX_ROOMS];
int numRooms = 0;
char map[MAP_HEIGHT][MAP_WIDTH];
char messageBuffer[256];
int messageTimer = 0; // Timer to clear the message log
int turnCounter = 0; // New turn counter for passive regeneration
int restCounter = 0; // Counter for resting
GameState gameState = STATE_PLAYING;
int isAwaitingSpellDirection = 0; // New flag for magic missile
int dungeonLevel = 1;

// Explored tiles: 1 if the player has seen the tile
int visibility[MAP_HEIGHT][MAP_WIDTH];

// Camera/Viewport position
int cameraX = 0;
int cameraY = 0;

// SDL2 variables
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
SDL_Color textColor = {255, 255, 255, 255}; // White color

// Sound variables
Mix_Chunk* beepSound = NULL;
unsigned char beep_raw_data[] = {
    // A simple 440 Hz square wave for 0.5 seconds
    // This is just a placeholder, you can replace it with any raw audio data
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
unsigned int beep_raw_data_len = 128;


// Function prototypes
void initSDL();
void closeSDL();
void generateDungeon();
void createRoom(int x, int y, int width, int height);
void connectRooms();
void placeMonsters();
int handlePlayingInput(SDL_Event* e); // Returns 1 if a turn passed, 0 otherwise
int handleHelpInput(SDL_Event* e);
void moveMonsters();
void fightMonster(int monsterIndex);
void rest();
void castHealSpell();
void castMagicMissile(int dx, int dy);
void castPhaseDoorSpell();
void useHealthPotion();
void renderGame();
void renderGameOverScreen();
void renderHelpScreen();
void renderWinScreen();
void renderLevelUpScreen();
void drawText(const char* text, int x, int y, SDL_Color color);
void showMessage(const char* message);
int getDistance(int x1, int y1, int x2, int y2);
int isOccupiedByMonster(int x, int y);
int isTileWalkable(int x, int y);
void checkLevelUp();
void updateVisibility();
void placePotions();
void placeFood();
void eatFood();

int main(int argc, char* args[]) {
    initSDL();
    srand(time(NULL));

    // Initialize player
    player.hp = 20;
    player.maxHp = 20;
    player.mana = 10;
    player.maxMana = 10;
    player.intelligence = 5;
    player.score = 0;
    player.healthPotions = 0;
    player.foodInInventory = 10; // Start with 10 food items
    player.level = 1;
    player.xp = 0;
    player.xpToNextLevel = 150; // Increased XP threshold
    player.hunger = 0;
    player.visibilityRadius = 8; // Default visibility radius
    player.causeOfDeath[0] = '\0';
    player.isStarving = 0;
    player.turnsToHunger = HUNGER_TURN_THRESHOLD;

    // Generate the initial dungeon and place monsters
    generateDungeon();
    placeMonsters();
    updateVisibility();

    int running = 1;
    SDL_Event e;
    int playerTurnPassed = 0;

    while (running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                if (gameState == STATE_HELP) {
                    gameState = STATE_PLAYING;
                } else if (gameState == STATE_PLAYING) {
                    running = 0;
                }
            } else {
                if (gameState == STATE_PLAYING) {
                    playerTurnPassed = handlePlayingInput(&e);
                } else if (gameState == STATE_HELP) {
                    // Handled in the main loop for now, but good to have a dedicated function
                } else if (gameState == STATE_LEVELUP) {
                    // The level up screen is a brief pause, so we can handle input
                    // to dismiss it here, or just let the delay run out.
                    // For now, let the delay handle it.
                }
            }
        }
        
        // Only update game state once per player turn
        if (playerTurnPassed) {
            if (gameState == STATE_PLAYING) {
                moveMonsters();
                // Decrement the message timer
                if (messageTimer > 0) {
                    messageTimer--;
                    if (messageTimer == 0) {
                        memset(messageBuffer, 0, sizeof(messageBuffer));
                    }
                }
                
                // Hunger mechanic
                player.hunger++;
                if (player.hunger >= HUNGER_STARVING) {
                    player.hp--;
                    if (player.isStarving == 0) {
                        Mix_PlayChannel(-1, beepSound, 0); // Play beep once
                        showMessage("You are starving!");
                        player.isStarving = 1;
                    }
                } else {
                    player.isStarving = 0; // Reset starving flag
                }

                // Passive regeneration
                turnCounter++;
                if (turnCounter >= PASSIVE_REGEN_INTERVAL) {
                    if (player.hp < player.maxHp) {
                        player.hp++;
                    }
                    if (player.mana < player.maxMana) {
                        player.mana++;
                    }
                    turnCounter = 0;
                }
                
                checkLevelUp(); // Check for level up after every turn
                updateVisibility(); // Update visibility after every turn
            }
            playerTurnPassed = 0; // Reset flag
        }
        
        // Check for game over or win condition
        if (player.hp <= 0 && gameState != STATE_GAMEOVER) {
            gameState = STATE_GAMEOVER;
            strncpy(player.causeOfDeath, "starvation", sizeof(player.causeOfDeath) - 1);
            player.causeOfDeath[sizeof(player.causeOfDeath) - 1] = '\0';
        }
        
        // Win condition: dungeon level 5 and the boss is defeated
        if (dungeonLevel >= 5) {
            int bossIsAlive = 0;
            for (int i = 0; i < MAX_MONSTERS; i++) {
                if (monsters[i].active && strcmp(monsters[i].name, "Lich Lord") == 0) {
                    bossIsAlive = 1;
                    break;
                }
            }
            if (!bossIsAlive && gameState != STATE_WIN) {
                gameState = STATE_WIN;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        switch (gameState) {
            case STATE_PLAYING:
                renderGame();
                break;
            case STATE_HELP:
                renderHelpScreen();
                break;
            case STATE_LEVELUP:
                renderLevelUpScreen();
                SDL_Delay(2000);
                gameState = STATE_PLAYING;
                break;
            case STATE_GAMEOVER:
                renderGameOverScreen();
                SDL_Delay(5000);
                running = 0;
                break;
            case STATE_WIN:
                renderWinScreen();
                SDL_Delay(5000);
                running = 0;
                break;
        }
        
        SDL_RenderPresent(renderer);
    }

    closeSDL();
    return 0;
}

// Initialize SDL2, Window, Renderer, and Font
void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    
    // Get display mode to determine screen size
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
        printf("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        exit(1);
    }
    SCREEN_WIDTH = dm.w;
    SCREEN_HEIGHT = dm.h;
    
    // Create a full-screen window
    window = SDL_CreateWindow("Moria-like Dungeon Crawler", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        exit(1);
    }

    // Load font from embedded data
    SDL_RWops* rw = SDL_RWFromConstMem(DejaVuSansMono_ttf, sizeof(DejaVuSansMono_ttf));
    font = TTF_OpenFontRW(rw, 1, TILE_SIZE);
    
    if (font == NULL) {
        printf("Failed to load font from memory! TTF_Error: %s\n", TTF_GetError());
        exit(1);
    }

    // Initialize SDL_mixer for sound
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! Mix_Error: %s\n", Mix_GetError());
    }

    // Load sound from embedded data
    SDL_RWops* rwBeep = SDL_RWFromConstMem(beep_raw_data, beep_raw_data_len);
    beepSound = Mix_LoadWAV_RW(rwBeep, 1);
    if (beepSound == NULL) {
        printf("Failed to load beep sound! Mix_Error: %s\n", Mix_GetError());
    }

    // Initialize message buffer
    memset(messageBuffer, 0, sizeof(messageBuffer));
}

// Clean up SDL resources
void closeSDL() {
    Mix_FreeChunk(beepSound);
    beepSound = NULL;
    Mix_Quit();
    TTF_CloseFont(font);
    font = NULL;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    window = NULL;
    renderer = NULL;
    TTF_Quit();
    SDL_Quit();
}

// Procedurally generate a dungeon with rooms and corridors
void generateDungeon() {
    // Fill map with walls
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map[y][x] = '#';
        }
    }
    
    // Create random rooms
    numRooms = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        int roomWidth = rand() % 10 + 5; // Room width 5-14
        int roomHeight = rand() % 8 + 4; // Room height 4-11
        
        // Ensure rooms are within map boundaries
        int roomX = rand() % (MAP_WIDTH - roomWidth - 2) + 1;
        int roomY = rand() % (MAP_HEIGHT - roomHeight - 2) + 1;
        
        // Check for overlap with existing rooms
        int overlaps = 0;
        for (int j = 0; j < numRooms; j++) {
            if (roomX < rooms[j].x + rooms[j].width && roomX + roomWidth > rooms[j].x &&
                roomY < rooms[j].y + rooms[j].height && roomY + roomHeight > rooms[j].y) {
                overlaps = 1;
                break;
            }
        }

        if (!overlaps) {
            createRoom(roomX, roomY, roomWidth, roomHeight);
            rooms[numRooms].x = roomX;
            rooms[numRooms].y = roomY;
            rooms[numRooms].width = roomWidth;
            rooms[numRooms].height = roomHeight;
            numRooms++;
        }
    }

    // Connect the rooms
    connectRooms();

    // Place the player in the center of the first room
    if (numRooms > 0) {
        player.x = rooms[0].x + rooms[0].width / 2;
        player.y = rooms[0].y + rooms[0].height / 2;
    } else {
        // If no rooms were created, place the player in a safe default location
        player.x = MAP_WIDTH / 2;
        player.y = MAP_HEIGHT / 2;
        map[player.y][player.x] = '.';
    }
    
    // Place potions and food
    placePotions();
    placeFood();
    
    // Place stairs down if not the final level
    if (numRooms > 1 && dungeonLevel < 5) {
        int lastRoomIndex = numRooms - 1;
        int stairsX = rooms[lastRoomIndex].x + rooms[lastRoomIndex].width / 2;
        int stairsY = rooms[lastRoomIndex].y + rooms[lastRoomIndex].height / 2;
        map[stairsY][stairsX] = '>';
    }
    
    // Initialize visibility map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visibility[y][x] = 0;
        }
    }
}

// Helper function to carve out a room
void createRoom(int x, int y, int width, int height) {
    for (int i = y; i < y + height; i++) {
        for (int j = x; j < x + width; j++) {
            map[i][j] = '.';
        }
    }
}

// Connect the rooms with corridors
void connectRooms() {
    for (int i = 0; i < numRooms - 1; i++) {
        int x1 = rooms[i].x + rooms[i].width / 2;
        int y1 = rooms[i].y + rooms[i].height / 2;
        int x2 = rooms[i+1].x + rooms[i+1].width / 2;
        int y2 = rooms[i+1].y + rooms[i+1].height / 2;

        // Carve horizontal corridor
        if (x1 < x2) {
            for (int x = x1; x <= x2; x++) {
                map[y1][x] = '.';
            }
        } else {
            for (int x = x2; x <= x1; x++) {
                map[y1][x] = '.';
            }
        }

        // Carve vertical corridor
        if (y1 < y2) {
            for (int y = y1; y <= y2; y++) {
                map[y][x2] = '.';
            }
        } else {
            for (int y = y2; y <= y1; y++) {
                map[y][x2] = '.';
            }
        }
    }
}

// Place monsters in the dungeon
void placeMonsters() {
    if (dungeonLevel == 5) {
        // Place the final boss on level 5
        monsters[0] = finalBossTemplate;
        monsters[0].hp = finalBossTemplate.hp * 2; // Make boss even stronger
        monsters[0].points = finalBossTemplate.points * 2; // More points for the boss
        monsters[0].active = 1;
        
        int placed = 0;
        int attempt = 0;
        while (!placed && attempt < 100) {
            int x = rooms[numRooms-1].x + rooms[numRooms-1].width / 2;
            int y = rooms[numRooms-1].y + rooms[numRooms-1].height / 2;
            if (map[y][x] == '.' && (x != player.x || y != player.y)) {
                monsters[0].x = x;
                monsters[0].y = y;
                placed = 1;
            }
            attempt++;
        }
        for (int i = 1; i < MAX_MONSTERS; i++) {
            monsters[i].active = 0; // Deactivate other monsters on the final level
        }

    } else {
        for (int i = 0; i < MAX_MONSTERS; i++) {
            // Randomly choose a monster type from the templates
            int type = rand() % NUM_MONSTER_TYPES;
            monsters[i] = monsterTemplates[type];
            monsters[i].active = 1; // All monsters are active
            
            // Scale monster stats with dungeon level
            monsters[i].hp += dungeonLevel * 2;
            monsters[i].points += dungeonLevel * 5;

            // Find a random valid floor tile to place the monster
            int placed = 0;
            int attempt = 0;
            while (!placed && attempt < 100) {
                int x = rand() % MAP_WIDTH;
                int y = rand() % MAP_HEIGHT;
                if (map[y][x] == '.' && (x != player.x || y != player.y)) {
                    monsters[i].x = x;
                    monsters[i].y = y;
                    placed = 1;
                }
                attempt++;
            }
            if (!placed) {
                monsters[i].active = 0; // If no space is found, deactivate the monster
            }
        }
    }
}

// Place potions on the floor
void placePotions() {
    if (rand() % 3 == 0) { // 33% chance to place a potion on a new level
        int placed = 0;
        while(!placed) {
            int x = rand() % MAP_WIDTH;
            int y = rand() % MAP_HEIGHT;
            if (map[y][x] == '.' && (x != player.x || y != player.y)) {
                map[y][x] = '!'; // Potion symbol
                placed = 1;
            }
        }
    }
}

// Place food on the floor
void placeFood() {
    if (rand() % 2 == 0) { // 50% chance to place food on a new level
        int placed = 0;
        while(!placed) {
            int x = rand() % MAP_WIDTH;
            int y = rand() % MAP_HEIGHT;
            if (map[y][x] == '.' && (x != player.x || y != player.y)) {
                map[y][x] = 'F'; // Food symbol
                placed = 1;
            }
        }
    }
}

// Handle player input for playing state
int handlePlayingInput(SDL_Event* e) {
    if (e->type == SDL_KEYDOWN) {
        // If we were waiting for a spell direction, and a directional key is pressed
        if (isAwaitingSpellDirection) {
            int dx = 0, dy = 0;
            switch (e->key.keysym.sym) {
                case SDLK_UP:    dy = -1; break;
                case SDLK_DOWN:  dy = 1; break;
                case SDLK_LEFT:  dx = -1; break;
                case SDLK_RIGHT: dx = 1; break;
                default:
                    // If any other key is pressed, cancel the spell
                    isAwaitingSpellDirection = 0;
                    showMessage("Magic missile cancelled.");
                    return 0; // No turn passed
            }
            isAwaitingSpellDirection = 0;
            castMagicMissile(dx, dy);
            return 1; // A turn has passed
        }
        
        int newX = player.x;
        int newY = player.y;
        int playerMoved = 0;

        switch (e->key.keysym.sym) {
            case SDLK_UP:
                // Only process movement if not starving or if a new key is pressed
                if (player.isStarving == 0 || e->key.repeat == 0) {
                    newY--;
                    playerMoved = 1;
                }
                break;
            case SDLK_DOWN:
                if (player.isStarving == 0 || e->key.repeat == 0) {
                    newY++;
                    playerMoved = 1;
                }
                break;
            case SDLK_LEFT:
                if (player.isStarving == 0 || e->key.repeat == 0) {
                    newX--;
                    playerMoved = 1;
                }
                break;
            case SDLK_RIGHT:
                if (player.isStarving == 0 || e->key.repeat == 0) {
                    newX++;
                    playerMoved = 1;
                }
                break;
            case SDLK_r: // New rest functionality
                if (e->key.repeat == 0) {
                    if (isOccupiedByMonster(player.x-1, player.y) != -1 || isOccupiedByMonster(player.x+1, player.y) != -1 ||
                        isOccupiedByMonster(player.x, player.y-1) != -1 || isOccupiedByMonster(player.x, player.y+1) != -1) {
                            showMessage("You can't rest while adjacent to a monster!");
                            return 0;
                    }
                    restCounter++;
                    if (restCounter >= REST_TURNS_REQUIRED) {
                        player.hp++;
                        if (player.hp > player.maxHp) player.hp = player.maxHp;
                        player.mana++;
                        if (player.mana > player.maxMana) player.mana = player.maxMana;
                        restCounter = 0;
                        showMessage("You have rested and recovered 1 HP and 1 Mana!");
                    } else {
                        char tempBuffer[256];
                        snprintf(tempBuffer, sizeof(tempBuffer), "Resting... (Turn %d/%d)", restCounter, REST_TURNS_REQUIRED);
                        showMessage(tempBuffer);
                    }
                    player.hunger += 5; // Resting makes you hungrier
                    return 1; // A turn has passed
                }
                return 0;
            case SDLK_h: // Heal spell
                castHealSpell();
                return 1;
            case SDLK_f: // Magic Missile spell
                isAwaitingSpellDirection = 1;
                showMessage("Choose a direction for magic missile!");
                return 0; // No turn passed yet
            case SDLK_t: // Teleportation spell
                castPhaseDoorSpell();
                return 1; // A turn has passed
            case SDLK_e: // Eat food
                eatFood();
                return 1;
            case SDLK_p: // Use health potion
                useHealthPotion();
                return 1;
            case SDLK_SLASH:
                gameState = STATE_HELP;
                return 0; // No turn passed
            default:
                return 0; // No action taken
        }

        if (playerMoved) {
            // Check if the new position is a floor tile and not a wall
            if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT && map[newY][newX] != '#') {
                
                // Check for stairs
                if (map[newY][newX] == '>') {
                    dungeonLevel++;
                    generateDungeon();
                    placeMonsters();
                    showMessage("You descend to a new level!");
                    return 1;
                }
                
                // Check for potion
                if (map[newY][newX] == '!') {
                    player.healthPotions++;
                    map[newY][newX] = '.';
                    showMessage("You found a health potion!");
                }
                
                // Check for food
                if (map[newY][newX] == 'F') {
                    player.foodInInventory++;
                    map[newY][newX] = '.';
                    showMessage("You found some food!");
                }

                // Check for a monster in the new position
                int monsterIndex = -1;
                for (int i = 0; i < MAX_MONSTERS; i++) {
                    if (monsters[i].active && monsters[i].x == newX && monsters[i].y == newY) {
                        monsterIndex = i;
                        break;
                    }
                }

                if (monsterIndex != -1) {
                    // Monster found, initiate combat
                    fightMonster(monsterIndex);
                    return 1; // A turn has passed
                } else {
                    // No monster, move the player
                    player.x = newX;
                    player.y = newY;
                    return 1; // A turn has passed
                }
            }
        }
    }
    return 0; // No turn passed
}

// Handle player input for help screen
int handleHelpInput(SDL_Event* e) {
    // Escape key handling is in the main loop
    return 0;
}

// Monster movement AI
void moveMonsters() {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].active) {
            // Monsters move based on their speed
            for (int j = 0; j < monsters[i].speed; j++) {
                // Check if player is in range
                if (getDistance(monsters[i].x, monsters[i].y, player.x, player.y) <= MONSTER_DETECTION_RANGE) {
                    int dx = player.x - monsters[i].x;
                    int dy = player.y - monsters[i].y;
                    int newX = monsters[i].x;
                    int newY = monsters[i].y;
                    int moved = 0;
                    
                    // Prioritize movement on the axis with the greater distance
                    if (abs(dx) > abs(dy)) {
                        newX += (dx > 0) ? 1 : -1;
                        if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT &&
                            map[newY][newX] != '#' && (newX != player.x || newY != player.y) &&
                            isOccupiedByMonster(newX, newY) == -1) {
                            monsters[i].x = newX;
                            moved = 1;
                        }
                    } else {
                        newY += (dy > 0) ? 1 : -1;
                        if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT &&
                            map[newY][newX] != '#' && (newX != player.x || newY != player.y) &&
                            isOccupiedByMonster(newX, newY) == -1) {
                            monsters[i].y = newY;
                            moved = 1;
                        }
                    }

                    // If the primary move failed, try the secondary move
                    if (!moved) {
                        if (abs(dx) > abs(dy)) {
                            newY = monsters[i].y + ((dy > 0) ? 1 : -1);
                            if (newY >= 0 && newY < MAP_HEIGHT &&
                                map[newY][monsters[i].x] != '#' &&
                                (monsters[i].x != player.x || newY != player.y) &&
                                isOccupiedByMonster(monsters[i].x, newY) == -1) {
                                monsters[i].y = newY;
                            }
                        } else {
                            newX = monsters[i].x + ((dx > 0) ? 1 : -1);
                            if (newX >= 0 && newX < MAP_WIDTH &&
                                map[monsters[i].y][newX] != '#' &&
                                (newX != player.x || monsters[i].y != player.y) &&
                                isOccupiedByMonster(newX, monsters[i].y) == -1) {
                                monsters[i].x = newX;
                            }
                        }
                    }
                }
            }
        }
    }
}


// Handle combat between player and monster
void fightMonster(int monsterIndex) {
    char tempBuffer[256];
    int playerDamage = rand() % (player.intelligence * 2) + 1;
    monsters[monsterIndex].hp -= playerDamage;
    snprintf(tempBuffer, sizeof(tempBuffer), "You hit the %s for %d damage!", monsters[monsterIndex].name, playerDamage);
    showMessage(tempBuffer);

    if (monsters[monsterIndex].hp <= 0) {
        player.score += monsters[monsterIndex].points;
        player.xp += monsters[monsterIndex].points; // Gain XP for defeating a monster
        
        // 50% chance to drop a food item
        if (rand() % 2 == 0) {
            player.foodInInventory++;
            snprintf(tempBuffer, sizeof(tempBuffer), "You defeated the %s and found some food!", monsters[monsterIndex].name);
        } else {
            snprintf(tempBuffer, sizeof(tempBuffer), "You defeated the %s!", monsters[monsterIndex].name);
        }
        monsters[monsterIndex].active = 0;
        showMessage(tempBuffer);
    } else {
        int monsterDamage = rand() % (5 + dungeonLevel) + 1; // Monsters do 1-5 damage + dungeon level
        player.hp -= monsterDamage;
        if (player.hp <= 0) {
            strncpy(player.causeOfDeath, monsters[monsterIndex].name, sizeof(player.causeOfDeath) - 1);
            player.causeOfDeath[sizeof(player.causeOfDeath) - 1] = '\0';
        }
        snprintf(tempBuffer, sizeof(tempBuffer), "The %s hits you for %d damage! Your HP is now %d/%d.", monsters[monsterIndex].name, monsterDamage, player.hp, player.maxHp);
        showMessage(tempBuffer);
    }
}

// New rest function to recover HP and Mana
void rest() {
    char tempBuffer[256];
    restCounter++;
    if (restCounter >= REST_TURNS_REQUIRED) {
        player.hp++;
        if (player.hp > player.maxHp) player.hp = player.maxHp;
        player.mana++;
        if (player.mana > player.maxMana) player.mana = player.maxMana;
        restCounter = 0;
        snprintf(tempBuffer, sizeof(tempBuffer), "You have rested and recovered 1 HP and 1 Mana!");
    } else {
        snprintf(tempBuffer, sizeof(tempBuffer), "Resting... (Turn %d/%d)", restCounter, REST_TURNS_REQUIRED);
    }
    showMessage(tempBuffer);
}

// New healing spell
void castHealSpell() {
    char tempBuffer[256];
    int manaCost = 3;
    if (player.mana >= manaCost) {
        player.mana -= manaCost;
        int healAmount = rand() % 5 + 3 + player.intelligence; // Heal for 3-7 + int amount
        player.hp += healAmount;
        if (player.hp > player.maxHp) {
            player.hp = player.maxHp;
        }
        snprintf(tempBuffer, sizeof(tempBuffer), "You cast a healing spell and recover %d HP!", healAmount);
    } else {
        snprintf(tempBuffer, sizeof(tempBuffer), "Not enough mana to cast the healing spell!");
    }
    showMessage(tempBuffer);
}

// New magic missile spell
void castMagicMissile(int dx, int dy) {
    char tempBuffer[256];
    int manaCost = 2;
    if (player.mana < manaCost) {
        snprintf(tempBuffer, sizeof(tempBuffer), "Not enough mana to cast magic missile!");
        showMessage(tempBuffer);
        return;
    }

    player.mana -= manaCost;
    
    // Animate missile
    int missileX = player.x;
    int missileY = player.y;

    while(1) {
        missileX += dx;
        missileY += dy;

        // Check for collision with wall or map boundaries
        if (missileX < 0 || missileX >= MAP_WIDTH || missileY < 0 || missileY >= MAP_HEIGHT || map[missileY][missileX] == '#') {
            snprintf(tempBuffer, sizeof(tempBuffer), "The magic missile hits a wall!");
            break;
        }

        // Check for collision with monster
        int monsterIndex = isOccupiedByMonster(missileX, missileY);
        if (monsterIndex != -1) {
            int damage = rand() % 5 + 1 + player.intelligence;
            monsters[monsterIndex].hp -= damage;
            snprintf(tempBuffer, sizeof(tempBuffer), "You cast magic missile at the %s for %d damage!", monsters[monsterIndex].name, damage);
            if (monsters[monsterIndex].hp <= 0) {
                player.score += monsters[monsterIndex].points;
                player.xp += monsters[monsterIndex].points; // Gain XP for defeating a monster
                
                snprintf(tempBuffer, sizeof(tempBuffer), "You defeated the %s!", monsters[monsterIndex].name);
                monsters[monsterIndex].active = 0;
            }
            break;
        }

        // Update screen to show missile
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        renderGame();
        drawText("*", (missileX-cameraX)*TILE_SIZE, (missileY-cameraY)*TILE_SIZE, (SDL_Color){255, 255, 0, 255});
        SDL_RenderPresent(renderer);
        SDL_Delay(50);
    }

    showMessage(tempBuffer);
}

// New Phase Door spell
void castPhaseDoorSpell() {
    char tempBuffer[256];
    int manaCost = 5;
    if (player.mana < manaCost) {
        snprintf(tempBuffer, sizeof(tempBuffer), "Not enough mana to cast Phase Door!");
        showMessage(tempBuffer);
        return;
    }

    player.mana -= manaCost;

    // Find a random, empty, walkable tile to teleport to
    int newX, newY;
    int attempts = 0;
    do {
        newX = rand() % MAP_WIDTH;
        newY = rand() % MAP_HEIGHT;
        attempts++;
        if (attempts > 1000) {
            snprintf(tempBuffer, sizeof(tempBuffer), "The spell fails to find a safe location!");
            showMessage(tempBuffer);
            return;
        }
    } while (!isTileWalkable(newX, newY) || isOccupiedByMonster(newX, newY) != -1);
    
    player.x = newX;
    player.y = newY;
    
    snprintf(tempBuffer, sizeof(tempBuffer), "You cast Phase Door and teleport to a new location!");
    showMessage(tempBuffer);
}


// New function to use a health potion
void useHealthPotion() {
    char tempBuffer[256];
    if (player.healthPotions > 0) {
        player.healthPotions--;
        int healAmount = rand() % 8 + 5; // Heal for 5-12 HP
        player.hp += healAmount;
        if (player.hp > player.maxHp) {
            player.hp = player.maxHp;
        }
        snprintf(tempBuffer, sizeof(tempBuffer), "You use a health potion and recover %d HP!", healAmount);
    } else {
        snprintf(tempBuffer, sizeof(tempBuffer), "You have no health potions!");
    }
    showMessage(tempBuffer);
}

// New function to eat food
void eatFood() {
    char tempBuffer[256];
    if (player.foodInInventory > 0) {
        player.foodInInventory--;
        player.hunger = 0;
        snprintf(tempBuffer, sizeof(tempBuffer), "You eat the food and are no longer hungry!");
    } else {
        snprintf(tempBuffer, sizeof(tempBuffer), "You have no food!");
    }
    showMessage(tempBuffer);
}

// Check if player has enough XP to level up
void checkLevelUp() {
    if (player.xp >= player.xpToNextLevel) {
        player.level++;
        player.xp -= player.xpToNextLevel; // Reset XP for the new level
        player.xpToNextLevel = player.xpToNextLevel * 2; // Increase XP required for the next level
        player.maxHp += 5; // Increase max HP
        player.hp = player.maxHp; // Fully heal on level up
        player.maxMana += 2; // Increase max Mana
        player.mana = player.maxMana; // Fully restore mana
        player.intelligence++; // Increase intelligence
        
        gameState = STATE_LEVELUP;
    }
}

// Mark tiles within the player's sight as explored
void updateVisibility() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (getDistance(player.x, player.y, x, y) <= player.visibilityRadius) {
                visibility[y][x] = 1;
            }
        }
    }
}

// Render the game state to the screen
void renderGame() {
    // Center the camera on the player
    cameraX = player.x - SCREEN_WIDTH / (2 * TILE_SIZE);
    cameraY = player.y - SCREEN_HEIGHT / (2 * TILE_SIZE);

    // Clamp the camera to the map boundaries
    if (cameraX < 0) {
        cameraX = 0;
    }
    if (cameraY < 0) {
        cameraY = 0;
    }
    if (cameraX > MAP_WIDTH - (SCREEN_WIDTH / TILE_SIZE)) {
        cameraX = MAP_WIDTH - (SCREEN_WIDTH / TILE_SIZE);
    }
    if (cameraY > MAP_HEIGHT - (SCREEN_HEIGHT / TILE_SIZE)) {
        cameraY = MAP_HEIGHT - (SCREEN_HEIGHT / TILE_SIZE);
    }

    // Render the dungeon map, only what is visible by the camera
    int visibleMapWidth = SCREEN_WIDTH / TILE_SIZE;
    int visibleMapHeight = SCREEN_HEIGHT / TILE_SIZE;

    for (int y = 0; y < visibleMapHeight; y++) {
        for (int x = 0; x < visibleMapWidth; x++) {
            int mapX = cameraX + x;
            int mapY = cameraY + y;
            if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
                if (visibility[mapY][mapX]) {
                    char tileChar[2];
                    tileChar[0] = map[mapY][mapX];
                    tileChar[1] = '\0';

                    int currentlyVisible = getDistance(player.x, player.y, mapX, mapY) <= player.visibilityRadius;
                    SDL_Color color;

                    if (map[mapY][mapX] == '#') {
                        color = currentlyVisible ? (SDL_Color){100, 100, 100, 255} : (SDL_Color){50, 50, 50, 255};
                    } else if (map[mapY][mapX] == '>') {
                        color = currentlyVisible ? (SDL_Color){255, 255, 0, 255} : (SDL_Color){128, 128, 0, 255};
                    } else if (map[mapY][mapX] == '!') {
                        color = currentlyVisible ? (SDL_Color){0, 255, 255, 255} : (SDL_Color){0, 128, 128, 255};
                    } else if (map[mapY][mapX] == 'F') {
                        color = currentlyVisible ? (SDL_Color){102, 51, 0, 255} : (SDL_Color){51, 25, 0, 255};
                    } else {
                        color = currentlyVisible ? (SDL_Color){255, 255, 255, 255} : (SDL_Color){150, 150, 150, 255};
                    }

                    drawText(tileChar, x * TILE_SIZE, y * TILE_SIZE, color);
                } else {
                    // Render dark tiles for unexplored areas
                    SDL_Color blackColor = {0, 0, 0, 255};
                    drawText(" ", x * TILE_SIZE, y * TILE_SIZE, blackColor);
                }
            }
        }
    }

    // Render monsters, only if they are currently within sight
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].active && monsters[i].x >= cameraX && monsters[i].x < cameraX + visibleMapWidth &&
            monsters[i].y >= cameraY && monsters[i].y < cameraY + visibleMapHeight &&
            getDistance(player.x, player.y, monsters[i].x, monsters[i].y) <= player.visibilityRadius) {
            char monsterChar[2];
            monsterChar[0] = monsters[i].symbol;
            monsterChar[1] = '\0';
            int screenX = (monsters[i].x - cameraX) * TILE_SIZE;
            int screenY = (monsters[i].y - cameraY) * TILE_SIZE;
            drawText(monsterChar, screenX, screenY, (SDL_Color){255, 0, 0, 255}); // Red for monsters
        }
    }

    // Render the player, also relative to the camera
    char playerChar[2] = {'@', '\0'};
    int playerScreenX = (player.x - cameraX) * TILE_SIZE;
    int playerScreenY = (player.y - cameraY) * TILE_SIZE;
    drawText(playerChar, playerScreenX, playerScreenY, (SDL_Color){0, 255, 0, 255}); // Green for player

    // Render player stats at the top of the screen (fixed position)
    char statsBuffer[256];
    snprintf(statsBuffer, sizeof(statsBuffer), "HP: %d/%d | Mana: %d/%d | Int: %d | Score: %d | Potions: %d | Food: %d | Lvl: %d | XP: %d/%d | Dlvl: %d",
            player.hp, player.maxHp, player.mana, player.maxMana, player.intelligence, player.score, player.healthPotions, player.foodInInventory, player.level, player.xp, player.xpToNextLevel, dungeonLevel);
    drawText(statsBuffer, 10, 10, (SDL_Color){255, 255, 255, 255});

    // Render message log at the bottom of the screen (fixed position)
    drawText(messageBuffer, 10, SCREEN_HEIGHT - TILE_SIZE, (SDL_Color){255, 255, 255, 255});
}

// Function to render the game over screen
void renderGameOverScreen() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    char tombstone[] = 
    "       .---. \n"
    "      /     \\\n"
    "      | RIP |\n"
    "      |     |\n"
    "      |     |\n"
    "      '-----'";

    char* line;
    char tombstoneCopy[256];
    strncpy(tombstoneCopy, tombstone, sizeof(tombstoneCopy) - 1);
    tombstoneCopy[sizeof(tombstoneCopy) - 1] = '\0';
    
    int yOffset = 100;
    line = strtok(tombstoneCopy, "\n");
    while(line != NULL) {
        int textWidth, textHeight;
        TTF_SizeText(font, line, &textWidth, &textHeight);
        drawText(line, (SCREEN_WIDTH - textWidth) / 2, yOffset, (SDL_Color){255, 255, 255, 255});
        yOffset += textHeight;
        line = strtok(NULL, "\n");
    }

    char deathMessage[100];
    snprintf(deathMessage, sizeof(deathMessage), "You have died!");
    int deathMessageWidth;
    TTF_SizeText(font, deathMessage, &deathMessageWidth, NULL);
    drawText(deathMessage, (SCREEN_WIDTH - deathMessageWidth) / 2, yOffset + 24, (SDL_Color){255, 255, 255, 255});

    char causeMessage[100];
    snprintf(causeMessage, sizeof(causeMessage), "Cause of Death: %s", player.causeOfDeath);
    int causeMessageWidth;
    TTF_SizeText(font, causeMessage, &causeMessageWidth, NULL);
    drawText(causeMessage, (SCREEN_WIDTH - causeMessageWidth) / 2, yOffset + 48, (SDL_Color){255, 255, 255, 255});

    char scoreMessage[100];
    snprintf(scoreMessage, sizeof(scoreMessage), "Final Score: %d", player.score);
    int scoreMessageWidth;
    TTF_SizeText(font, scoreMessage, &scoreMessageWidth, NULL);
    drawText(scoreMessage, (SCREEN_WIDTH - scoreMessageWidth) / 2, yOffset + 72, (SDL_Color){255, 255, 255, 255});

    SDL_RenderPresent(renderer);
}

// Function to render the help screen
void renderHelpScreen() {
    // Render the game in the background with a slight fade
    renderGame();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180); // Dark semi-transparent overlay
    SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &rect);

    int xPos = SCREEN_WIDTH / 2 - 200;
    int yPos = SCREEN_HEIGHT / 2 - 200;

    drawText("--- Controls ---", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("Arrow Keys: Move", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("r: Rest (recover HP/Mana)", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("h: Cast Healing Spell", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("f + Arrow Key: Cast Magic Missile", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("t: Cast Phase Door", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("p: Use Health Potion", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("e: Eat Food", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;
    drawText("?: Show Help (this screen)", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE * 2;
    drawText("Press ESC to return to the game", xPos, yPos, (SDL_Color){255, 255, 255, 255});
}

// Function to render the win screen
void renderWinScreen() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int xPos = SCREEN_WIDTH / 2 - 200;
    int yPos = SCREEN_HEIGHT / 2 - 200;
    
    drawText("Congratulations!", xPos, yPos, (SDL_Color){0, 255, 0, 255});
    yPos += TILE_SIZE * 2;
    drawText("You have defeated the Lich Lord!", xPos, yPos, (SDL_Color){255, 255, 255, 255});
    yPos += TILE_SIZE;

    char scoreMessage[100];
    snprintf(scoreMessage, sizeof(scoreMessage), "Final Score: %d", player.score);
    int scoreMessageWidth;
    TTF_SizeText(font, scoreMessage, &scoreMessageWidth, NULL);
    drawText(scoreMessage, (SCREEN_WIDTH - scoreMessageWidth) / 2, yPos, (SDL_Color){255, 255, 255, 255});
    
    SDL_RenderPresent(renderer);
}

void renderLevelUpScreen() {
    renderGame();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180); // Dark semi-transparent overlay
    SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &rect);
    
    char message[100];
    snprintf(message, sizeof(message), "Welcome to Level %d!", player.level);
    int messageWidth;
    TTF_SizeText(font, message, &messageWidth, NULL);
    drawText(message, (SCREEN_WIDTH - messageWidth) / 2, SCREEN_HEIGHT / 2, (SDL_Color){0, 255, 0, 255});
    SDL_RenderPresent(renderer);
}

// A helper function to draw text to the screen
void drawText(const char* text, int x, int y, SDL_Color color) {
    if (font == NULL) return;
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, color);
    if (textSurface != NULL) {
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_Rect renderQuad = {x, y, textSurface->w, textSurface->h};
        SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }
}

// Function to display a message to the player
void showMessage(const char* message) {
    strncpy(messageBuffer, message, sizeof(messageBuffer) - 1);
    messageBuffer[sizeof(messageBuffer) - 1] = '\0';
    messageTimer = 2; // Set timer to 2 so it stays for 1 turn after the current one
}

// Simple Manhattan distance calculation
int getDistance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// Check if a tile is occupied by a monster
int isOccupiedByMonster(int x, int y) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].active && monsters[i].x == x && monsters[i].y == y) {
            return i;
        }
    }
    return -1;
}

// Check if a tile is walkable (not a wall)
int isTileWalkable(int x, int y) {
    if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT && map[y][x] != '#') {
        return 1;
    }
    return 0;
}
