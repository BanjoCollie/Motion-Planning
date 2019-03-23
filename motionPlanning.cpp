// Includes and defs --------------------------

// openGL functionality
#include <glad/glad.h>
#include <glad/glad.c>
#include <GLFW/glfw3.h>
// shader helper
#include <learn_opengl/shader.h>
// math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <time.h>
// image loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <vector>

// Functions ---------------------------------

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

// Global variables ---------------------------

// window
const int SCR_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// time
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame

// Controls
bool moveAgent = false;
bool showConfig = false;
bool showPoints = false;
bool showEdges = false;
bool showPath = false;


int main()
{
	// Before loop starts ---------------------
	// glfw init
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// glfw window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "MotionPlanning", NULL, NULL);
	glfwMakeContextCurrent(window);

	// register callbacks
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Initialize glad
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	// Enable openGL settings
	//glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	// Setup ----------------------------------

	/* initialize random seed: */
	srand(time(NULL));

	Shader myShader("solidColor.vert", "solidColor.frag");

	// Grid
	unsigned int gridVAO, gridVBO, gridEBO;
	glGenVertexArrays(1, &gridVAO);
	glBindVertexArray(gridVAO);

	glm::vec3 gridPoints[20][20];
	for (int i = 0; i < 20; i++)
	{
		for (int j = 0; j < 20; j++)
		{
			gridPoints[i][j] = glm::vec3((float)i - 10.0f, (float)j - 10.0f, 0.99f);
		}
	}

	glGenBuffers(1, &gridVBO);
	glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(gridPoints), gridPoints, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(0);

	// Circles
	const float PI = 3.14159265;
	const int numPoints = 20;
	
	glm::vec3 circlePoints[numPoints+1];
	for (int i = 0; i < numPoints; i++)
	{
		float x = std::cos(2 * PI * i / (float)numPoints);
		float y = std::sin(2 * PI * i / (float)numPoints);
		circlePoints[i] = glm::vec3(x, y, 0.0f);
	}
	circlePoints[numPoints] = glm::vec3(0.0f);
	
	unsigned int circleVAO;
	glGenVertexArrays(1, &circleVAO);
	glBindVertexArray(circleVAO);

	unsigned int circleVBO;
	glGenBuffers(1, &circleVBO);
	glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(circlePoints), circlePoints, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(0);

	unsigned int circleIndices[numPoints * 3];
	for (int i = 0; i < numPoints-1; i++)
	{
		circleIndices[i*3] = i;
		circleIndices[i*3 + 1] = numPoints;
		circleIndices[i*3 + 2] = i+1;
	}
	circleIndices[(numPoints - 1) * 3] = numPoints - 1;
	circleIndices[(numPoints - 1) * 3 + 1] = numPoints;
	circleIndices[(numPoints - 1) * 3 + 2] = 0;

	unsigned int circleEBO;
	glGenBuffers(1, &circleEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circleEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(circleIndices), circleIndices, GL_STATIC_DRAW);
	//glVertexAttribPointer(0,3,GL_UNSIGNED_INT,)

	
	// obstacle
	glm::vec3 obsPos = glm::vec3(0.0f);
	float obsRad = 2.0f;
	
	// agent
	glm::vec3 agentPos = glm::vec3(-9.0, -9.0, 0.0);
	float agentRad = 0.5f;
	glm::vec3 agentStart = agentPos;
	glm::vec3 agentGoal = glm::vec3(9.0, 9.0, 0.0);


	// Random positions
	const int numNewPos = 30;
	glm::vec3 points[numNewPos+2];
	for (int i = 0; i < numNewPos; i++)
	{
		float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
		float x = (r - 0.5f) * 20.0f;
		r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
		float y = (r - 0.5f) * 20.0f;
		points[i] = glm::vec3(x, y, -0.1f);
	}
	points[numNewPos] = agentPos;
	points[numNewPos+1] = agentGoal;
	int numNodes = numNewPos + 2;
	

	std::vector<unsigned int> edges[numNewPos + 2];
	std::vector<unsigned int> edgeIndices;
	std::vector<glm::vec3> nearestPoints;
	for (int i = 0; i < numNewPos + 2; i++)
	{
		// For each point connect to every point with line of sight
		for (int j = 0; j < numNewPos+2; j++)
		{
			if (i != j)
			{
				glm::vec3 line = points[j] - points[i];
				glm::vec3 pointToObs = obsPos - points[i];
				float dot = glm::dot(pointToObs, glm::normalize(line));
				glm::vec3 nearestPoint = points[i] + glm::normalize(line) * dot;

				bool point1InObs = glm::length(obsPos - points[i]) < (obsRad + agentRad);
				bool point2InObs = glm::length(obsPos - points[j]) < (obsRad + agentRad);
				bool onSegment = (dot > 0 && dot < glm::length(line));
				bool inObs(glm::length(obsPos - nearestPoint) < (obsRad + agentRad));
				
				if (!point1InObs && !point2InObs && !(onSegment && inObs))
				{
					// Line of sight
					edges[i].push_back(j);
					edgeIndices.push_back(i);
					edgeIndices.push_back(j);
				}

				nearestPoints.push_back(nearestPoint);
			}
		}
	}

	unsigned int pointVAO, pointVBO, edgeEBO;
	glGenVertexArrays(1, &pointVAO);
	glBindVertexArray(pointVAO);

	glGenBuffers(1, &pointVBO);
	glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &edgeEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * edgeIndices.size(), edgeIndices.data(), GL_STATIC_DRAW);

	unsigned int pointVAO2, pointVBO2;
	glGenVertexArrays(1, &pointVAO2);
	glBindVertexArray(pointVAO2);

	glGenBuffers(1, &pointVBO2);
	glBindBuffer(GL_ARRAY_BUFFER, pointVBO2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * nearestPoints.size(), nearestPoints.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(0);

	//*
	// Find a path w/ A*
	int start = numNewPos;
	int goal = numNewPos + 1;
	std::vector<unsigned int> fringe; //Known nodes not yet explored
	std::vector<unsigned int> explored; //Nodes already explored
	unsigned int* cameFrom = new unsigned int[numNodes]; // For each node the best node to get their from
	float* gVal = new float[numNodes]; // Cost of getting to each node
	float* fVal = new float[numNodes]; // Cost of getting to each node plus distance to goal
	for (int i = 0; i < numNodes; i++)
	{
		gVal[i] = INFINITY;
		fVal[i] = INFINITY;
	}
	gVal[start] = 0.0f;
	fVal[start] = glm::length(points[start] - points[goal]);
	fringe.push_back(start);

	unsigned int current = start;
	while (current != goal && fringe.size() > 0)
	{
		// First get lowest cost node in fringe - we will explore that one
		float lowestF = INFINITY;
		unsigned int lowestFIndex = 0;
		for (int i = 0; i < fringe.size(); i++)
		{ // For each node in fringe
			if (fVal[fringe[i]] < lowestF)
			{
				lowestF = fVal[fringe[i]];
				lowestFIndex = i;
				current = fringe[i];
			}
		}

		if (lowestF != INFINITY)
		{
			// Explore current node
			fringe.erase(fringe.begin() + lowestFIndex); // Remove from fringe
			explored.push_back(current); // Add to explored

			// For each neighbor of current node
			for (int i = 0; i < edges[current].size(); i++)
			{
				unsigned int lookingAt = edges[current][i];

				// If it has not already been explored
				bool hasBeenExplored = false;
				for (int j = 0; j < explored.size(); j++)
				{
					if (explored[j] == lookingAt)
					{
						hasBeenExplored = true;
					}
				}
				float pathLength = gVal[current] + glm::length(points[lookingAt] - points[current]);
				bool isInFringe = false;
				bool hasBetterPath = false;
				for (int j = 0; j < fringe.size(); j++)
				{
					if (fringe[j] == lookingAt)
					{
						isInFringe = true;
						if (gVal[lookingAt] < pathLength)
						{
							hasBetterPath = true;
							break;
						}
					}
				}

				if (!hasBeenExplored && !isInFringe)
				{
					// Add it if it isn't in the fringe
					fringe.push_back(lookingAt);

					cameFrom[lookingAt] = current;
					gVal[lookingAt] = pathLength;
					fVal[lookingAt] = 1.0f * pathLength + 0.0f * glm::length(points[lookingAt] - points[goal]);
				}
				// If there isn't already a better path
				else if (isInFringe && !hasBetterPath)
				{
					cameFrom[lookingAt] = current;
					gVal[lookingAt] = pathLength;
					fVal[lookingAt] = 1.0f * pathLength + 1.0f * glm::length(points[lookingAt] - points[goal]);
				}
			}
		}

	}
	//A* is done, build path
	std::vector<unsigned int> path;
	current = goal;
	while (current != start)
	{
		path.push_back(current);
		current = cameFrom[current];
	}
	path.push_back(start);
	//Now just pop path to get next point on path


	int pathSize = path.size();

	unsigned int pathVAO, pathVBO, pathEBO;
	glGenVertexArrays(1, &pathVAO);
	glBindVertexArray(pathVAO);

	glGenBuffers(1, &pathVBO);
	glBindBuffer(GL_ARRAY_BUFFER, pathVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &pathEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pathEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * path.size(), path.data(), GL_STATIC_DRAW);

	path.pop_back();
	glm::vec3 nextPathPoint = points[path.back()];

	//*/

	// render loop ----------------------------
	while (!glfwWindowShouldClose(window))
	{
		// Set deltaT
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		processInput(window);

		// processing

		// Move agent along path
		if (moveAgent)
		{
			// First check if you can see the next point, if you can move towards that instead
			if (nextPathPoint != points[goal])
			{
				glm::vec3 line = points[path.back()] - agentPos;
				glm::vec3 pointToObs = obsPos - agentPos;
				float dot = glm::dot(pointToObs, glm::normalize(line));
				glm::vec3 nearestPoint = agentPos + glm::normalize(line) * dot;

				bool onSegment = (dot > 0 && dot < glm::length(line));
				bool inObs(glm::length(obsPos - nearestPoint) < (obsRad + agentRad));

				if (!(onSegment && inObs))
				{
					// Can see next point
					nextPathPoint = points[path.back()];
					path.pop_back();
				}
			}

			float agentSpeed = 2.0f;
			agentPos += glm::normalize(nextPathPoint - agentPos) * agentSpeed * deltaTime;
		}

		// rendering commands here
		glClearColor(0.9f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Grid
		myShader.use();
		myShader.setFloat("aspectRatio", (float)SCR_WIDTH / (float)SCR_HEIGHT);
		
		
		glBindVertexArray(gridVAO);
		glm::mat4 model = glm::mat4(1.0f);
		myShader.setMat4("model", model);
		myShader.setVec4("color", glm::vec4(0.1, 0.1, 0.1, 1.0));
		glPointSize(2.0f);
		glDrawArrays(GL_POINTS, 0, 400);



		glBindVertexArray(circleVAO);
		// Draw agent
		myShader.setVec4("color", glm::vec4(0.2f, 0.9f, 0.3f, 1.0f));
		model = glm::translate(model, agentPos - glm::vec3(0.0f, 0.0f, 0.5f));
		model = glm::scale(model, glm::vec3(agentRad));
		myShader.setMat4("model", model);
		glDrawElements(GL_TRIANGLES, numPoints * 3, GL_UNSIGNED_INT, 0);
		
		// Draw Obstacle
		myShader.setVec4("color", glm::vec4(0.5f, 0.1f, 0.1f, 1.0f));
		model = glm::mat4(1.0f);
		model = glm::scale(model, glm::vec3(obsRad));
		myShader.setMat4("model", model);
		glDrawElements(GL_TRIANGLES, numPoints * 3, GL_UNSIGNED_INT, 0);

		// Draw Config Obstacle
		if (showConfig)
		{
			myShader.setVec4("color", glm::vec4(0.9f, 0.1f, 0.1f, 1.0f));
			model = glm::mat4(1.0f);
			model = glm::scale(model, glm::vec3(obsRad + agentRad));
			myShader.setMat4("model", model);
			glDrawElements(GL_TRIANGLES, numPoints * 3, GL_UNSIGNED_INT, 0);
		}

		// Draw Goal
		myShader.setVec4("color", glm::vec4(0.1f, 0.3f, 0.7f, 1.0f));
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(9.0f, 9.0f, 0.9f));
		model = glm::scale(model, glm::vec3(0.35));
		myShader.setMat4("model", model);
		glDrawElements(GL_TRIANGLES, numPoints * 3, GL_UNSIGNED_INT, 0);

		if (showPoints)
		{
			// Points
			glBindVertexArray(pointVAO);
			model = glm::mat4(1.0f);
			myShader.setMat4("model", model);
			myShader.setVec4("color", glm::vec4(0.0, 0.5, 0.5, 1.0));
			glPointSize(10.0f);
			glDrawArrays(GL_POINTS, 0, numNewPos + 2);

			if (showEdges)
				glDrawElements(GL_LINES, edgeIndices.size(), GL_UNSIGNED_INT, 0);
		}
		// Points 2
		/*
		glBindVertexArray(pointVAO2);
		model = glm::mat4(1.0f);
		myShader.setMat4("model", model);
		myShader.setVec4("color", glm::vec4(0.9, 0.5, 0.5, 1.0));
		glPointSize(7.0f);
		glDrawArrays(GL_POINTS, 0, nearestPoints.size());
		//*/

		if (showPath)
		{
			glBindVertexArray(pathVAO);
			model = glm::mat4(1.0f);
			myShader.setMat4("model", model);
			myShader.setVec4("color", glm::vec4(0.0, 0.8, 0.2, 1.0));
			glPointSize(15.0f);
			glDrawElements(GL_POINTS, pathSize, GL_UNSIGNED_INT, 0);

			glDrawElements(GL_LINE_STRIP, pathSize, GL_UNSIGNED_INT, 0);
		}

		// check and call events and swap the buffers
		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	glfwTerminate();

	return 0;
}

// This function is called whenever window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// Process all keyboard input here
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		moveAgent = !moveAgent;
	if (key == GLFW_KEY_1 && action == GLFW_PRESS)
		showConfig = !showConfig;
	if (key == GLFW_KEY_2 && action == GLFW_PRESS)
		showPoints = !showPoints;
	if (key == GLFW_KEY_3 && action == GLFW_PRESS)
		showEdges = !showEdges;
	if (key == GLFW_KEY_4 && action == GLFW_PRESS)
		showPath = !showPath;
}