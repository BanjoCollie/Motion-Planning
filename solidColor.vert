#version 330 core

layout (location = 0) in vec3 aPos;
 
uniform mat4 model;
uniform float aspectRatio;

void main()
{
	//Move it into a 20x20 grid
	vec4 pos = model * vec4(aPos, 1.0);

	pos.y = pos.y / 10.0f;
	pos.x = pos.x / (10.0f * aspectRatio);

    gl_Position = pos;
}