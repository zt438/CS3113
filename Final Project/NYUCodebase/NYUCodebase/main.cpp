#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#include "ShaderProgram.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <math.h>
#include <vector>

// for parsing map
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>

// for AI
#include <queue>
#include <utility> // for pair
#include <tuple>
using namespace std;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define FIXED_TIMESTEP 0.01666666f
#define MAX_TIMESTEPS 6
float accumulator = 0.0f;

#define MAP_TILE_SIZE 0.1f
#define LEVEL_1_WIDTH 20
#define LEVEL_1_HEIGHT 10
#define LEVEL_2_WIDTH 40
#define LEVEL_2_HEIGHT 21
#define LEVEL_3_WIDTH 30
#define LEVEL_3_HEIGHT 30
#define MAP_SPRITE_COUNT_X 10
#define MAP_SPRITE_COUNT_Y 10

#define MOVEMENT_DELAY 0.2f
#define FADEOUT_TIME 3.0f

enum GameState { STATE_TITLE, STATE_GAME, STATE_GAMEOVER, STATE_NEXT_LEVEL };
enum EntityType { ENTITY_NONE, ENTITY_PLAYER, ENTITY_SKULL, ENTITY_TORCH, ENTITY_SIDE_TORCH, ENTITY_DOOR, ENTITY_KEY, ENTITY_EXIT, ENTITY_SWORD };
enum EntityState { ENTITY_IDLE, ENTITY_CHASE };
enum Direction { DIRECTION_NONE, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_RIGHT };

GameState state;

ShaderProgram program;
glm::mat4 modelMatrix;
glm::mat4 viewMatrix;

// for animation
const int animationFrames[] = { 0, 1, 2, 3 };
const int numFrames = 4;
float animationElapsed = 0.0f;
float framesPerSecond = 10.0f;
int currentIndex = 0;
float fadeout = 0.0f;

// for sound
Mix_Chunk *hit_wall;
Mix_Chunk *swordSound;
Mix_Chunk *keySound;
Mix_Chunk *doorSound;
Mix_Music *bgm;

int mapWidth;
int mapHeight;
short **levelData;
EntityType **entityPositionData; // for movement checks

GLuint font;
GLuint playerSpriteSheet;
GLuint skullSpriteSheet;
GLuint torchSpriteSheet;
GLuint sideTorchSpriteSheet;
GLuint keySpriteSheet;
GLuint mapSpriteSheet;
GLuint swordSprite;
string gameOverMessage = "";

int currentLevel = 1;
int keyCount = 0;

SDL_Window* displayWindow;

float lerp(float v0, float v1, float t) {
	return (1.0 - t) * v0 + t * v1;
}

GLuint LoadTexture(const char *filePath)
{
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);

	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}

	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	stbi_image_free(image);
	return retTexture;
}

void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing) {
	float character_size = 1.0 / 16.0f;

	std::vector<float> vertexData;
	std::vector<float> texCoordData;

	for (unsigned i = 0; i < text.size(); i++) {
		int spriteIndex = (int)text[i];

		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;

		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			});

		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + character_size,
			texture_x + character_size, texture_y,
			texture_x + character_size, texture_y + character_size,
			texture_x + character_size, texture_y,
			texture_x, texture_y + character_size,
			});
	}

	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glUseProgram(program.programID);

	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program.positionAttribute);
	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);

	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

class SheetSprite {
public:
	SheetSprite() {}
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size) {
		this->textureID = textureID;
		this->u = u;
		this->v = v;
		this->width = width;
		this->height = height;
		this->size = size;
	}

	void Draw(ShaderProgram &program) {
glBindTexture(GL_TEXTURE_2D, textureID);

GLfloat texCoords[] = {
	u, v + height,
	u + width, v,
	u, v,
	u + width, v,
	u, v + height,
	u + width, v + height
};

float aspect = 1;

float vertices[] = {
	-0.5f * size * aspect, -0.5f * size,
	0.5f * size * aspect, 0.5f * size,
	-0.5f * size * aspect, 0.5f * size,
	0.5f * size * aspect, 0.5f * size,
	-0.5f * size * aspect, -0.5f * size,
	0.5f * size * aspect, -0.5f * size,
};

glUseProgram(program.programID);

glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
glEnableVertexAttribArray(program.positionAttribute);
glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
glEnableVertexAttribArray(program.texCoordAttribute);

glDrawArrays(GL_TRIANGLES, 0, 6);

glDisableVertexAttribArray(program.positionAttribute);
glDisableVertexAttribArray(program.texCoordAttribute);
	}

	float size;
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

void worldToTileCoordinates(float worldX, float worldY, int& gridX, int& gridY) {
	gridX = (int)(worldX / MAP_TILE_SIZE);
	gridY = (int)(worldY / -MAP_TILE_SIZE);
}

bool isSolid(int tileIndex) {
	// the walls
	return ((tileIndex >= 0 && tileIndex <= 5)
		|| tileIndex == 10 || tileIndex == 15
		|| tileIndex == 10 || tileIndex == 15
		|| tileIndex == 20 || tileIndex == 25
		|| tileIndex == 30 || tileIndex == 35
		|| (tileIndex >= 40 && tileIndex <= 45)
		|| (tileIndex >= 50 && tileIndex <= 55));
}

int distance(int tileX, int tileY, int goalX, int goalY) {
	return abs(tileX - goalX) + abs(tileY - goalY);
}

