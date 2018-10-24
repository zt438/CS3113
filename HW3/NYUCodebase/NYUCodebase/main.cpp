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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

SDL_Window* displayWindow;
enum GameState {STATE_TITLE, STATE_GAME, STATE_GAMEOVER};

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

		float aspect = 1.0f; // the sprite sheet is not a square so using width/height messes up the ratio
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
	Entity(glm::vec3 position, glm::vec3 size, glm::vec3 velocity, bool friendly, bool canShoot) {
		this->position = position;
		this->size = size;
		this->velocity = velocity;
		this->friendly = friendly;
		this->canShoot = canShoot;
		if (friendly) {
			this->health = 3;
			if (canShoot) {
				this->shootDelay = 20;
			}
		}
		else {
			this->health = 1;
			if (canShoot) {
				this->shootDelay = rand() % 80 + 80;
			}
		}

		if (!canShoot) {
			this->health = 100;
		}
	}

	void Draw(ShaderProgram &program) {
		if (health > 0) {
			sprite.Draw(program);
		}
	}

	void Update(float elapsed) {
		position.x += velocity.x * elapsed;
		position.y += velocity.y * elapsed;

		shootDelay -= elapsed;
	}

	bool collided(Entity& other) {
		// cant collide with entities on the same side
		if (friendly == other.friendly) {
			return false;
		}

		float distance_sq = pow(position.x - other.position.x, 2) + pow(position.y - other.position.y, 2);
		float radii_sum_sq = pow(size.x + other.size.x, 2);
		return (distance_sq < radii_sum_sq);
	}

	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 size;

	float rotation;
	SheetSprite sprite;
	
	int shootDelay;
	int health;
	bool friendly;
	bool canShoot;
};

Entity player;
std::vector<Entity> enemies;
std::vector<Entity> projectiles;

SheetSprite playerSprite;
SheetSprite enemySprite;
SheetSprite playerProjectile;
SheetSprite enemyProjectile;

const glm::vec3 emoteSize = glm::vec3(0.1f, 0.1f, 0.0f);
const glm::vec3 projectileSize = glm::vec3(0.05f, 0.05f, 0.0f);

const int MAX_BULLETS = 100;

int projectileIndex; // for iterating through projectiles later
bool isFirst; // for determining if enemy is first is column
int score;

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
    glewInit();
