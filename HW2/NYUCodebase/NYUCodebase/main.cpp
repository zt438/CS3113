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

SDL_Window* displayWindow;

static const float SPEED = 1.0f;
static const float OBJECT_WIDTH = (2.0f / 25);
static const float CENTER_SPACING = (2.0f / 15);
static const float leftPaddleX = -1.777f + 3 * OBJECT_WIDTH / 2;
static const float rightPaddleX = 1.777f - 3 * OBJECT_WIDTH / 2;

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("HW2 - Pong", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
    glewInit();
#endif

    SDL_Event event;
    bool done = false;
	bool inGame = true;

	glViewport(0, 0, 640, 360);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	ShaderProgram untextured_program;
	untextured_program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	glUseProgram(untextured_program.programID);

	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	glm::mat4 modelMatrix_rightPaddle = glm::mat4(1.0f);
	glm::mat4 modelMatrix_leftPaddle = glm::mat4(1.0f);
	glm::mat4 modelMatrix_ball = glm::mat4(1.0f);
	glm::mat4 modelMatrix_centerLine = glm::mat4(1.0f);

	untextured_program.SetProjectionMatrix(projectionMatrix);
	untextured_program.SetViewMatrix(viewMatrix);
	untextured_program.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    
	// center line vertices
	float centerLine_vertices[180];
	for (int i = 0; i < 15; i++) {
		centerLine_vertices[i * 12] = 0 - OBJECT_WIDTH / 2;
		centerLine_vertices[i * 12 + 1] = CENTER_SPACING * i - 1.0f + OBJECT_WIDTH / 3;
		centerLine_vertices[i * 12 + 2] = OBJECT_WIDTH / 2;
		centerLine_vertices[i * 12 + 3] = CENTER_SPACING * i - 1.0f + OBJECT_WIDTH / 3;
		centerLine_vertices[i * 12 + 4] = 0 - OBJECT_WIDTH / 2;
		centerLine_vertices[i * 12 + 5] = CENTER_SPACING * i + OBJECT_WIDTH - 1.0f + OBJECT_WIDTH / 3;

		centerLine_vertices[i * 12 + 6] = 0 - OBJECT_WIDTH / 2;
		centerLine_vertices[i * 12 + 7] = CENTER_SPACING * i + OBJECT_WIDTH - 1.0f + OBJECT_WIDTH / 3;
		centerLine_vertices[i * 12 + 8] = OBJECT_WIDTH / 2;
		centerLine_vertices[i * 12 + 9] = CENTER_SPACING * i - 1.0f + OBJECT_WIDTH / 3;
		centerLine_vertices[i * 12 + 10] = OBJECT_WIDTH / 2;
		centerLine_vertices[i * 12 + 11] = CENTER_SPACING * i + OBJECT_WIDTH - 1.0f + OBJECT_WIDTH / 3;
	}

	// ball vertices
	float ball_vertices[] = { -OBJECT_WIDTH / 2, -OBJECT_WIDTH / 2, OBJECT_WIDTH / 2, OBJECT_WIDTH / 2, -OBJECT_WIDTH / 2, OBJECT_WIDTH / 2,
							OBJECT_WIDTH / 2, OBJECT_WIDTH / 2, -OBJECT_WIDTH / 2, -OBJECT_WIDTH / 2, OBJECT_WIDTH / 2, -OBJECT_WIDTH / 2};

	// paddle vertices
	float paddle_vertices[] = { -OBJECT_WIDTH / 2, -OBJECT_WIDTH * 2, OBJECT_WIDTH / 2, OBJECT_WIDTH * 2, -OBJECT_WIDTH / 2, OBJECT_WIDTH * 2,
							OBJECT_WIDTH / 2, OBJECT_WIDTH * 2, -OBJECT_WIDTH / 2, -OBJECT_WIDTH * 2, OBJECT_WIDTH / 2, -OBJECT_WIDTH * 2 };

	modelMatrix_leftPaddle = glm::translate(modelMatrix_leftPaddle, glm::vec3(leftPaddleX, 0.0f, 0.0f));
	modelMatrix_rightPaddle = glm::translate(modelMatrix_rightPaddle, glm::vec3(rightPaddleX, 0.0f, 0.0f));

	float lastFrameTicks = 0.0f;

	// for movements
	float leftPaddleY = 0;
	float rightPaddleY = 0;
	float ballX = 0;
	float ballY = 0;
	float ballAngle = 45.0f;
	int lastRightMove = 0;
	int lastLeftMove = 0;

	while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }

			// press r to restart
			if (!inGame) {
				const Uint8 *keys = SDL_GetKeyboardState(NULL);
				if (keys[SDL_SCANCODE_R]) {
					leftPaddleY = 0;
					rightPaddleY = 0;
					ballX = 0;
					ballY = 0;
					ballAngle = 45.0f;
					inGame = true;
				}
			}
        }
        glClear(GL_COLOR_BUFFER_BIT);

		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		if (inGame) {
			// ball movement
			ballX += cos(ballAngle * (M_PI / 180.0f)) * elapsed * 1.3 * SPEED;
			ballY += sin(ballAngle * (M_PI / 180.0f)) * elapsed * 1.3 * SPEED;

			if (ballY + OBJECT_WIDTH / 2 > 1.0f) {
				ballAngle *= -1;
				ballY = 1.0f - OBJECT_WIDTH / 2;
			}
			else if (ballY - OBJECT_WIDTH / 2 < -1.0f) {
				ballAngle *= -1;
				ballY = -1.0f + OBJECT_WIDTH / 2;
			}

			// ball paddle collision
			float ball_leftPaddle_x = abs(ballX - leftPaddleX) - (OBJECT_WIDTH);
			float ball_leftPaddle_y = abs(ballY - leftPaddleY) - (OBJECT_WIDTH + OBJECT_WIDTH * 4) / 2;
			float ball_rightPaddle_x = abs(ballX - rightPaddleX) - (OBJECT_WIDTH);
			float ball_rightPaddle_y = abs(ballY - rightPaddleY) - (OBJECT_WIDTH + OBJECT_WIDTH * 4) / 2;

			if (ball_leftPaddle_x < 0 && ball_leftPaddle_y < 0) {
				// if the center of the ball already passed the center of the paddle, the paddle shouldnt be able to block it
				if (leftPaddleX <= ballX) {
					ballAngle += 2 * (90 - ballAngle);
					ballAngle += lastLeftMove * 5;
					ballX = leftPaddleX + OBJECT_WIDTH;
				}
			}
			else if (ball_rightPaddle_x < 0 && ball_rightPaddle_y < 0) {
				if (rightPaddleX >= ballX) {
					ballAngle += 2 * (90 - ballAngle);
					ballAngle -= lastRightMove * 5;
					ballX = rightPaddleX - OBJECT_WIDTH;
				}
			}

			// left paddle is computer controlled paddle
			if (ballY > leftPaddleY) {
				leftPaddleY += elapsed * SPEED;
				lastLeftMove = 1;
				if (leftPaddleY + OBJECT_WIDTH * 2 >= 1.0f) {
					leftPaddleY = 1.0f - OBJECT_WIDTH * 2;
				}
			}
			else if (ballY < leftPaddleY) {
				leftPaddleY -= elapsed * SPEED;
				lastLeftMove = -1;
				if (leftPaddleY - OBJECT_WIDTH * 2 <= -1.0f) {
					leftPaddleY = -1.0f + OBJECT_WIDTH * 2;
				}
			}
			else {
				lastLeftMove = 0;
			}

			// player paddle
			const Uint8 *keys = SDL_GetKeyboardState(NULL);
			if (keys[SDL_SCANCODE_UP]) {
				rightPaddleY += elapsed * SPEED;
				lastRightMove = 1;
				if (rightPaddleY + OBJECT_WIDTH * 2 >= 1.0f) {
					rightPaddleY = 1.0f - OBJECT_WIDTH * 2;
				}
			}
			else if (keys[SDL_SCANCODE_DOWN]) {
				rightPaddleY -= elapsed * SPEED;
				lastRightMove = -1;
				if (rightPaddleY - OBJECT_WIDTH * 2 <= -1.0f) {
					rightPaddleY = -1.0f + OBJECT_WIDTH * 2;
				}
			}
			else {
				lastRightMove = 0;
			}
		}

		// end game
		if (ballX + OBJECT_WIDTH / 2 > 1.777f || ballX - OBJECT_WIDTH / 2 < -1.777f) {
			inGame = false;
		}

		//==============================CENTER LINE==============================//
		untextured_program.SetModelMatrix(modelMatrix_centerLine);

		glVertexAttribPointer(untextured_program.positionAttribute, 2, GL_FLOAT, false, 0, centerLine_vertices);
		glEnableVertexAttribArray(untextured_program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 90);

		glDisableVertexAttribArray(untextured_program.positionAttribute);

		//=================================BALL=================================//
		untextured_program.SetModelMatrix(glm::translate(modelMatrix_ball, glm::vec3(ballX, ballY, 0.0f)));
		untextured_program.SetColor(1.0f, 0.533f, 0.0f, 1.0f);

		glVertexAttribPointer(untextured_program.positionAttribute, 2, GL_FLOAT, false, 0, ball_vertices);
		glEnableVertexAttribArray(untextured_program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(untextured_program.positionAttribute);

		//=============================LEFT PADDLE==============================//
		untextured_program.SetModelMatrix(glm::translate(modelMatrix_leftPaddle, glm::vec3(0.0f, leftPaddleY, 0.0f)));
		untextured_program.SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		glVertexAttribPointer(untextured_program.positionAttribute, 2, GL_FLOAT, false, 0, paddle_vertices);
		glEnableVertexAttribArray(untextured_program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(untextured_program.positionAttribute);

		//============================RIGHT PADDLE==============================//
		untextured_program.SetModelMatrix(glm::translate(modelMatrix_rightPaddle, glm::vec3(0.0f, rightPaddleY, 0.0f)));

		glVertexAttribPointer(untextured_program.positionAttribute, 2, GL_FLOAT, false, 0, paddle_vertices);
		glEnableVertexAttribArray(untextured_program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(untextured_program.positionAttribute);

        SDL_GL_SwapWindow(displayWindow);
    }
    
    SDL_Quit();
    return 0;
}
