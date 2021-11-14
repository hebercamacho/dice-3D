#include "openglwindow.hpp"

#include <fmt/core.h>
#include <imgui.h>
#include <tiny_obj_loader.h>

#include <cppitertools/itertools.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

// Explicit specialization of std::hash for Vertex
namespace std {
template <>
//necessário para podermos usar Vertex como chave para pegar um valor de índice de uma tabela hash 
//isso ajuda a compactar bem nossa geometria indexada
struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const noexcept {
    const std::size_t h1{std::hash<glm::vec3>()(vertex.position)}; //como é 3D e hash já possui uma especialização para vec3, vamos usar isso por enquanto
    return h1;
  }
};
}  // namespace std

void OpenGLWindow::initializeGL() {
  abcg::glClearColor(0, 0, 0, 1);

  // Enable depth buffering
  abcg::glEnable(GL_DEPTH_TEST); //descartar fragmentos dependendo da profundidade

  // Create program
  m_program = createProgramFromFile(getAssetsPath() + "loadmodel.vert",
                                    getAssetsPath() + "loadmodel.frag");

  // Load model
  loadModelFromFile(getAssetsPath() + "dice.obj"); //carregamento do .obj
  standardize();

  m_verticesToDraw = m_indices.size();

  // Generate VBO
  abcg::glGenBuffers(1, &m_VBO);
  abcg::glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  abcg::glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices[0]) * m_vertices.size(),
                     m_vertices.data(), GL_STATIC_DRAW);
  abcg::glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Generate EBO
  abcg::glGenBuffers(1, &m_EBO);
  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
  abcg::glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(m_indices[0]) * m_indices.size(), m_indices.data(),
                     GL_STATIC_DRAW);
  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Create VAO
  abcg::glGenVertexArrays(1, &m_VAO);

  // Bind vertex attributes to current VAO
  abcg::glBindVertexArray(m_VAO);

  abcg::glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  GLint positionAttribute{abcg::glGetAttribLocation(m_program, "inPosition")};
  abcg::glEnableVertexAttribArray(positionAttribute);
  abcg::glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex), nullptr);
  abcg::glBindBuffer(GL_ARRAY_BUFFER, 0);

  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

  // End of binding to current VAO
  abcg::glBindVertexArray(0);
}

//carregar e ler o arquivo .obj, armazenar vertices e indices em m_vertices e m_indices.
void OpenGLWindow::loadModelFromFile(std::string path) {
  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path.data())) {
    if (!reader.Error().empty()) {
      throw abcg::Exception{abcg::Exception::Runtime(
          fmt::format("Failed to load model {} ({})", path, reader.Error()))};
    }
    throw abcg::Exception{
        abcg::Exception::Runtime(fmt::format("Failed to load model {}", path))};
  }

  if (!reader.Warning().empty()) {
    fmt::print("Warning: {}\n", reader.Warning());
  }

  const auto& attrib{reader.GetAttrib()}; //conjunto de vertices
  const auto& shapes{reader.GetShapes()}; //conjunto de malhas

  m_vertices.clear();
  m_indices.clear();

  // A key:value map with key=Vertex and value=index
  std::unordered_map<Vertex, GLuint> hash{};

  // ler todos os triangulos e vertices
  for (const auto& shape : shapes) {
    // pra cada um dos indices
    for (const auto offset : iter::range(shape.mesh.indices.size())) {
      // Access to vertex
      const tinyobj::index_t index{shape.mesh.indices.at(offset)};

      // Vertex position
      const int startIndex{3 * index.vertex_index};
      const float vx{attrib.vertices.at(startIndex + 0)};
      const float vy{attrib.vertices.at(startIndex + 1)};
      const float vz{attrib.vertices.at(startIndex + 2)};

      Vertex vertex{};
      vertex.position = {vx, vy, vz}; //a chave do vertex é sua posição
    
      // If hash doesn't contain this vertex
      if (hash.count(vertex) == 0) {
        // Add this index (size of m_vertices)
        hash[vertex] = m_vertices.size(); //o valor do hash é a ordem que esse vertex foi lido
        // Add this vertex
        m_vertices.push_back(vertex); //o vértice é adicionado ao arranjo de vértices, se ainda não existir
      }
      //no arranjo de índices, podem haver posições duplicadas, pois os vértices podem ser compartilhados por triangulos diferentes
      m_indices.push_back(hash[vertex]); //o valor do hash deste vértice e adicionado ao arranjo de indices
    }
  }
}
//função para centralizar e aplicar escala,
//modificando vertices carregados do .obj para que a geometria caiba no volume de visão do pipeline gráfico,
// que é o cubo de tamanho 2×2×2 centralizado em (0,0,0)
void OpenGLWindow::standardize() {
  // Center to origin and normalize largest bound to [-1, 1]

  // achar maiores e menores valores de x,y,z
  glm::vec3 max(std::numeric_limits<float>::lowest());
  glm::vec3 min(std::numeric_limits<float>::max());
  for (const auto& vertex : m_vertices) {
    max.x = std::max(max.x, vertex.position.x);
    max.y = std::max(max.y, vertex.position.y);
    max.z = std::max(max.z, vertex.position.z);
    min.x = std::min(min.x, vertex.position.x);
    min.y = std::min(min.y, vertex.position.y);
    min.z = std::min(min.z, vertex.position.z);
  }

  
  const auto center{(min + max) / 2.0f}; // calculo do centro da caixa
  const auto scaling{2.0f / glm::length(max - min)}; //calculo do fator de escala, de forma que a maior dimensão da caixa tenha comprimento 2
  for (auto& vertex : m_vertices) {
    vertex.position = (vertex.position - center) * scaling; //centralizar modelo na origem e aplicar escala
  }
}