#endif

	GameState state = STATE_TITLE;
	GLuint font = LoadTexture(RESOURCE_FOLDER"font1.png");
	GLuint spriteSheet = LoadTexture(RESOURCE_FOLDER"sprites.png");

	playerSprite = SheetSprite(spriteSheet, 0.0f, 0.0f, 18.0f / 64.0f, 18.0f / 32.0f, 0.20f);
	enemySprite = SheetSprite(spriteSheet, 20.0f / 64.0f, 0.0f, 18.0f / 64.0f, 18.0f / 32.0f, 0.20f);
	playerProjectile = SheetSprite(spriteSheet, 0.0f, 20.0f / 32.0f, 9.0f / 64.0f, 9.0f / 32.0f, 0.1f);
	enemyProjectile = SheetSprite(spriteSheet, 11.0f / 64.0f, 20.0f / 32.0f, 9.0f / 64.0f, 9.0f / 32.0f, 0.1f);

	float lastFrameTicks = 0.0f;

	glViewport(0, 0, 640, 360);
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	projectionMatrix = glm::ortho(-3.556f, 3.556f, -2.0f, 2.0f, -1.0f, 1.0f);

	ShaderProgram program;
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
					// set up the game
					score = 0;

					player = Entity(glm::vec3(0.0f, -1.6f, 0.0f), 
									emoteSize,
									glm::vec3(0.0f, 0.0f, 0.0f), 
									true, true);
					player.sprite = playerSprite;

					float spacing_x = 7.111f / 7.0f;
					float spacing_y = 2.0f / 5.0f + 0.1f;
					for (int i = 0; i < 4; i++) {
						for (int j = 0; j < 6; j++) {
							enemies.push_back(Entity(glm::vec3(spacing_x * (j + 1) - 3.556, spacing_y * i, 0.0f), 
													emoteSize,
													glm::vec3(0.4f, 0.0f, 0.0f), 
													false, true));
							// should be last element in enemies vector
							enemies[enemies.size() - 1].sprite = enemySprite;
						}
					}

					state = STATE_GAME;
				}
			}
			else if (state == STATE_GAMEOVER) {
				if (keys[SDL_SCANCODE_ESCAPE]) {
					// clear data
					enemies.clear();
					projectiles.clear();

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
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.5f, 1.0f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "SPACE INVADERS", 0.4f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-3.0f, 0.4f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "MOVEMENT: Left/Right Arrow Keys", 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, 0.2f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "SHOOT: Up Arrow Key", 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, -0.2f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "Press Space to Start", 0.2f, 0);

			break;

		case STATE_GAME:
			if (keys[SDL_SCANCODE_LEFT]) {
				player.velocity.x = -1.0f;
			}
			else if (keys[SDL_SCANCODE_RIGHT]) {
				player.velocity.x = 1.0f;
			}
			else {
				player.velocity.x = 0.0f;
			}
			if (keys[SDL_SCANCODE_UP]) {
				if (player.shootDelay <= 0) {
					if (projectiles.size() > MAX_BULLETS) {
						projectiles.erase(projectiles.begin());
					}
					projectiles.push_back(Entity(player.position, projectileSize,
						glm::vec3(0.0f, 3.0f, 0.0f), true, false));
					projectiles[projectiles.size() - 1].sprite = playerProjectile;
					player.shootDelay = 20;
				}
			}

			//================================================//
			//======================STAT======================//
			//================================================//
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(2.8f, 1.9f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "HP:" + std::to_string(player.health), 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-3.5f, 1.9f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "SCORE:" + std::to_string(score), 0.2f, 0);


			//================================================//
			//=====================PLAYER=====================//
			//================================================//
			player.Update(elapsed);
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, player.position);
			program.SetModelMatrix(modelMatrix);
			player.Draw(program);


			//================================================//
			//====================ENEMIES=====================//
			//================================================//
			for (int i = 0; i < enemies.size(); i++) {
				if (enemies[i].health > 0) {
					// check collision with wall
					if (enemies[i].position.x + emoteSize.x / 2 >= 3.5f) {
						float penetration = fabs(fabs(enemies[i].position.x - 3.5f) - emoteSize.x - 0.05f);
						for (Entity& enemy : enemies) {
							enemy.position.x -= penetration - 0.005f;
							enemy.velocity.x *= -1;
							enemy.position.y -= 0.1f;
						}
					}
					if (enemies[i].position.x - emoteSize.x / 2 <= -3.5f) {
						float penetration = fabs(fabs(enemies[i].position.x + 3.5f) - emoteSize.x - 0.05f);
						for (Entity& enemy : enemies) {
							enemy.position.x += penetration + 0.005f;
							enemy.velocity.x -= -1;
							enemy.position.y -= 0.1f;
						}
					}
					if (enemies[i].position.y < player.position.y - emoteSize.y / 2) {
						player.health = 0;
					}

					enemies[i].Update(elapsed);

					modelMatrix = glm::mat4(1.0f);
					modelMatrix = glm::translate(modelMatrix, enemies[i].position);
					program.SetModelMatrix(modelMatrix);
					enemies[i].Draw(program);

					// check collision with player
					if (enemies[i].collided(player)) {
						player.health--;
						enemies[i].health--;
						score++;
					}

					// shoot
					// check if theres someone in front
					isFirst = true;
					for (int prev = i - 6; prev > 0; prev -= 6) {
						if (enemies[prev].health > 0) {
							isFirst = false;
							break;
						}
					}
					// shoot if no one in front
					if (isFirst) {
						if (enemies[i].shootDelay <= 0) {
							if (projectiles.size() > MAX_BULLETS) {
								projectiles.erase(projectiles.begin());
							}
							projectiles.push_back(Entity(enemies[i].position, projectileSize,
												glm::vec3(0.0f, -3.0f, 0.0f), false, false));
							projectiles[projectiles.size() - 1].sprite = enemyProjectile;
							enemies[i].shootDelay = rand() % 80 + 80;
						}
					}
				}
			}


			//================================================//
			//==================PROJECTILES===================//
			//================================================//
			projectileIndex = 0;
			while (projectileIndex < projectiles.size()) {
				projectiles[projectileIndex].Update(elapsed);
				// using health as timer
				projectiles[projectileIndex].health -= 1.0f * elapsed;

				modelMatrix = glm::mat4(1.0f);
				modelMatrix = glm::translate(modelMatrix, projectiles[projectileIndex].position);
				program.SetModelMatrix(modelMatrix);
				projectiles[projectileIndex].Draw(program);


				// check collision with player
				if (projectiles[projectileIndex].collided(player)) {
					player.health--;
					projectiles[projectileIndex].health = 0;
				}

				// check collision with enemies
				for (int j = 0; j < enemies.size(); j++) {
					if (enemies[j].health > 0) {
						if (enemies[j].collided(projectiles[projectileIndex])) {
							enemies[j].health--;
							projectiles[projectileIndex].health = 0;
							score++;
						}
					}
				}

				// erase expired projectiles
				if (projectiles[projectileIndex].health <= 0) {
					projectiles.erase(projectiles.begin() + projectileIndex);
					projectileIndex--;
				}

				projectileIndex++;
			}

			// max score is 24
			if (player.health == 0 || score >= 24) {
				state = STATE_GAMEOVER;
			}

			break;

		case STATE_GAMEOVER:
			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(2.8f, 1.9f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "HP:" + std::to_string(player.health), 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-3.5f, 1.9f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "SCORE:" + std::to_string(score), 0.2f, 0);

			modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.7f, 0.8f, 0.0f));
			program.SetModelMatrix(modelMatrix);
			DrawText(program, font, "GAME OVER", 0.4f, 0);

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
