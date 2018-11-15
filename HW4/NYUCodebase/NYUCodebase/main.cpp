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

#define ENTITY_SPRITE_COUNT_X 8
#define ENTITY_SPRITE_COUNT_Y 4

#define MAP_TILE_SIZE 0.2f
#define LEVEL_WIDTH 30
#define LEVEL_HEIGHT 45
#define MAP_SPRITE_COUNT_X 24
#define MAP_SPRITE_COUNT_Y 16
enum GameState { STATE_TITLE, STATE_GAME, STATE_GAMEOVER };
enum EntityType { ENTITY_PLAYER, ENTITY_KING };

GameState state;

ShaderProgram program;
glm::mat4 modelMatrix;
glm::mat4 viewMatrix;

// for animation
const int playerRunAnimation[] = { 17, 18, 19, 20, 21 };
const int numFrames = 5;
float animationElapsed = 0.0f;
float framesPerSecond = 30.0f;
int currentIndex = 0;

int mapWidth;
int mapHeight;
short **levelData;

GLuint font;
GLuint entitySpriteSheet;
GLuint mapSpriteSheet;
string gameOverMessage = "";

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

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
	Entity(glm::vec3 position, glm::vec3 velocity, glm::vec3 acceleration, bool isStatic, EntityType type) {
		this->position = position;
		this->size = glm::vec3(0.2f, 0.4f, 1.0f);
		this->velocity = velocity;
		this->acceleration = acceleration;
		this->isStatic = isStatic;
		this->entityType = type;
	}

	void Draw(ShaderProgram &program) {
		sprite.Draw(program);
	}

	void Update(float elapsed) {
		if (entityType == ENTITY_KING) {
			// king jumps
			if (collidedBottom) {
				velocity.y = 1.5;
			}
		}

		int tileX, tileY;
		worldToTileCoordinates(position.x, position.y - size.y / 2, tileX, tileY);

		// bound check for player
		if (entityType == ENTITY_PLAYER) {
			// player dies if out of bound
			if (tileY >= LEVEL_HEIGHT - 1) {
				state = STATE_GAMEOVER;
				gameOverMessage = "You Fell Out of Bounds";
				return;
			}
		}

		// apply gravity if not touching the ground
		acceleration.y = (collidedBottom ? 0.0f : -3.0f);

		// horizontal movement
		velocity.x += acceleration.x * elapsed;
		// limit maximum speed
		if (velocity.x > 2.0f) {
			velocity.x = 2.0f;
		}
		// friction
		velocity.x = lerp(velocity.x, 0.0f, elapsed * friction.x);
		position.x += velocity.x * elapsed;

		velocity.y += acceleration.y * elapsed;
		// friction
		velocity.y = lerp(velocity.y, 0.0f, elapsed * friction.y);
		position.y += velocity.y * elapsed;

		tileCollision();
	}

	bool collided(Entity& other) {
		float player_other_x = abs(position.x - other.position.x) - size.x;
		float player_other_y = abs(position.y - other.position.y) - size.y;
		return (player_other_x < 0 && player_other_y < 0);
	}

	SheetSprite sprite;

	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 size;
	glm::vec3 acceleration;

	bool isStatic;
	EntityType entityType;

	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;
	bool faceRight = true;

private:
	glm::vec2 friction = glm::vec2(2.0f, 0.8f);

	void worldToTileCoordinates(float worldX, float worldY, int& gridX, int& gridY) {
		gridX = (int)(worldX / MAP_TILE_SIZE);
		gridY = (int)(worldY / -MAP_TILE_SIZE);
	}

	bool isSolid(int tileIndex) {
		return (tileIndex == 4 || tileIndex == 5 || tileIndex == 48 || tileIndex == 75);
	}

	void tileCollision() {
		int tileX, tileY, tileLeft, tileRight, tileUp, tileDown;
		float penetration = 0;
		// right side
		worldToTileCoordinates(position.x + size.x / 2.0f, position.y, tileRight, tileY);
		if (tileRight >= LEVEL_WIDTH - 1 || isSolid(levelData[tileY][tileRight])) {
			collidedRight = true;
			penetration = (MAP_TILE_SIZE * tileRight) - (position.x + size.x / 2.0f);
			position.x += (penetration + 0.001f);
			// stops the player movement but allows player to continue moving in the other direction
			if (velocity.x > 0) {
				velocity.x = 0.0f;
			}
		}
		else {
			collidedRight = false;
		}
		// left side
		worldToTileCoordinates(position.x - size.x / 2.0f, position.y, tileLeft, tileY);
		if (tileLeft <= 0 || isSolid(levelData[tileY][tileLeft])) {
			collidedLeft = true;
			penetration = (position.x - size.x / 2.0f) - (MAP_TILE_SIZE * (tileLeft + 1));
			position.x -= (penetration + 0.001f);
			// stops the player movement but allows player to continue moving in the other direction
			if (velocity.x < 0) {
				velocity.x = 0.0f;
			}
		}
		else {
			collidedLeft = false;
		}
		// bottom
		worldToTileCoordinates(position.x, position.y - size.y / 2.0f, tileX, tileDown);
		if (isSolid(levelData[tileDown][tileX])) {
			collidedBottom = true;
			velocity.y = 0.0f;
			penetration = (-MAP_TILE_SIZE * tileDown) - (position.y - size.y / 2.0f);
			position.y += penetration + 0.001f;
		}
		else {
			collidedBottom = false;
		}
		// top
		worldToTileCoordinates(position.x, position.y + size.y / 4.0f, tileX, tileUp);
		if (isSolid(levelData[tileUp][tileX])) {
			collidedTop = true;
			velocity.y = 0.0f;
			penetration = (position.y + size.y / 4.0f) - (-MAP_TILE_SIZE * (tileUp + 1));
			position.y -= (penetration + 0.001f);
		}
		else {
			collidedTop = false;
		}
	}
};