struct TupleCompare {
	bool operator()(const tuple<int, int, int, int>& first, const tuple<int, int, int, int>& second) {
		return get<2>(first) + get<3>(first) > get<2>(second) + get<3>(second);
	}
};

Direction aStarSearch(int tileX, int tileY, int goalX, int goalY) {
	Direction result = DIRECTION_NONE;

	// stores the cost for each position in the level
	// tuple<prevX, prevY, cost>
	vector<vector<tuple<int, int, int>>> path(mapHeight,
		vector<tuple<int, int, int>>(mapWidth, make_tuple(INT_MAX, INT_MAX, INT_MAX)));

	path[tileY][tileX] = make_tuple(-1, -1, 0);

	// pq contains tuple<tileX, tileY, cost, distance>
	priority_queue<tuple<int, int, int, int>, vector<tuple<int, int, int, int>>, TupleCompare> pq;
	pq.push(make_tuple(tileX, tileY, 0, distance(tileX, tileY, goalX, goalY)));
	int currentX = -1, currentY = -1;

	while (!pq.empty()) {
		currentX = get<0>(pq.top());
		currentY = get<1>(pq.top());
		int currentCost = get<2>(pq.top());
		int currentDist = get<3>(pq.top());
		pq.pop();

		if (currentX == goalX && currentY == goalY) {
			break;
		}

		// check each direction
		if (!isSolid(levelData[currentY + 1][currentX])
			&& entityPositionData[currentY + 1][currentX] != ENTITY_SKULL
			&& entityPositionData[currentY + 1][currentX] != ENTITY_DOOR) {

			if (currentCost + 1 < get<2>(path[currentY + 1][currentX])) {
				pq.push(make_tuple(currentX, currentY + 1, currentCost + 1, distance(currentX, currentY + 1, goalX, goalY)));
				path[currentY + 1][currentX] = make_tuple(currentX, currentY, currentCost + 1);
			}
		}

		if (!isSolid(levelData[currentY - 1][currentX])
			&& entityPositionData[currentY - 1][currentX] != ENTITY_SKULL
			&& entityPositionData[currentY - 1][currentX] != ENTITY_DOOR) {

			if (currentCost + 1 < get<2>(path[currentY - 1][currentX])) {
				pq.push(make_tuple(currentX, currentY - 1, currentCost + 1, distance(currentX, currentY - 1, goalX, goalY)));
				path[currentY - 1][currentX] = make_tuple(currentX, currentY, currentCost + 1);
			}
		}

		if (!isSolid(levelData[currentY][currentX + 1])
			&& entityPositionData[currentY][currentX + 1] != ENTITY_SKULL
			&& entityPositionData[currentY][currentX + 1] != ENTITY_DOOR) {

			if (currentCost + 1 < get<2>(path[currentY][currentX + 1])) {
				pq.push(make_tuple(currentX + 1, currentY, currentCost + 1, distance(currentX + 1, currentY, goalX, goalY)));
				path[currentY][currentX + 1] = make_tuple(currentX, currentY, currentCost + 1);
			}
		}

		if (!isSolid(levelData[currentY][currentX - 1])
			&& entityPositionData[currentY][currentX - 1] != ENTITY_SKULL
			&& entityPositionData[currentY][currentX - 1] != ENTITY_DOOR) {

			if (currentCost + 1 < get<2>(path[currentY][currentX - 1])) {
				pq.push(make_tuple(currentX - 1, currentY, currentCost + 1, distance(currentX - 1, currentY, goalX, goalY)));
				path[currentY][currentX - 1] = make_tuple(currentX, currentY, currentCost + 1);
			}
		}
	}

	if (currentX >= 0 && currentY >= 0) {
		int prevX = get<0>(path[currentY][currentX]);
		int prevY = get<1>(path[currentY][currentX]);
		while (get<0>(path[prevY][prevX]) != -1 || get<1>(path[prevY][prevX]) != -1) {
			int newX = get<0>(path[currentY][currentX]);
			int newY = get<1>(path[currentY][currentX]);
			prevX = get<0>(path[newY][newX]);
			prevY = get<1>(path[newY][newX]);
			currentX = newX;
			currentY = newY;
		}

		// figure out the direction
		if (currentX > prevX) {
			result = DIRECTION_RIGHT;
		}
		else if (currentX < prevX) {
			result = DIRECTION_LEFT;
		}
		else if (currentY < prevY) {
			result = DIRECTION_UP;
		}
		else if (currentY > prevY) {
			result = DIRECTION_DOWN;
		}

	}
	return result;
}

class Entity {
public:
	Entity() {}
	Entity(glm::vec3 position, glm::vec3 size, bool isStatic, EntityType type, bool faceRight) {
		this->position = position;
		this->size = size;
		this->isStatic = isStatic;
		this->entityType = type;
		this->faceRight = faceRight;
	}

	void Draw(ShaderProgram &program) {
		sprite.Draw(program);
	}

