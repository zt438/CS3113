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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(image);
	return retTexture;
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("Homework 1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
    glewInit();
#endif

    SDL_Event event;
    bool done = false;

	glViewport(0, 0, 640, 360);
	glClearColor(0.25f, 0.94f, 0.91f, 1.0f);

	// untextured polygon shader
	ShaderProgram untextured_program;
	untextured_program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

	// textured polygon shader
	ShaderProgram textured_program;
	textured_program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix_hourglass = glm::mat4(1.0f);
	glm::mat4 modelMatrix_plane = glm::mat4(1.0f);
	glm::mat4 modelMatrix_sun = glm::mat4(1.0f);
	glm::mat4 modelMatrix_moon = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	float lastFrameTicks = 0.0f;

	// hourglass shape formed by two triangles
	float vertices[] = { 0.0f, -0.0f, 0.3f, 0.5f, -0.3f, 0.5f, 
						0.0f, -0.0f, -0.3f, -0.5f, 0.3f, -0.5f };

	// plane vertices
	float plane_vertices[] = { -0.1, 0.4, 0.1, 0.4, 0.1, 0.5,
							-0.1, 0.4, 0.1, 0.5, -0.1, 0.5 };
	float plane_texCoords[] = {0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 
						0.0, 1.0, 1.0, 0.0, 0.0, 0.0};

	// sun vertices
	float sun_vertices[] = { 1.377, 0.6, 1.777, 0.6, 1.777, 1.0,
							1.377, 0.6, 1.777, 1.0, 1.377, 1.0 };
	float sun_texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0,
						0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };

	// moon vertices
	float moon_vertices[] = { -1.377, -0.6, -1.777, -0.6, -1.777, -1.0,
							-1.377, -0.6, -1.777, -1.0, -1.377, -1.0 };
	float moon_texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0,
						0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };

	// load the textures
	GLuint planeTexture = LoadTexture(RESOURCE_FOLDER"plane.png");
	GLuint sunTexture = LoadTexture(RESOURCE_FOLDER"sun.png");
	GLuint moonTexture = LoadTexture(RESOURCE_FOLDER"moon.png");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);


    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);
		// calculated elapsed
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		// calculate rotation angle for hourglass
		float hourglass_angle = elapsed * 30.0f * (M_PI / 180.0f);
		// plane angle
		float plane_angle = elapsed * -30.0f * (M_PI / 180.0f);

		//========================MOON===========================//
		glUseProgram(textured_program.programID);
		textured_program.SetModelMatrix(modelMatrix_moon);
		textured_program.SetProjectionMatrix(projectionMatrix);
		textured_program.SetViewMatrix(viewMatrix);

		glBindTexture(GL_TEXTURE_2D, moonTexture);

		glVertexAttribPointer(textured_program.positionAttribute, 2, GL_FLOAT, false, 0, moon_vertices);
		glEnableVertexAttribArray(textured_program.positionAttribute);
		glVertexAttribPointer(textured_program.texCoordAttribute, 2, GL_FLOAT, false, 0, moon_texCoords);
		glEnableVertexAttribArray(textured_program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(textured_program.positionAttribute);
		glDisableVertexAttribArray(textured_program.texCoordAttribute);

		//========================SUN===========================//
		glUseProgram(textured_program.programID);
		textured_program.SetModelMatrix(modelMatrix_sun);
		textured_program.SetProjectionMatrix(projectionMatrix);
		textured_program.SetViewMatrix(viewMatrix);

		glBindTexture(GL_TEXTURE_2D, sunTexture);

		glVertexAttribPointer(textured_program.positionAttribute, 2, GL_FLOAT, false, 0, sun_vertices);
		glEnableVertexAttribArray(textured_program.positionAttribute);
		glVertexAttribPointer(textured_program.texCoordAttribute, 2, GL_FLOAT, false, 0, sun_texCoords);
		glEnableVertexAttribArray(textured_program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(textured_program.positionAttribute);
		glDisableVertexAttribArray(textured_program.texCoordAttribute);

		//=====================PLANE===========================//
		// want plane behind the hourglass so drawing it first
		modelMatrix_plane = glm::rotate(modelMatrix_plane, plane_angle, glm::vec3(0.0f, 0.0f, 1.0f));

		glUseProgram(textured_program.programID);
		textured_program.SetModelMatrix(modelMatrix_plane);
		textured_program.SetProjectionMatrix(projectionMatrix);
		textured_program.SetViewMatrix(viewMatrix);

		glBindTexture(GL_TEXTURE_2D, planeTexture);

		glVertexAttribPointer(textured_program.positionAttribute, 2, GL_FLOAT, false, 0, plane_vertices);
		glEnableVertexAttribArray(textured_program.positionAttribute);
		glVertexAttribPointer(textured_program.texCoordAttribute, 2, GL_FLOAT, false, 0, plane_texCoords);
		glEnableVertexAttribArray(textured_program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(textured_program.positionAttribute);
		glDisableVertexAttribArray(textured_program.texCoordAttribute);

		//===========================HOURGLASS============================//
		modelMatrix_hourglass = glm::rotate(modelMatrix_hourglass, hourglass_angle, glm::vec3(0.0f, 0.0f, 1.0f));

		glUseProgram(untextured_program.programID);

		untextured_program.SetModelMatrix(modelMatrix_hourglass);
		untextured_program.SetProjectionMatrix(projectionMatrix);
		untextured_program.SetViewMatrix(viewMatrix);

		// set color for the hourglass
		untextured_program.SetColor(1.0f, 0.0f, 0.0f, 0.5f);

		// hourglass
		glVertexAttribPointer(untextured_program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(untextured_program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(untextured_program.positionAttribute);

        SDL_GL_SwapWindow(displayWindow);
    }
    
    SDL_Quit();
    return 0;
}
