#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>

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
using namespace std;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define FIXED_TIMESTEP 0.01666666f
#define MAX_TIMESTEPS 6
float accumulator = 0.0f;

#define MAP_TILE_SIZE 0.1f
#define LEVEL_WIDTH 20
#define LEVEL_HEIGHT 10
#define MAP_SPRITE_COUNT_X 10
#define MAP_SPRITE_COUNT_Y 10

#define MOVEMENT_DELAY 0.2f

enum GameState { STATE_TITLE, STATE_GAME, STATE_GAMEOVER };
enum EntityType { ENTITY_PLAYER, ENTITY_SKULL, ENTITY_TORCH, ENTITY_SIDE_TORCH, ENTITY_DOOR, ENTITY_KEY, ENTITY_EXIT };
enum EntityState { STATE_IDLE, STATE_CHASE };

GameState state;

ShaderProgram program;
glm::mat4 modelMatrix;
glm::mat4 viewMatrix;

// for animation
const int animationFrames[] = { 0, 1, 2, 3 };
const int numFrames = 4;
float animationElapsed = 0.0f;
float framesPerSecond = 4.0f;
int currentIndex = 0;

int mapWidth;
int mapHeight;
short **levelData;

GLuint font;
GLuint playerSpriteSheet;
GLuint skullSpriteSheet;
GLuint torchSpriteSheet;
GLuint sideTorchSpriteSheet;
GLuint keySpriteSheet;
GLuint mapSpriteSheet;
string gameOverMessage = "";

SDL_Window* displayWindow;

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

	for (int i = 0; i < text.size(); i++) {
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

			if (isSolid(levelData[tileY][tileX + 1])) {
				rightBlocked = true;
			}
			else {
				rightBlocked = false;
			}
			if (isSolid(levelData[tileY][tileX - 1])) {
				leftBlocked = true;
			}
			else {
				leftBlocked = false;
			}
			if (isSolid(levelData[tileY - 1][tileX])) {
				upBlocked = true;
			}
			else {
				upBlocked = false;
			}
			if (isSolid(levelData[tileY + 1][tileX])) {
				downBlocked = true;
			}
			else {
				downBlocked = false;
			}
		}
	}

	// basic movement AI for non static enemy only
	void Move(glm::vec3 playerPosition) {
		if (currentState == STATE_IDLE) {
			// pick a random nonblocked direction and move that way
			vector<string> potentialDirections;
			if (!leftBlocked) potentialDirections.push_back("left");
			if (!rightBlocked) potentialDirections.push_back("right");
			if (!upBlocked) potentialDirections.push_back("up");
			if (!downBlocked) potentialDirections.push_back("down");

			int randomMove = rand() % potentialDirections.size();

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
		}
		else if (currentState == STATE_CHASE) {
			// move towards the player
		}
	}

	bool collided(Entity& other) {
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
	
	EntityState currentState = STATE_IDLE;

private:
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
};

Entity player;
vector<Entity> enemies;
vector<Entity> torches;
vector<Entity> keysVector;
vector<Entity> doors;
Entity exitLadder;

vector<float> vertexData;
vector<float> texCoordData;
void drawMap() {
	for (int y = 0; y < LEVEL_HEIGHT; y++) {
		for (int x = 0; x < LEVEL_WIDTH; x++) {
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
		for (int i = 0; i < mapHeight; ++i) {
			levelData[i] = new short[mapWidth];
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
		doors[doors.size() - 1].sprite = SheetSprite(mapSpriteSheet, 0.7f, 0.4f, 0.1f, 0.1f, 0.10f);
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

	font = LoadTexture(RESOURCE_FOLDER"font1.png");
	playerSpriteSheet = LoadTexture(RESOURCE_FOLDER"priest2_framesheet.png");
	skullSpriteSheet = LoadTexture(RESOURCE_FOLDER"skull_framesheet.png");
	torchSpriteSheet = LoadTexture(RESOURCE_FOLDER"torch_framesheet.png");
	sideTorchSpriteSheet = LoadTexture(RESOURCE_FOLDER"side_torch_framesheet.png");
	keySpriteSheet = LoadTexture(RESOURCE_FOLDER"key_framesheet.png");
	mapSpriteSheet = LoadTexture(RESOURCE_FOLDER"Dungeon_Tileset.png");

	state = STATE_TITLE;

	float lastFrameTicks = 0.0f;

	glViewport(0, 0, 640, 360);
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);

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
			if (state == STATE_TITLE) {
				if (keys[SDL_SCANCODE_SPACE]) {
					setupScene("level1.txt");

					state = STATE_GAME;
				}
			}
			else if (state == STATE_GAME) {
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
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.0f, -0.3f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Press Space to Start", 0.1f, 0);

			break;

		case STATE_GAME:
			glClearColor(0.1412f, 0.0745f, 0.1020f, 1.0f);
			renderMap();

			if (currentMovementDelay <= 0) {
				if (keys[SDL_SCANCODE_LEFT]) {
					player.faceRight = false;
					if (!player.leftBlocked) {
						player.position.x -= MAP_TILE_SIZE;
					}
					
					for (int i = 0; i < enemies.size(); i++) {
						enemies[i].Move(player.position);
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}
				else if (keys[SDL_SCANCODE_RIGHT]) {
					player.faceRight = true;
					if (!player.rightBlocked) {
						player.position.x += MAP_TILE_SIZE;
					}

					for (int i = 0; i < enemies.size(); i++) {
						enemies[i].Move(player.position);
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}
				else if (keys[SDL_SCANCODE_DOWN]) {
					if (!player.downBlocked) {
						player.position.y -= MAP_TILE_SIZE;
					}

					for (int i = 0; i < enemies.size(); i++) {
						enemies[i].Move(player.position);
					}
					currentMovementDelay = MOVEMENT_DELAY;
				}
				else if (keys[SDL_SCANCODE_UP]) {
					if (!player.upBlocked) {
						player.position.y += MAP_TILE_SIZE;
					}

					for (int i = 0; i < enemies.size(); i++) {
						enemies[i].Move(player.position);
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
					for (int i = 0; i < enemies.size(); i++) {
						enemies[i].sprite.u = 0.25f * currentIndex;
						enemies[i].Update(FIXED_TIMESTEP);
					}
					for (int i = 0; i < torches.size(); i++) {
						torches[i].sprite.u = 0.25f * currentIndex;
					}
					for (int i = 0; i < keysVector.size(); i++) {
						keysVector[i].sprite.u = 0.25f * currentIndex;
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

			for (Entity& doorKey : keysVector) {
				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, doorKey.position);
				if (!doorKey.faceRight) {
					modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
				}
				program.SetModelMatrix(modelMatrix);
				doorKey.Draw(program);
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

			break;

		case STATE_GAMEOVER:
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			program.SetViewMatrix(glm::mat4(1.0f));

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.7f, 0.8f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "GAME OVER", 0.4f, 0);

			modelMatrix = glm::mat4(1.0f);
			float fontXPos = -((gameOverMessage.size() - 1) * 0.3f) / 2;
			modelMatrix = glm::translate(modelMatrix, glm::vec3(fontXPos, 0.3f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, gameOverMessage, 0.3f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-3.4f, -0.2f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Press ESC to Return to Title Screen", 0.2f, 0);
		}


		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}