	void Update(float elapsed) {
		if (!isStatic) {
			// check which movement is allowed
			int tileX, tileY;
			worldToTileCoordinates(position.x, position.y, tileX, tileY);

			if (isSolid(levelData[tileY][tileX + 1]) 
				|| entityPositionData[tileY][tileX + 1] == ENTITY_DOOR
				|| entityPositionData[tileY][tileX + 1] == ENTITY_SKULL) {
				rightBlocked = true;
			}
			else {
				rightBlocked = false;
			}
			if (isSolid(levelData[tileY][tileX - 1])
				|| entityPositionData[tileY][tileX - 1] == ENTITY_DOOR
				|| entityPositionData[tileY][tileX - 1] == ENTITY_SKULL) {
				leftBlocked = true;
			}
			else {
				leftBlocked = false;
			}
			if (isSolid(levelData[tileY - 1][tileX])
				|| entityPositionData[tileY - 1][tileX] == ENTITY_DOOR
				|| entityPositionData[tileY - 1][tileX] == ENTITY_SKULL) {
				upBlocked = true;
			}
			else {
				upBlocked = false;
			}
			if (isSolid(levelData[tileY + 1][tileX])
				|| entityPositionData[tileY + 1][tileX] == ENTITY_DOOR
				|| entityPositionData[tileY + 1][tileX] == ENTITY_SKULL) {
				downBlocked = true;
			}
			else {
				downBlocked = false;
			}

			if (entityType == ENTITY_SKULL && currentState == ENTITY_IDLE) {
				// check if player is in line of sight
				queue<pair<int, int>> lineOfSight;
				// check immediate surrounding (1 tile in each cardinal direction)
				lineOfSight.push(pair<int, int>(tileY, tileX + 1));
				lineOfSight.push(pair<int, int>(tileY, tileX - 1));
				lineOfSight.push(pair<int, int>(tileY + 1, tileX));
				lineOfSight.push(pair<int, int>(tileY - 1, tileX));

				// change entity state to chasing if player is in line of sight
				while (!lineOfSight.empty()) {
					int checkY = lineOfSight.front().first;
					int checkX = lineOfSight.front().second;

					if (entityPositionData[checkY][checkX] == ENTITY_PLAYER) {
						currentState = ENTITY_CHASE;
						break;
					}
					if (!isSolid(levelData[checkY][checkX])) {
						// check next neighbor if current distance from original position is less than 2
						if (abs(tileY - checkY) + abs(tileX - checkX) < 2) {
							if (tileY != checkY || tileX != checkX) {
							lineOfSight.push(pair<int, int>(checkY, checkX + 1));
							lineOfSight.push(pair<int, int>(checkY, checkX - 1));
							lineOfSight.push(pair<int, int>(checkY + 1, checkX));
							lineOfSight.push(pair<int, int>(checkY - 1, checkX));
							}
						}
					}
					lineOfSight.pop();
				}
			}
		}
	}

	void clearPositionData() {
		int tileX, tileY;
		worldToTileCoordinates(position.x, position.y, tileX, tileY);
		entityPositionData[tileY][tileX] = ENTITY_NONE;
	};
	void setPositionData() {
		int tileX, tileY;
		worldToTileCoordinates(position.x, position.y, tileX, tileY);
		entityPositionData[tileY][tileX] = entityType;
	};

	// basic movement AI for non static enemy only
	void Move(glm::vec3 playerPosition) {
		Direction next = DIRECTION_NONE;
		if (currentState == ENTITY_CHASE) {
			// move towards the player
			int playerTileX, playerTileY, thisTileX, thisTileY;
			worldToTileCoordinates(playerPosition.x, playerPosition.y, playerTileX, playerTileY);
			worldToTileCoordinates(position.x, position.y, thisTileX, thisTileY);

			// A* search is not really necessary since skull only sees player if within 2 tile distance
			// and the walls are at least 2 tiles wide
			next = aStarSearch(thisTileX, thisTileY, playerTileX, playerTileY);

			clearPositionData();
			// make the move
			if (next == DIRECTION_LEFT) {
				position.x -= MAP_TILE_SIZE;
				faceRight = false;
			}
			else if (next == DIRECTION_RIGHT) {
				position.x += MAP_TILE_SIZE;
				faceRight = true;
			}
			else if (next == DIRECTION_UP) {
				position.y += MAP_TILE_SIZE;
			}
			else if (next == DIRECTION_DOWN) {
				position.y -= MAP_TILE_SIZE;
			}
			setPositionData();
		}
		else if (currentState == ENTITY_IDLE || next == DIRECTION_NONE) {
			int tileX, tileY;
			worldToTileCoordinates(position.x, position.y, tileX, tileY);

			// pick a random nonblocked direction and move that way
			vector<string> potentialDirections;
			if (!(isSolid(levelData[tileY][tileX - 1])
				|| entityPositionData[tileY][tileX - 1] == ENTITY_DOOR
				|| entityPositionData[tileY][tileX - 1] == ENTITY_SKULL)) {
				potentialDirections.push_back("left"); 
			}
			if (!(isSolid(levelData[tileY][tileX + 1])
				|| entityPositionData[tileY][tileX + 1] == ENTITY_DOOR
				|| entityPositionData[tileY][tileX + 1] == ENTITY_SKULL)) {
				potentialDirections.push_back("right");
			}
			if (!(isSolid(levelData[tileY - 1][tileX])
				|| entityPositionData[tileY - 1][tileX] == ENTITY_DOOR
				|| entityPositionData[tileY - 1][tileX] == ENTITY_SKULL)) {
				potentialDirections.push_back("up");
			}
			if (!(isSolid(levelData[tileY + 1][tileX])
				|| entityPositionData[tileY + 1][tileX] == ENTITY_DOOR
				|| entityPositionData[tileY + 1][tileX] == ENTITY_SKULL)) {
				potentialDirections.push_back("down");
			}

			if (potentialDirections.empty()) {
				// no movement if no moves available
				return;
			}

			int randomMove = rand() % potentialDirections.size();
			
			clearPositionData();
			// make the move
			if (potentialDirections[randomMove] == "left") {
				position.x -= MAP_TILE_SIZE;
				faceRight = false;
			}
			else if (potentialDirections[randomMove] == "right") {
				position.x += MAP_TILE_SIZE;
				faceRight = true;
			}
			else if (potentialDirections[randomMove] == "up") {
				position.y += MAP_TILE_SIZE;
			}
			else if (potentialDirections[randomMove] == "down") {
				position.y -= MAP_TILE_SIZE;
			}
			setPositionData();
		}
	}

