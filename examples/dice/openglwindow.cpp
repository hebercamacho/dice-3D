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

  //aqui a gente passa a cor do vértice já pronta para o shader
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

  inicializarDado();
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

//função para centralizar o modelo na origem e aplicar escala, 
//normalizar as coordenadas de todos os vértices no intervalo [-1,1],
//modificando vertices carregados do .obj para que a geometria caiba no volume de visão do pipeline gráfico,
// que é o cubo de tamanho 2×2×2 centralizado em (0,0,0).
void OpenGLWindow::standardize() {
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
  //quantos segundos se passaram desde a ultima atualização da tela
  const float deltaTime{static_cast<float>(getDeltaTime())};

  //Dado sendo girado, temos que definir algumas variáveis para ilustrar seu giro de forma realista
  if(dadoGirando){
    
    quadros++;
    if(translation.x >= 1.5f) {
      movimentoDado.x = false;
      velocidadeAngularAleatoria();
      velocidadeDirecionalAleatoria();
    }
    else if (translation.x <= -1.5f) {
      movimentoDado.x = true;
      velocidadeAngularAleatoria();
      velocidadeDirecionalAleatoria();
    }

    if(translation.y >= 1.5f) {
      movimentoDado.y = false;
      velocidadeAngularAleatoria();
      velocidadeDirecionalAleatoria();
    }
    else if (translation.y <= -1.5f) {
      movimentoDado.y = true;
      velocidadeAngularAleatoria();
      velocidadeDirecionalAleatoria();
    }
    
    //ir pra direita
    if(movimentoDado.x) {
      translation.x += velocidadeDirecional.x; 
    }
    //ir pra esquerda
    else{
      translation.x -= velocidadeDirecional.x;
    }
    //ir pra cima
    if(movimentoDado.y) {
      translation.y += velocidadeDirecional.y; 
    }
    //ir pra baixo
    else{
      translation.y -= velocidadeDirecional.y;
    }

    //fmt::print("q: {} translation: {} {}\n",quadros, translation.x, translation.y);
    
    //podemos finalizar o giro do dado e parar num número aleatório
    if(quadros > maxQuadros){
      pousarDado();
    }
  }

  
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

  // atualizar variavel do angulo de rotação e posição de translação dentro do vertex shader
  const GLint rotationXLoc{abcg::glGetUniformLocation(m_program, "rotationX")};
  abcg::glUniform1f(rotationXLoc, m_angle.x);
  const GLint rotationYLoc{abcg::glGetUniformLocation(m_program, "rotationY")};
  abcg::glUniform1f(rotationYLoc, m_angle.y);
  const GLint rotationZLoc{abcg::glGetUniformLocation(m_program, "rotationZ")};
  abcg::glUniform1f(rotationZLoc, m_angle.z);
  const GLint translationLoc{abcg::glGetUniformLocation(m_program, "translation")};
  abcg::glUniform3fv(translationLoc, 1, &translation.x);

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
    ImGui::SetNextWindowSize(ImVec2(70, 40));
    ImGui::Begin("Button window", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::PushItemWidth(200);

    if(ImGui::Button("Jogar!")){
      tempoGirandoAleatorio();
      velocidadeAngularAleatoria();
      velocidadeDirecionalAleatoria();
      dadoGirando = true;
      
    }

    ImGui::PopItemWidth();
    ImGui::End();
  }
  
  //virar a face pra fora
  abcg::glFrontFace(GL_CW);
}

//função para começar o dado numa posição e número aleatório, além de inicializar algumas outras variáveis necessárias
void OpenGLWindow::inicializarDado() {
  // Inicializar gerador de números pseudo-aleatórios
  auto seed{std::chrono::steady_clock::now().time_since_epoch().count()};
  m_randomEngine.seed(seed);
  
  //estado inicial de algumas variáveis
  m_rotation = {0, 0, 0};  
  velocidadeAngular = {0.0f, 0.0f, 0.0f};
  myTime = 0.0f;
  quadros=0;

  std::uniform_real_distribution<float> fdist(-1.5f,1.5f);
  translation = {fdist(m_randomEngine),fdist(m_randomEngine),0.0f};
  pousarDado(); //começar num numero aleatorio
}

//função para fazer o dado parar numa das faces retas aleatoriamente
void OpenGLWindow::pousarDado() {
  //reinicialização de variáveis para podermos parar o dado e jogar novamente
  quadros = 0;
  dadoGirando = false;
  m_rotation = {0,0,0};

  //fmt::print("translation final: {} {}\n", translation.x, translation.y);

  std::uniform_int_distribution<int> idist(1,6);
  const int numeroDoDado = idist(m_randomEngine);
  m_angle.x = glm::radians(angulosRetos[numeroDoDado].x);
  m_angle.y = glm::radians(angulosRetos[numeroDoDado].y);
}

//função para definir tempo de giro do dado, algo entre 2 e 5 segundos 
void OpenGLWindow::tempoGirandoAleatorio(){
  const float FPS = ImGui::GetIO().Framerate;
  //distribuição aleatória para definir tempo de giro do dado, algo entre 2 e 5 segundos 
  std::uniform_int_distribution<int> idist((int)FPS * 2, (int)FPS * 5);
  maxQuadros = idist(m_randomEngine); //número máximo de quadros/vezes que o dado irá girar
  //fmt::print("maxQuadros: {}\n",maxQuadros);
}

//atualiza as velocidades de cada um dos eixos de forma aleatória
void OpenGLWindow::velocidadeAngularAleatoria(){
  //distribuição aleatória entre 0 e 2, para girar somente 1 eixo
  m_rotation = {0, 0, 0};
  std::uniform_int_distribution<int> idist(0,2);
  m_rotation[idist(m_randomEngine)] = 1;
  //fmt::print("m_rotation.x: {} m_rotation.y: {} m_rotation.z: {}\n",m_rotation.x,m_rotation.y,m_rotation.z);

  const float FPS = ImGui::GetIO().Framerate; //(para calcular os segundos precisamos do número inteiro de FPS)
  //fmt::print("FPS: {}\n",FPS);

  //distribuição aleatória de velocidade angular, para girar em cada eixo numa velocidade
  std::uniform_real_distribution<float> fdist(FPS * 4, FPS * 8);
  velocidadeAngular = {glm::radians(fdist(m_randomEngine))
                      ,glm::radians(fdist(m_randomEngine))
                      ,glm::radians(fdist(m_randomEngine))};
  //fmt::print("velocidadeAngular.x: {} velocidadeAngular.y: {} velocidadeAngular.z: {}\n",velocidadeAngular.x,velocidadeAngular.y,velocidadeAngular.z);
}

//recebe uma das dimensões da janela e retorna uma fração aleatória do seu tamanho
void OpenGLWindow::velocidadeDirecionalAleatoria(){
  const float deltaTime{static_cast<float>(getDeltaTime())};
  //distribuição aleatória de velocidade, para andar em cada eixo numa velocidade
  std::uniform_real_distribution<float> fdist(deltaTime / 200.0f, deltaTime / 100.0f);
  velocidadeDirecional.x = fdist(m_randomEngine) * m_viewportWidth;
  velocidadeDirecional.y = fdist(m_randomEngine) * m_viewportHeight;
  //fmt::print("velocidadeDirecionalAleatoria: {}\n", direcao);
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