Entity player;
Entity king;

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
		player = Entity(glm::vec3(x, y + 0.1f, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), false, ENTITY_PLAYER);
		player.sprite = SheetSprite(entitySpriteSheet, 0.0f, 0.5f, 0.125f, 0.25f, 0.40f);
	}
	else if (type == "King") {
		king = Entity(glm::vec3(x + 0.1f, y, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), false, ENTITY_KING);
		king.sprite = SheetSprite(entitySpriteSheet, 0.0f, 0.25f, 0.125f, 0.25f, 0.40f);
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

			float placeX = atoi(xPosition.c_str())*MAP_TILE_SIZE;
			float placeY = atoi(yPosition.c_str())*-MAP_TILE_SIZE;
			placeEntity(type, placeX, placeY);
		}
	}
	return true;
}

void setupScene() {
	ifstream infile("map.txt");
	string line;
	while (getline(infile, line)) {
		if (line == "[header]") {
			if (!readHeader(infile))
				return;
		}
		else if (line == "[layer]") {
			readLayerData(infile);
		}
		else if (line == "[Object Layer 1]") {
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
	entitySpriteSheet = LoadTexture(RESOURCE_FOLDER"characters_3.png");
	mapSpriteSheet = LoadTexture(RESOURCE_FOLDER"dirt-tiles.png");
	
	state = STATE_TITLE;

	float lastFrameTicks = 0.0f;

	glViewport(0, 0, 640, 360);
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	projectionMatrix = glm::ortho(-3.556f, 3.556f, -2.0f, 2.0f, -1.0f, 1.0f);

	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);
	glUseProgram(program.programID);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
					setupScene();

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
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.7f, 1.0f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "SOME PLATFORMER", 0.4f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.8f, 0.7f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Reach the King", 0.3f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-3.0f, 0.0f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "MOVEMENT: Left/Right Arrow Keys", 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.3f, -0.2f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "JUMP: Spacebar", 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, -0.6f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Press Space to Start", 0.2f, 0);

			break;

		case STATE_GAME:
			glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
			renderMap();

			if (keys[SDL_SCANCODE_LEFT]) {
				player.faceRight = false;
				player.acceleration.x = -3.0f;
			}
			else if (keys[SDL_SCANCODE_RIGHT]) {
				player.faceRight = true;
				player.acceleration.x = 3.0f;
			}
			else {
				player.acceleration.x = 0.0f;
			}
			if (keys[SDL_SCANCODE_SPACE]) {
				if (keys[SDL_SCANCODE_SPACE]) {
					if (player.collidedBottom) {
						player.velocity.y = 1.9f;
					}
				}
			}

			// win condition
			if (player.collided(king)) {
				state = STATE_GAMEOVER;
				gameOverMessage = "You Reached the King";
			}

			// fixed update
			elapsed += accumulator;
			if (elapsed < FIXED_TIMESTEP) {
				accumulator = elapsed;
				continue;
			}
			while (elapsed >= FIXED_TIMESTEP) {
				player.Update(FIXED_TIMESTEP);
				king.Update(FIXED_TIMESTEP);

				// animation
				animationElapsed += FIXED_TIMESTEP;
				if (animationElapsed > 1.0 / framesPerSecond) {
					// walking animation
					if (player.acceleration.x != 0.0f) {
						currentIndex++;
						if (currentIndex > numFrames - 1) {
							currentIndex = 0;
						}
					}
					// just standing
					else {
						currentIndex = 4;
					}
					player.sprite.u = (float)((playerRunAnimation[currentIndex]) % ENTITY_SPRITE_COUNT_X) / (float)ENTITY_SPRITE_COUNT_X;
					player.sprite.v = (float)((playerRunAnimation[currentIndex]) / ENTITY_SPRITE_COUNT_X) / (float)ENTITY_SPRITE_COUNT_Y;
					animationElapsed = 0.0;
				}

				elapsed -= FIXED_TIMESTEP;
			}
			accumulator = elapsed;

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, king.position);
			modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
			program.SetModelMatrix(modelMatrix);
			king.Draw(program);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			if (!player.faceRight) {
				modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
			}
			program.SetModelMatrix(modelMatrix);
			player.Draw(program);

			// center camera on the player
			viewMatrix = glm::mat4(1.0f);
			viewMatrix = glm::scale(viewMatrix, glm::vec3(2.0f, 2.0f, 1.0f));
			viewMatrix = glm::translate(viewMatrix, -player.position);
			program.SetViewMatrix(viewMatrix);

			break;

		case STATE_GAMEOVER:
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			program.SetViewMatrix(glm::mat4(1.0f));

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.7f, 0.8f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "GAME OVER", 0.4f, 0);

			modelMatrix = glm::mat4(1.0f);
			float fontXPos = - ((gameOverMessage.size() - 1) * 0.3f) / 2;
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
