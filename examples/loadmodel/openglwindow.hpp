#ifndef OPENGLWINDOW_HPP_
#define OPENGLWINDOW_HPP_

#include <vector>

#include "abcg.hpp"

//atributos que definem um vértice: posição 3D e operador == pra verificar se um vértice é igual a outro
struct Vertex {
  glm::vec3 position;
  glm::vec3 color;

  bool operator==(const Vertex& other) const {
    return position == other.position;
  }
};

class OpenGLWindow : public abcg::OpenGLWindow {
 protected:
  void initializeGL() override;
  void paintGL() override;
  void paintUI() override;
  void resizeGL(int width, int height) override;
  void terminateGL() override;

 private:
  GLuint m_VAO{};
  GLuint m_VBO{};
  GLuint m_EBO{};
  GLuint m_program{};

  int m_viewportWidth{};
  int m_viewportHeight{};

  glm::vec3 m_angle{0.0f, 0.0f, 0.0f}; // ângulo de rotação que será enviado à variável uniforme do vertex shader.
  int m_verticesToDraw{}; //quantidade de vértices do VBO que será processada pela função de renderização, glDrawElements
  glm::ivec3 m_rotation{0,0,0}; // nos ajuda a decidir qual a direção da rotação
  float myTime{0.0f};

  std::vector<Vertex> m_vertices; //arranjo de vértices lido do arquivo OBJ que será enviado ao VBO
  std::vector<GLuint> m_indices; //arranjo de indices lido do arquivo OBJ que será enviado ao EBO

  void loadModelFromFile(std::string path); //função para carregamento do arquivo OBJ
  void standardize(); //função para centralizar o modelo na origem e normalizar as coordenadas de todos os vértices no intervalo [-1,1]
};

#endif