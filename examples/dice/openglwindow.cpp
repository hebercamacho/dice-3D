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
  m_program = createProgramFromFile(getAssetsPath() + "dice.vert",
                                    getAssetsPath() + "dice.frag");

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

  // Bind vertex attributes
  GLint positionAttribute{abcg::glGetAttribLocation(m_program, "inPosition")}; //layout(location = _)
  if (positionAttribute >= 0) {
    abcg::glEnableVertexAttribArray(positionAttribute);
    abcg::glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE,
                                sizeof(Vertex), nullptr);
  }

  const GLint colorAttribute{abcg::glGetAttribLocation(m_program, "inColor")};
  if (colorAttribute >= 0) {
    abcg::glEnableVertexAttribArray(colorAttribute);
    GLsizei offset{sizeof(glm::vec3)};
    abcg::glVertexAttribPointer(colorAttribute, 3, GL_FLOAT, GL_FALSE,
                                sizeof(Vertex),
                                reinterpret_cast<void*>(offset));
  }

  

  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

  // End of binding to current VAO
  abcg::glBindBuffer(GL_ARRAY_BUFFER, 0);
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
  const auto& shapes{reader.GetShapes()}; //conjunto de objetos (só tem 1)

  m_vertices.clear();
  m_indices.clear();

  // A key:value map with key=Vertex and value=index
  std::unordered_map<Vertex, GLuint> hash{};
  // fmt::print("shapes.at(0).mesh.material_ids.size(): {}\n", shapes.at(0).mesh.material_ids.size());
  // fmt::print("shapes.at(0).points.indices.size(): {}\n", shapes.at(0).mesh.indices.size());
  // fmt::print("shapes.at(0).mesh.num_face_vertices.size(): {}\n", shapes.at(0).mesh.num_face_vertices.size());
  // fmt::print("shapes.at(0).mesh.tags.size(): {}\n", shapes.at(0).mesh.tags.size());
  // ler todos os triangulos e vertices
  for (const auto& shape : shapes) { 
    // pra cada um dos indices
    for (const auto offset : iter::range(shape.mesh.indices.size())) { //122112 indices = numero de triangulos * 3
      // Access to vertex
      const tinyobj::index_t index{shape.mesh.indices.at(offset)}; //offset vai ser de 0 a 122112, index vai acessar cada vertice nessas posições offset

      // Vertex position
      const int startIndex{3 * index.vertex_index}; //startIndex vai encontrar o indice exato de cada vertice
      const float vx{attrib.vertices.at(startIndex + 0)};
      const float vy{attrib.vertices.at(startIndex + 1)};
      const float vz{attrib.vertices.at(startIndex + 2)};

      //são 40704 triangulos, dos quais 27264 brancos.
      //se fizermos offset / 3 teremos o indice do triangulos?
      
      const auto material_id = shape.mesh.material_ids.at(offset/3);
      // if(material_id !=1)
      // fmt::print("material_id do triangulo: {}\n", (float)material_id);

      // fmt::print("offset: {}\n", offset);
      // fmt::print("shape.mesh.indices.size(): {}\n", shape.mesh.indices.size());
      // fmt::print("index.vertex_index: {}\n", index.vertex_index);
      // fmt::print("startIndex: {}\n", startIndex);

      Vertex vertex{};
      vertex.position = {vx, vy, vz}; //a chave do vertex é sua posição
      vertex.color = {(float)material_id, (float)material_id, (float)material_id};
      // fmt::print("position x: {} color r: {}\n", vertex.position.x, vertex.color.r);

      // If hash doesn't contain this vertex
      if (hash.count(vertex) == 0) {
        // Add this index (size of m_vertices)
        hash[vertex] = m_vertices.size(); //o valor do hash é a ordem que esse vertex foi lido
        // Add this vertex
        m_vertices.push_back(vertex); //o vértice é adicionado ao arranjo de vértices, se ainda não existir
      }
      //no arranjo de índices, podem haver posições duplicadas, pois os vértices podem ser compartilhados por triangulos diferentes
      m_indices.push_back(hash[vertex]); //o valor do hash deste vértice (suua ordem) é adicionado ao arranjo de indices
    }
  }
}
//função para centralizar e aplicar escala,
//modificando vertices carregados do .obj para que a geometria caiba no volume de visão do pipeline gráfico,
// que é o cubo de tamanho 2×2×2 centralizado em (0,0,0).
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
  // const float deltaTime{static_cast<float>(getDeltaTime())};
  // // angulo (em radianos) é incrementado se houver alguma rotação ativa
  // if(m_rotation.x || m_rotation.y ||m_rotation.z){
  //   //ajuste de velocidade de rotação, necessário para conseguirmos pausar
  //   myTime = deltaTime;
    
  //   //incrementa ângulo de {x,y,z} se rotação em torno do eixo {x,y,z} estiver ativa
  //   if(m_rotation.x)
  //     m_angle.x = glm::wrapAngle(m_angle.x + glm::radians(45.0f) * myTime);

  //   if(m_rotation.y)
  //     m_angle.y = glm::wrapAngle(m_angle.y + glm::radians(45.0f) * myTime);

  //   if(m_rotation.z)
  //     m_angle.z = glm::wrapAngle(m_angle.z + glm::radians(45.0f) * myTime);
  // }
  //debug
  //fmt::print("angle: {} {} {}\n", m_angle.x, m_angle.y, m_angle.z);
  //fmt::print("myTime: {} delta: {}\n", myTime, deltaTime);

  //Dado sendo girado
  if(dadoGirando){
    quadros++;
    if(quadros > 600){
      jogarDado();
    }
  }
  //quantos segundos se passaram desde a ultima atualização da tela
  const float deltaTime{static_cast<float>(getDeltaTime())};
  // angulo (em radianos) é incrementado se houver alguma rotação ativa
  if(m_rotation.x || m_rotation.y ||m_rotation.z){
    //ajuste de velocidade de rotação, necessário para conseguirmos pausar
    myTime = deltaTime;
    
    //incrementa ângulo de {x,y,z} se rotação em torno do eixo {x,y,z} estiver ativa
    if(m_rotation.x)
      m_angle.x = glm::wrapAngle(m_angle.x + velocidadeAngular.x * myTime);

    if(m_rotation.y)
      m_angle.y = glm::wrapAngle(m_angle.y + velocidadeAngular.y * myTime);

    if(m_rotation.z)
      m_angle.z = glm::wrapAngle(m_angle.z + velocidadeAngular.z * myTime);
  }
  //fmt::print("angle: {} {} {}\n", m_angle.x, m_angle.y, m_angle.z);

  
  // Clear color buffer and depth buffer
  abcg::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //código praticamente padrão daqui em diante
  abcg::glViewport(0, 0, m_viewportWidth, m_viewportHeight);
  
  abcg::glUseProgram(m_program); //usar shaders
  abcg::glBindVertexArray(m_VAO); //usar vao

  //atualizar cor do vertice
  // const GLint m_colorLoc = abcg::glGetUniformLocation(m_program, "color");  
  // abcg::glUniform3f(m_colorLoc, 1,1,1);
  //fmt::print("m_vertices[0].position.x: {} color r: {}\n",m_vertices[0].position.x, m_vertices[0].color.r);

  // atualizar variavel do angulo para dentro do vertex shader
  const GLint rotationXLoc{abcg::glGetUniformLocation(m_program, "rotationX")};
  abcg::glUniform1f(rotationXLoc, m_angle.x);
  const GLint rotationYLoc{abcg::glGetUniformLocation(m_program, "rotationY")};
  abcg::glUniform1f(rotationYLoc, m_angle.y);
  const GLint rotationZLoc{abcg::glGetUniformLocation(m_program, "rotationZ")};
  abcg::glUniform1f(rotationZLoc, m_angle.z);

  // Draw triangles
  abcg::glDrawElements(GL_TRIANGLES, m_verticesToDraw, GL_UNSIGNED_INT,
                       nullptr);

  abcg::glBindVertexArray(0);
  abcg::glUseProgram(0);
}

