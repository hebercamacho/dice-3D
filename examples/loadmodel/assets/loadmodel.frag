#version 410

out vec4 outColor;

void main() {
  //i é a intensidade da cor
  float i = 1.1 - gl_FragCoord.z;
  //i depende de z para que um fragmento mais no fundo fique mais escuro

  //gl_FrontFacing é true se esta é a face da frente
  if (gl_FrontFacing) {
    outColor = vec4(i, i, i, 1); //por fora, tom entre branco e cinza
  } else {
    outColor = vec4(0.1, 0.1, 0.1, 1); //por dentro, tom de cinza escuro
  }
}