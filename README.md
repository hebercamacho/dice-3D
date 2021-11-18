# Dice 3D
## Projeto de Computação Gráfica 2021.3.

O projeto consiste num jogo de dado em 3D, com a possibilidade de jogar várias vezes e ter resultados aleatórios.
A visualização é fixa, como se estivéssemos olhando de cima para a superfície onde o dado está sendo jogado.
Pressionando o botão "Jogar!", é possível jogar o dado novamente, sendo que cada jogada é independente da anterior, exceto pelo fato de que o dado irá partir da posição na qual parou na última vez.

Para renderização, foi utilizada as biblioteca [ABCg](https://github.com/hbatagelo/abcg) e suas dependências.

[Clique aqui para jogar](https://hebercamacho.github.io/dice-3D/dice)

# Desenvolvedores
Héber Camacho Desterro RA: 11069416

# Técnicas Utilizadas
- [x] Rotação 3D através de transformação matricial, em torno de qualquer um dos três eixos
- [x] Translação para qualquer direção dentro da janela
- [x] Carregamento de arquivo .obj
- [x] Diferença de cores entre diferentes materiais, usando arquivo .mtl (Material Template Library)
- [x] Efeito de sombreamento no objeto dependente da distância
- [x] Uso de números aleatórios para: 
    - resultado do dado, 
    - posição inicial, 
    - direção, 
    - velocidade, 
    - velocidade angular, 
    - eixo a ser rotacionado (para efeito mais realista),
    - tempo em que o dado permanecerá girando.
- [x] Botão da biblioteca ImGui para jogar o dado quantas vezes quiser
- [x] Separação da classe Dice para gerar vários dados
- [ ] Combo ou Slider da biblioteca ImGui para decidir quantos dados gerar