void OpenGLWindow::paintUI() {
  abcg::OpenGLWindow::paintUI();
  //Botão jogar dado
  {
    ImGui::SetNextWindowPos(ImVec2(5,15));
    ImGui::SetNextWindowSize(ImVec2(100, 200));
    ImGui::Begin("Button window", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::PushItemWidth(200);

    if(ImGui::Button("Jogar!")){
      giradinhaAleatoria();
      dadoGirando = true;
    }

    ImGui::PopItemWidth();
    ImGui::End();
  }

  // // Create window for slider
  // {
  //   ImGui::SetNextWindowPos(ImVec2(5, m_viewportHeight - 150));
  //   ImGui::SetNextWindowSize(ImVec2(m_viewportWidth - 10, -1));
  //   ImGui::Begin("Slider window", nullptr, ImGuiWindowFlags_NoDecoration);

  //   // Create a slider to control the number of rendered triangles
  //   {
  //     // Slider will fill the space of the window
  //     ImGui::PushItemWidth(m_viewportWidth - 25);
  //     //definição do slider que controla o numero de triangulos que será renderizado
  //     // static int n{m_verticesToDraw / 3};
  //     // ImGui::SliderInt("", &n, 0, m_indices.size() / 3, "%d triangles");
  //     // m_verticesToDraw = n * 3;

  //     //Sliders de angulo
  //     // static float n_X{glm::degrees(m_angle.x)}; 
  //     // static float n_Y{glm::degrees(m_angle.y)}; 
  //     // static float n_Z{glm::degrees(m_angle.z)};
  //     // ImGui::SliderFloat("X", &n_X, 0.0f, 360.0f, "%.3f degrees");
  //     // ImGui::SliderFloat("Y", &n_Y, 0.0f, 360.0f, "%.3f degrees");
  //     // ImGui::SliderFloat("Z", &n_Z, 0.0f, 360.0f, "%.3f degrees");
  //     // m_angle.x = glm::radians(n_X);
  //     // m_angle.y = glm::radians(n_Y);
  //     // m_angle.z = glm::radians(n_Z);

  //     ImGui::PopItemWidth();
  //   }

  //   ImGui::End();
  // }

  // Create a window for the other widgets
  // {
  //   const auto widgetSize{ImVec2(172, 212)};
  //   ImGui::SetNextWindowPos(ImVec2(m_viewportWidth - widgetSize.x - 5, 5));
  //   ImGui::SetNextWindowSize(widgetSize);
  //   ImGui::Begin("Widget window", nullptr, ImGuiWindowFlags_NoDecoration);
  //   //checkbox para ativação de Face culling (descarte das faces não viradas para a tela)
  //   static bool faceCulling{};
  //   ImGui::Checkbox("Back-face culling", &faceCulling);

  //   if (faceCulling) {
  //     abcg::glEnable(GL_CULL_FACE);
  //   } else {
      // abcg::glDisable(GL_CULL_FACE);
  //   }

  //   // CW/CCW combo box
  //   {
  //     static std::size_t currentIndex{};
  //     const std::vector<std::string> comboItems{"CW", "CCW"};

  //     ImGui::PushItemWidth(70);
  //     if (ImGui::BeginCombo("Front face",
  //                           comboItems.at(currentIndex).c_str())) {
  //       for (const auto index : iter::range(comboItems.size())) {
  //         const bool isSelected{currentIndex == index};
  //         if (ImGui::Selectable(comboItems.at(index).c_str(), isSelected))
  //           currentIndex = index;
  //         if (isSelected) ImGui::SetItemDefaultFocus();
  //       }
  //       ImGui::EndCombo();
  //     }
  //     ImGui::PopItemWidth();
  //     //de acordo com o escolhido na combo box, define se a orientação dos indices é horario ou anti horario
  //     //na pratica, isso vira o objeto do avesso, pois inverte o que é frente e o que é costas dos triangulos
  //     if (currentIndex == 0) {
        abcg::glFrontFace(GL_CW);
  //     } else {
  //       abcg::glFrontFace(GL_CCW);
  //     }
  //   }

  //   // Número do dado box
  //   {
  //     static std::size_t currentIndex{};
  //     const std::vector<std::string> comboItems{"1", "2", "3", "4", "5", "6"};

  //     ImGui::PushItemWidth(70);
  //     if (ImGui::BeginCombo("Dice Face",
  //                           comboItems.at(currentIndex).c_str())) {
  //       for (const auto index : iter::range(comboItems.size())) {
  //         const bool isSelected{currentIndex == index};
  //         if (ImGui::Selectable(comboItems.at(index).c_str(), isSelected))
  //           currentIndex = index;
  //         if (isSelected) ImGui::SetItemDefaultFocus();
  //       }
  //       ImGui::EndCombo();
  //     }
  //     ImGui::PopItemWidth();
      
  //     m_angle = glm::radians(angulosRetos[currentIndex]);
  //   }

  //   {
  //     //checkbox para decisão de qual direção rotacionar
  //     static bool rotateX{}, rotateY{}, rotateZ{};
  //     ImGui::Checkbox("Rotate X", &rotateX);
  //     ImGui::Checkbox("Rotate Y", &rotateY);
  //     ImGui::Checkbox("Rotate Z", &rotateZ);

  //     m_rotation.x = rotateX;
  //     m_rotation.y = rotateY;
  //     m_rotation.z = rotateZ;
      
  //   }

  //   ImGui::End();
  // }
}

void OpenGLWindow::jogarDado() {
   // Start pseudo-random number generator
  auto seed{std::chrono::steady_clock::now().time_since_epoch().count()};
  m_randomEngine.seed(seed);
  
  quadros = 0;
  dadoGirando = false;
  m_rotation = {0,0,0};

  std::uniform_int_distribution<int> idist(1,6);
  m_angle = glm::radians(angulosRetos[idist(m_randomEngine)]);
}

void OpenGLWindow::giradinhaAleatoria(){
  // Start pseudo-random number generator
  auto seed{std::chrono::steady_clock::now().time_since_epoch().count()};
  m_randomEngine.seed(seed);
  
  //distribuição aleatória entre 0 e 1, porque não precisamos girar em todos os eixos
  std::uniform_int_distribution<int> idist(0,1);
  // m_rotation = {idist(m_randomEngine), idist(m_randomEngine), idist(m_randomEngine)};
  m_rotation = {1, 1, 1};
  fmt::print("m_rotation.x: {} m_rotation.y: {} m_rotation.z: {}\n",m_rotation.x,m_rotation.y,m_rotation.z);

  //distribuição aleatória de velocidade angular, para girar em cada eixo numa velocidade
  std::uniform_real_distribution<float> fdist(180.0f,720.0f);
  velocidadeAngular = {glm::radians(fdist(m_randomEngine))
                      ,glm::radians(fdist(m_randomEngine))
                      ,glm::radians(fdist(m_randomEngine))};
  fmt::print("velocidadeAngular.x: {} velocidadeAngular.y: {} velocidadeAngular.z: {}\n",velocidadeAngular.x,velocidadeAngular.y,velocidadeAngular.z);
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