void OpenGLWindow::paintGL() {
  const float deltaTime{static_cast<float>(getDeltaTime())};
  // angulo (em radianos) é incrementado se houver alguma rotação ativa
  if(m_rotation[0] || m_rotation[1] ||m_rotation[2]){
    //ajuste de velocidade de rotação, necessário para conseguirmos pausar
    myTime = deltaTime;
    
    //incrementa ângulo de {x,y,z} se rotação em torno do eixo {x,y,z} estiver ativa
    if(m_rotation[0])
      m_angle[0] = glm::wrapAngle(m_angle[0] + glm::radians(45.0f) * myTime);

    if(m_rotation[1])
      m_angle[1] = glm::wrapAngle(m_angle[1] + glm::radians(45.0f) * myTime);

    if(m_rotation[2])
      m_angle[2] = glm::wrapAngle(m_angle[2] + glm::radians(45.0f) * myTime);
  }
  //debug
  //fmt::print("angle: {} {} {}\n", m_angle[0], m_angle[1], m_angle[2]);
  //fmt::print("myTime: {} delta: {}\n", myTime, deltaTime);

  // Clear color buffer and depth buffer
  abcg::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //código praticamente padrão daqui em diante
  abcg::glViewport(0, 0, m_viewportWidth, m_viewportHeight);
  
  abcg::glUseProgram(m_program); //usar shaders
  abcg::glBindVertexArray(m_VAO); //usar vao

  // atualizar variavel do angulo para dentro do vertex shader

  const GLint rotationXLoc{abcg::glGetUniformLocation(m_program, "rotationX")};
  abcg::glUniform1f(rotationXLoc, m_angle[0]);
  const GLint rotationYLoc{abcg::glGetUniformLocation(m_program, "rotationY")};
  abcg::glUniform1f(rotationYLoc, m_angle[1]);
  const GLint rotationZLoc{abcg::glGetUniformLocation(m_program, "rotationZ")};
  abcg::glUniform1f(rotationZLoc, m_angle[2]);

  // Draw triangles
  abcg::glDrawElements(GL_TRIANGLES, m_verticesToDraw, GL_UNSIGNED_INT,
                       nullptr);

  abcg::glBindVertexArray(0);
  abcg::glUseProgram(0);
}

void OpenGLWindow::paintUI() {
  abcg::OpenGLWindow::paintUI();

  // Create window for slider
  {
    ImGui::SetNextWindowPos(ImVec2(5, m_viewportHeight - 94));
    ImGui::SetNextWindowSize(ImVec2(m_viewportWidth - 10, -1));
    ImGui::Begin("Slider window", nullptr, ImGuiWindowFlags_NoDecoration);

    // Create a slider to control the number of rendered triangles
    {
      // Slider will fill the space of the window
      ImGui::PushItemWidth(m_viewportWidth - 25);
      //definição do slider que controla o numero de triangulos que será renderizado
      static int n{m_verticesToDraw / 3};
      ImGui::SliderInt("", &n, 0, m_indices.size() / 3, "%d triangles");
      m_verticesToDraw = n * 3;

      ImGui::PopItemWidth();
    }

    ImGui::End();
  }

  // Create a window for the other widgets
  {
    const auto widgetSize{ImVec2(172, 212)};
    ImGui::SetNextWindowPos(ImVec2(m_viewportWidth - widgetSize.x - 5, 5));
    ImGui::SetNextWindowSize(widgetSize);
    ImGui::Begin("Widget window", nullptr, ImGuiWindowFlags_NoDecoration);
    //checkbox para ativação de Face culling (descarte das faces não viradas para a tela)
    static bool faceCulling{};
    ImGui::Checkbox("Back-face culling", &faceCulling);

    if (faceCulling) {
      abcg::glEnable(GL_CULL_FACE);
    } else {
      abcg::glDisable(GL_CULL_FACE);
    }

    // CW/CCW combo box
    {
      static std::size_t currentIndex{};
      const std::vector<std::string> comboItems{"CW", "CCW"};

      ImGui::PushItemWidth(70);
      if (ImGui::BeginCombo("Front face",
                            comboItems.at(currentIndex).c_str())) {
        for (const auto index : iter::range(comboItems.size())) {
          const bool isSelected{currentIndex == index};
          if (ImGui::Selectable(comboItems.at(index).c_str(), isSelected))
            currentIndex = index;
          if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::PopItemWidth();
      //de acordo com o escolhido na combo box, define se a orientação dos indices é horario ou anti horario
      //na pratica, isso vira o objeto do avesso, pois inverte o que é frente e o que é costas dos triangulos
      if (currentIndex == 0) {
        abcg::glFrontFace(GL_CW);
      } else {
        abcg::glFrontFace(GL_CCW);
      }
    }

    {
      //checkbox para decisão de qual direção rotacionar
      static bool rotateX{}, rotateY{}, rotateZ{};
      ImGui::Checkbox("Rotate X", &rotateX);
      ImGui::Checkbox("Rotate Y", &rotateY);
      ImGui::Checkbox("Rotate Z", &rotateZ);

      m_rotation[0] = rotateX;
      m_rotation[1] = rotateY;
      m_rotation[2] = rotateZ;
      
    }

    ImGui::End();
  }
}

void OpenGLWindow::resizeGL(int width, int height) {
  m_viewportWidth = width;
  m_viewportHeight = height;
}

void OpenGLWindow::terminateGL() {
  abcg::glDeleteProgram(m_program);
  abcg::glDeleteBuffers(1, &m_EBO);
  abcg::glDeleteBuffers(1, &m_VBO);
  abcg::glDeleteVertexArrays(1, &m_VAO);
}