	bool placeKey(Direction d);
	bool attack(Direction d);

	bool collided(Entity& other) {
		// just check if occupying the same tile
		int thisTileX, thisTileY, otherTileX, otherTileY;
		worldToTileCoordinates(position.x, position.y, thisTileX, thisTileY);
		worldToTileCoordinates(other.position.x, other.position.y, otherTileX, otherTileY);

		return ((thisTileX == otherTileX) && (thisTileY == otherTileY));
	}

	SheetSprite sprite;

	glm::vec3 position;
	glm::vec3 size;

	bool isStatic;
	EntityType entityType;

	bool faceRight;

	bool leftBlocked = false;
	bool rightBlocked = false;
	bool upBlocked = false;
	bool downBlocked = false;

	int timeRemaining = 2; // for sword entity only
	
	EntityState currentState = ENTITY_IDLE;
};

Entity player;
vector<Entity> enemies;
vector<Entity> torches;
vector<Entity> keysVector;
vector<Entity> doors;
Entity exitLadder;
vector<Entity> swords;

bool Entity::placeKey(Direction d) {
	if (keyCount > 0) {
		int tileX, tileY;
		worldToTileCoordinates(position.x, position.y, tileX, tileY);

		switch (d) {
		case DIRECTION_UP:
			if (entityPositionData[tileY - 1][tileX] == ENTITY_DOOR) {
				keysVector.push_back(Entity(glm::vec3(position.x, position.y + MAP_TILE_SIZE, 1),
					glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_KEY, true));
				keyCount--;
				return true;
			}
			break;
		case DIRECTION_DOWN:
			if (entityPositionData[tileY + 1][tileX] == ENTITY_DOOR) {
				keysVector.push_back(Entity(glm::vec3(position.x, position.y - MAP_TILE_SIZE, 1),
					glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_KEY, true));
				keyCount--;
				return true;
			}
			break;
		case DIRECTION_LEFT:
			if (entityPositionData[tileY][tileX - 1] == ENTITY_DOOR) {
				keysVector.push_back(Entity(glm::vec3(position.x - MAP_TILE_SIZE, position.y, 1),
					glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_KEY, true));
				keyCount--;
				return true;
			}
			break;
		case DIRECTION_RIGHT:
			if (entityPositionData[tileY][tileX + 1] == ENTITY_DOOR) {
				keysVector.push_back(Entity(glm::vec3(position.x + MAP_TILE_SIZE, position.y, 1),
					glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_KEY, true));
				keyCount--;
				return true;
			}
			break;
		}
	}
	return false;
}

bool Entity::attack(Direction d) {
	int tileX, tileY;
	worldToTileCoordinates(position.x, position.y, tileX, tileY);

	switch (d) {
	case DIRECTION_UP:
		if (entityPositionData[tileY - 1][tileX] == ENTITY_SKULL) {
			swords.push_back(Entity(glm::vec3(position.x, position.y + MAP_TILE_SIZE, 1),
				glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_SWORD, true));
			swords[swords.size() - 1].sprite = SheetSprite(swordSprite, 0.0f, 0.0f, 0.25f, 1.0f, 0.10f);
			return true;
		}
		break;
	case DIRECTION_DOWN:
		if (entityPositionData[tileY + 1][tileX] == ENTITY_SKULL) {
			swords.push_back(Entity(glm::vec3(position.x, position.y - MAP_TILE_SIZE, 1),
				glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_SWORD, true));
			swords[swords.size() - 1].sprite = SheetSprite(swordSprite, 0.5f, 0.0f, 0.25f, 1.0f, 0.10f);
			return true;
		}
		break;
	case DIRECTION_LEFT:
		if (entityPositionData[tileY][tileX - 1] == ENTITY_SKULL) {
			swords.push_back(Entity(glm::vec3(position.x - MAP_TILE_SIZE, position.y, 1),
				glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_SWORD, true));
			swords[swords.size() - 1].sprite = SheetSprite(swordSprite, 0.75f, 0.0f, 0.25f, 1.0f, 0.10f);
			return true;
		}
		break;
	case DIRECTION_RIGHT:
		if (entityPositionData[tileY][tileX + 1] == ENTITY_SKULL) {
			swords.push_back(Entity(glm::vec3(position.x + MAP_TILE_SIZE, position.y, 1),
				glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_SWORD, true));
			swords[swords.size() - 1].sprite = SheetSprite(swordSprite, 0.25f, 0.0f, 0.25f, 1.0f, 0.10f);
			return true;
		}
		break;
	}
	return false;
}

