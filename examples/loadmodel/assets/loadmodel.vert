#version 410 core

layout(location = 0) in vec3 inPosition; //posição (x,y,z) do vértice

uniform float angle;

void main() {
  float sinAngle = sin(angle);
  float cosAngle = cos(angle);
 //rotação em torno do eixo y (por isso y não muda)
  gl_Position = vec4( inPosition.x * cosAngle + inPosition.z * sinAngle,
                      inPosition.y,
                      inPosition.z * cosAngle - inPosition.x * sinAngle, 1.0);
}