vector<float> vertexData;
vector<float> texCoordData;
void drawMap() {
	int levelHeight = LEVEL_1_HEIGHT;
	int levelWidth = LEVEL_1_WIDTH;
	if (currentLevel == 2) {
		levelHeight = LEVEL_2_HEIGHT;
		levelWidth = LEVEL_2_WIDTH;
	}
	else if (currentLevel == 3) {
		levelHeight = LEVEL_3_HEIGHT;
		levelWidth = LEVEL_3_WIDTH;
	}
	for (int y = 0; y < levelHeight; y++) {
		for (int x = 0; x < levelWidth; x++) {
			if (levelData[y][x] != 0) {
				float u = (float)(((int)levelData[y][x]) % MAP_SPRITE_COUNT_X) / (float)MAP_SPRITE_COUNT_X;
				float v = (float)(((int)levelData[y][x]) / MAP_SPRITE_COUNT_X) / (float)MAP_SPRITE_COUNT_Y;

				float spriteWidth = 1.0f / (float)MAP_SPRITE_COUNT_X;
				float spriteHeight = 1.0f / (float)MAP_SPRITE_COUNT_Y;

				vertexData.insert(vertexData.end(), {
					MAP_TILE_SIZE * x, -MAP_TILE_SIZE * y,
					MAP_TILE_SIZE * x, (-MAP_TILE_SIZE * y) - MAP_TILE_SIZE,
					(MAP_TILE_SIZE * x) + MAP_TILE_SIZE, (-MAP_TILE_SIZE * y) - MAP_TILE_SIZE,

					MAP_TILE_SIZE * x, -MAP_TILE_SIZE * y,
					(MAP_TILE_SIZE * x) + MAP_TILE_SIZE, (-MAP_TILE_SIZE * y) - MAP_TILE_SIZE,
					(MAP_TILE_SIZE * x) + MAP_TILE_SIZE, -MAP_TILE_SIZE * y
					});

				texCoordData.insert(texCoordData.end(), {
					u, v,
					u, v + (spriteHeight),
					u + spriteWidth, v + (spriteHeight),
					u, v,
					u + spriteWidth, v + (spriteHeight),
					u + spriteWidth, v
					});
			}
		}
	}
}

void renderMap() {
	glUseProgram(program.programID);
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program.positionAttribute);
	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program.texCoordAttribute);

	modelMatrix = glm::mat4(1.0f);
	program.SetModelMatrix(modelMatrix);

	glBindTexture(GL_TEXTURE_2D, mapSpriteSheet);
	glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

bool readHeader(std::ifstream &stream) {
	string line;
	mapWidth = -1;
	mapHeight = -1;
	while (getline(stream, line)) {
		if (line == "") { break; }

		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);

		if (key == "width") {
			mapWidth = atoi(value.c_str());
		}
		else if (key == "height") {
			mapHeight = atoi(value.c_str());
		}
	}

	if (mapWidth == -1 || mapHeight == -1) {
		return false;
	}
	else { // allocate our map data
		levelData = new short*[mapHeight];
		entityPositionData = new EntityType*[mapHeight];
		for (int i = 0; i < mapHeight; ++i) {
			levelData[i] = new short[mapWidth];
			entityPositionData[i] = new EntityType[mapWidth];
		}
		return true;
	}
}

bool readLayerData(std::ifstream &stream) {
	string line;
	while (getline(stream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "data") {
			for (int y = 0; y < mapHeight; y++) {
				getline(stream, line);
				istringstream lineStream(line);
				string tile;

				for (int x = 0; x < mapWidth; x++) {
					getline(lineStream, tile, ',');
					unsigned char val = (unsigned char)atoi(tile.c_str());
					if (val > 0) {
						// be careful, the tiles in this format are indexed from 1 not 0
						levelData[y][x] = val - 1;
					}
					else {
						levelData[y][x] = 0;
					}
					entityPositionData[y][x] = ENTITY_NONE;
				}
			}
		}
	}
	return true;
}

void placeEntity(const string& type, float x, float y) {
	if (type == "Player") {
		player = Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), false, ENTITY_PLAYER, true);
		player.sprite = SheetSprite(playerSpriteSheet, 0.0f, 0.0f, 0.25f, 1.0f, 0.10f);
	}
	else if (type == "Skull") {
		enemies.push_back(Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), false, ENTITY_SKULL, false));
		enemies[enemies.size() - 1].sprite = SheetSprite(skullSpriteSheet, 0.0f, 0.0f, 0.25f, 1.0f, 0.10f);
	}
	else if (type == "Torch") {
		torches.push_back(Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_TORCH, true));
		torches[torches.size() - 1].sprite = SheetSprite(torchSpriteSheet, 0.0f, 0.0f, 0.25f, 1.0f, 0.10f);
	}
	else if (type == "Side_Torch") {
		torches.push_back(Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_SIDE_TORCH, true));
		torches[torches.size() - 1].sprite = SheetSprite(sideTorchSpriteSheet, 0.0f, 0.0f, 0.25f, 1.0f, 0.10f);
	}
	else if (type == "Key") {
		keysVector.push_back(Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_KEY, true));
		keysVector[keysVector.size() - 1].sprite = SheetSprite(keySpriteSheet, 0.0f, 0.0f, 0.25f, 1.0f, 0.10f);
	}
	else if (type == "Door") {
		doors.push_back(Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_DOOR, true));
		doors[doors.size() - 1].sprite = SheetSprite(mapSpriteSheet, 0.6f, 0.4f, 0.1f, 0.1f, 0.10f);
	}
	else if (type == "Exit") {
		exitLadder = Entity(glm::vec3(x, y, 1), glm::vec3(MAP_TILE_SIZE, MAP_TILE_SIZE, 1), true, ENTITY_EXIT, true);
		exitLadder.sprite = SheetSprite(mapSpriteSheet, 0.9f, 0.3f, 0.1f, 0.1f, 0.10f);
	}
}

bool readEntityData(std::ifstream &stream) {
	string line;
	string type;

	while (getline(stream, line)) {
		if (line == "") { break; }

		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);

		if (key == "type") {
			type = value;
		}
		else if (key == "location") {
			istringstream lineStream(value);
			string xPosition, yPosition;
			getline(lineStream, xPosition, ',');
			getline(lineStream, yPosition, ',');

			float placeX = atoi(xPosition.c_str())*MAP_TILE_SIZE + MAP_TILE_SIZE / 2;
			float placeY = atoi(yPosition.c_str())*-MAP_TILE_SIZE + MAP_TILE_SIZE / 2;
			placeEntity(type, placeX, placeY);

			int tileX, tileY;
			worldToTileCoordinates(placeX, placeY, tileX, tileY);
			if (type == "Player") {
				entityPositionData[tileY][tileX] = ENTITY_PLAYER;
			}
			else if (type == "Skull") {
				entityPositionData[tileY][tileX] = ENTITY_SKULL;
			}
			else if (type == "Door") {
				entityPositionData[tileY][tileX] = ENTITY_DOOR;
			}
		}
	}
	return true;
}

void setupScene(const string& mapFile) {
	ifstream infile(mapFile);
	string line;
	while (getline(infile, line)) {
		if (line == "[header]") {
			if (!readHeader(infile))
				return;
		}
		else if (line == "[layer]") {
			readLayerData(infile);
		}
		else if (line == "[Entity]") {
			readEntityData(infile);
		}
	}

	drawMap();
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Some Platformer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	// textures
	font = LoadTexture(RESOURCE_FOLDER"font1.png");
	playerSpriteSheet = LoadTexture(RESOURCE_FOLDER"priest2_framesheet.png");
	skullSpriteSheet = LoadTexture(RESOURCE_FOLDER"skull_framesheet.png");
	torchSpriteSheet = LoadTexture(RESOURCE_FOLDER"torch_framesheet.png");
	sideTorchSpriteSheet = LoadTexture(RESOURCE_FOLDER"side_torch_framesheet.png");
	keySpriteSheet = LoadTexture(RESOURCE_FOLDER"key_framesheet.png");
	mapSpriteSheet = LoadTexture(RESOURCE_FOLDER"Dungeon_Tileset.png");
	swordSprite = LoadTexture(RESOURCE_FOLDER"sword.png");

	// sounds
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
	hit_wall = Mix_LoadWAV(RESOURCE_FOLDER"hit_wall.wav");
	keySound = Mix_LoadWAV(RESOURCE_FOLDER"key.wav");
	doorSound = Mix_LoadWAV(RESOURCE_FOLDER"door.wav");
	swordSound = Mix_LoadWAV(RESOURCE_FOLDER"sword.wav");
	bgm = Mix_LoadMUS(RESOURCE_FOLDER"TRG_Banks_Christmas_Town.mp3");
	Mix_VolumeMusic(20);
	Mix_PlayMusic(bgm, -1);

	state = STATE_TITLE;

	float lastFrameTicks = 0.0f;

	glViewport(0, 0, 640, 360);
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);

	ShaderProgram untexturedProgram;
	untexturedProgram.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);
	glUseProgram(program.programID);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float currentMovementDelay = 0.0f;

	SDL_Event event;
	bool done = false;
	while (!done) {
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			// quit the game
			if (keys[SDL_SCANCODE_Q]) {
				done = true;
			}
			if (state == STATE_TITLE) {
				if (keys[SDL_SCANCODE_SPACE]) {
					// clear out the vectors
					enemies.clear();
					torches.clear();
					keysVector.clear();
					doors.clear();
					swords.clear();
					keyCount = 0;

					// clear out vertex and texcoord data
					vertexData.clear();
					texCoordData.clear();

					currentLevel = 1;
					setupScene("level1.txt");

					// center camera on the player
					viewMatrix = glm::mat4(1.0f);
					viewMatrix = glm::scale(viewMatrix, glm::vec3(2.0f, 2.0f, 1.0f));
					viewMatrix = glm::translate(viewMatrix, -player.position);
					program.SetViewMatrix(viewMatrix);

					state = STATE_GAME;
				}
			}
			else if (state == STATE_NEXT_LEVEL) {
				if (keys[SDL_SCANCODE_SPACE]) {
					// clear out the vectors
					enemies.clear();
					torches.clear();
					keysVector.clear();
					doors.clear();
					swords.clear();

					// clear out vertex and texcoord data
					vertexData.clear();
					texCoordData.clear();

					currentLevel++;
					if (currentLevel == 2) {
						setupScene("level2.txt");
					}
					else if (currentLevel == 3) {
						setupScene("level3.txt");
					}

					// center camera on the player
					viewMatrix = glm::mat4(1.0f);
					viewMatrix = glm::scale(viewMatrix, glm::vec3(2.0f, 2.0f, 1.0f));
					viewMatrix = glm::translate(viewMatrix, -player.position);
					program.SetViewMatrix(viewMatrix);

					state = STATE_GAME;
				}
				if (keys[SDL_SCANCODE_ESCAPE]) {
					state = STATE_TITLE;
					int currentLevel = 1;
					int keyCount = 0;
				}
			}
			else if (state == STATE_GAMEOVER) {
				if (keys[SDL_SCANCODE_ESCAPE]) {
					state = STATE_TITLE;
				}
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);

		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		// GAME STATE
		switch (state) {
		case STATE_TITLE:
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			program.SetViewMatrix(glm::mat4(1.0f));

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.8f, 0.4f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Some Game", 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.95f, -0.3f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Press Space to Start", 0.1f, 0);

			break;

		case STATE_NEXT_LEVEL:
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			program.SetViewMatrix(glm::mat4(1.0f));

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.5f, 0.2f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Level " + to_string(currentLevel) + " Complete", 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.75f, -0.1f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Space : Continue", 0.1f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.215f, -0.4f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "ESC : Return to Title Screen", 0.09f, 0);

			break;

		case STATE_GAME:
			glClearColor(0.1412f, 0.0745f, 0.1020f, 1.0f);
			renderMap();

			if (currentMovementDelay <= 0 && swords.empty()) {
				if (keys[SDL_SCANCODE_LEFT]) {
					player.faceRight = false;
					if (!player.leftBlocked) {
						player.clearPositionData();
						player.position.x -= MAP_TILE_SIZE;
						player.setPositionData();

						for (unsigned i = 0; i < enemies.size(); i++) {
							enemies[i].Move(player.position);
						}
					}
					else {
						player.placeKey(DIRECTION_LEFT);
						if (!player.attack(DIRECTION_LEFT)) {
							Mix_PlayChannel(-1, hit_wall, 0);
							for (unsigned i = 0; i < enemies.size(); i++) {
								enemies[i].Move(player.position);
							}
						}
						else {
							Mix_PlayChannel(-1, swordSound, 0);
						}
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}
				else if (keys[SDL_SCANCODE_RIGHT]) {
					player.faceRight = true;
					if (!player.rightBlocked) {
						player.clearPositionData();
						player.position.x += MAP_TILE_SIZE;
						player.setPositionData();

						for (unsigned i = 0; i < enemies.size(); i++) {
							enemies[i].Move(player.position);
						}
					}
					else {
						player.placeKey(DIRECTION_RIGHT);
						if (!player.attack(DIRECTION_RIGHT)) {
							Mix_PlayChannel(-1, hit_wall, 0);
							for (unsigned i = 0; i < enemies.size(); i++) {
								enemies[i].Move(player.position);
							}
						}
						else {
							Mix_PlayChannel(-1, swordSound, 0);
						}
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}
				else if (keys[SDL_SCANCODE_DOWN]) {
					if (!player.downBlocked) {
						player.clearPositionData();
						player.position.y -= MAP_TILE_SIZE;
						player.setPositionData();

						for (unsigned i = 0; i < enemies.size(); i++) {
							enemies[i].Move(player.position);
						}
					}
					else {
						player.placeKey(DIRECTION_DOWN);
						if (!player.attack(DIRECTION_DOWN)) {
							Mix_PlayChannel(-1, hit_wall, 0);
							for (unsigned i = 0; i < enemies.size(); i++) {
								enemies[i].Move(player.position);
							}
						}
						else {
							Mix_PlayChannel(-1, swordSound, 0);
						}
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}
				else if (keys[SDL_SCANCODE_UP]) {
					if (!player.upBlocked) {
						player.clearPositionData();
						player.position.y += MAP_TILE_SIZE;
						player.setPositionData();

						for (unsigned i = 0; i < enemies.size(); i++) {
							enemies[i].Move(player.position);
						}
					}
					else {
						player.placeKey(DIRECTION_UP);
						if (!player.attack(DIRECTION_UP)) {
							Mix_PlayChannel(-1, hit_wall, 0);
							for (unsigned i = 0; i < enemies.size(); i++) {
								enemies[i].Move(player.position);
							}
						}
						else {
							Mix_PlayChannel(-1, swordSound, 0);
						}
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}

				// center camera on the player
				viewMatrix = glm::mat4(1.0f);
				viewMatrix = glm::scale(viewMatrix, glm::vec3(2.0f, 2.0f, 1.0f));
				viewMatrix = glm::translate(viewMatrix, -player.position);
				program.SetViewMatrix(viewMatrix);
			}

			// fixed update
			elapsed += accumulator;
			if (elapsed < FIXED_TIMESTEP) {
				accumulator = elapsed;
				continue;
			}
			while (elapsed >= FIXED_TIMESTEP) {
				player.Update(FIXED_TIMESTEP);

				currentMovementDelay -= FIXED_TIMESTEP;

				// animation
				animationElapsed += FIXED_TIMESTEP;
				if (animationElapsed > 1.0 / framesPerSecond) {
					currentIndex++;
					if (currentIndex > numFrames - 1) {
						currentIndex = 0;
					}

					// update all the sprites
					player.sprite.u = 0.25f * currentIndex;
					for (unsigned i = 0; i < enemies.size(); i++) {
						enemies[i].sprite.u = 0.25f * currentIndex;
						enemies[i].Update(FIXED_TIMESTEP);
					}
					for (unsigned i = 0; i < torches.size(); i++) {
						torches[i].sprite.u = 0.25f * currentIndex;
					}
					for (unsigned i = 0; i < keysVector.size(); i++) {
						keysVector[i].sprite.u = 0.25f * currentIndex;
					}
					for (unsigned i = 0; i < swords.size(); i++) {
						swords[i].timeRemaining--;
					}

					animationElapsed = 0.0;
				}

				elapsed -= FIXED_TIMESTEP;
			}
			accumulator = elapsed;

			// draw static entities
			for (Entity& torch : torches) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, torch.position);
				if (!torch.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				torch.Draw(program);
			}

			for (unsigned i = 0; i < keysVector.size(); i++) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, keysVector[i].position);
				program.SetModelMatrix(modelMatrix);
				keysVector[i].Draw(program);

				if (keysVector[i].collided(player)) {
					Mix_PlayChannel(-1, keySound, 0);
					keysVector.erase(keysVector.begin() + i);
					keyCount++;
					i--;
				}
				else {
					for (unsigned j = 0; j < doors.size(); j++) {
						if (keysVector[i].collided(doors[j])) {
							Mix_PlayChannel(-1, doorSound, 0);
							keysVector.erase(keysVector.begin() + i);
							doors[j].clearPositionData();
							doors.erase(doors.begin() + j);
							i--;
							break;
						}
					}
				}
			}

			for (Entity& door : doors) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, door.position);
				if (!door.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				door.Draw(program);
			}

			// draw exit
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, exitLadder.position);
			program.SetModelMatrix(modelMatrix);
			exitLadder.Draw(program);

			// draw player
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			if (!player.faceRight) {
				modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
			}
			program.SetModelMatrix(modelMatrix);
			player.Draw(program);

			// draw enemies
			for (Entity& enemy : enemies) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, enemy.position);
				if (!enemy.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				if (enemy.collided(player)) {
					state = STATE_GAMEOVER;
					fadeout = 0.0f;
					gameOverMessage = "You Died";
				}
				program.SetModelMatrix(modelMatrix);
				enemy.Draw(program);
			}

			for (unsigned i = 0; i < swords.size(); i++) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, swords[i].position);
				if (!swords[i].faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				swords[i].Draw(program);

				if (swords[i].timeRemaining == 0) {
					for (unsigned j = 0; j < enemies.size(); j++) {
						if (enemies[j].collided(swords[i])) {
							enemies[j].clearPositionData();
							enemies.erase(enemies.begin() + j);
							swords.erase(swords.begin() + i);
							i--;
							break;
						}
					}

					for (unsigned i = 0; i < enemies.size(); i++) {
						enemies[i].Move(player.position);
					}
				}
			}

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.85f, 0.45f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Keys:" + to_string(keyCount), 0.05f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(0.55f, 0.45f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Q:Quit", 0.05f, 0);

			if (player.collided(exitLadder)) {
				if (currentLevel == 3) {
					state = STATE_GAMEOVER;
					fadeout = 0.0f;
					gameOverMessage = "That's all the Levels";
				}
				else {
					state = STATE_NEXT_LEVEL;
				}
			}

			break;

		case STATE_GAMEOVER:
			glClearColor(0.1412f, 0.0745f, 0.1020f, 1.0f);
			renderMap();

			// center camera on the player
			viewMatrix = glm::mat4(1.0f);
			viewMatrix = glm::scale(viewMatrix, glm::vec3(2.0f, 2.0f, 1.0f));
			viewMatrix = glm::translate(viewMatrix, -player.position);
			program.SetViewMatrix(viewMatrix);

			// draw static entities
			for (Entity& torch : torches) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, torch.position);
				if (!torch.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				torch.Draw(program);
			}

			for (unsigned i = 0; i < keysVector.size(); i++) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, keysVector[i].position);
				program.SetModelMatrix(modelMatrix);
				keysVector[i].Draw(program);
			}

			for (Entity& door : doors) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, door.position);
				if (!door.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				door.Draw(program);
			}

			// draw exit
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, exitLadder.position);
			program.SetModelMatrix(modelMatrix);
			exitLadder.Draw(program);

			// draw player
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			if (!player.faceRight) {
				modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
			}
			program.SetModelMatrix(modelMatrix);
			player.Draw(program);

			// draw enemies
			for (Entity& enemy : enemies) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, enemy.position);
				if (!enemy.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				enemy.Draw(program);
			}

			float fadeOutVertices[] = { -1.777f, 1.0f, -1.777f, -1.0f, 1.777f, -1.0f, 
				-1.777f, 1.0f, 1.777f, -1.0f, 1.777f, 1.0f};

			if (fadeout < FADEOUT_TIME) {
				fadeout += elapsed;
			}
			fadeout = (fadeout < FADEOUT_TIME ? fadeout : FADEOUT_TIME);

			glUseProgram(untexturedProgram.programID);
			untexturedProgram.SetModelMatrix(glm::translate(glm::mat4(1.0f), player.position));
			untexturedProgram.SetProjectionMatrix(projectionMatrix);
			untexturedProgram.SetViewMatrix(viewMatrix);
			untexturedProgram.SetColor(0.0f, 0.0f, 0.0f, fadeout / FADEOUT_TIME);
			glVertexAttribPointer(untexturedProgram.positionAttribute, 2, GL_FLOAT, false, 0, fadeOutVertices);
			glEnableVertexAttribArray(untexturedProgram.positionAttribute);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glDisableVertexAttribArray(untexturedProgram.positionAttribute);

			glUseProgram(program.programID);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.4f, 0.1f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "GAME OVER", 0.1f, 0);

			modelMatrix = glm::mat4(1.0f);
			float fontXPos = -((gameOverMessage.size() - 1) * 0.05f) / 2;
			modelMatrix = glm::translate(modelMatrix, player.position);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(fontXPos, -0.1f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, gameOverMessage, 0.05f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.675f, -0.2f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "ESC : Return to Title Screen", 0.05f, 0);
		}


		SDL_GL_SwapWindow(displayWindow);
	}

	Mix_HaltMusic();
	Mix_FreeChunk(hit_wall);
	Mix_FreeChunk(keySound);
	Mix_FreeChunk(doorSound);
	Mix_FreeChunk(swordSound);
	Mix_FreeMusic(bgm);

	SDL_Quit();
	return 0;
}