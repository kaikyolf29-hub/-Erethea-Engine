# Teste do criador (Kaiky, no PC)

O CI prova o que dá para provar sem placa de vídeo: lavapipe no Linux e WARP no
Windows desenham na CPU. Este roteiro é o que só pode ser provado no seu PC, com
as placas de referência (a RX 580 8 GB, sem ray tracing, e a RTX 4060 Ti ou
5060 Ti de 16 GB) e com a Fornalha 3D de verdade. Cada item diz o que fazer e
o que tem que acontecer. Anote o que falhar, com uma foto ou um vídeo curto, e
o que pareceu lento ou feio: isso volta como tarefa para o Claude.

Para cada placa, faça os passos 1 a 9 e o 11. O passo 10 (teste conjunto com a
Fornalha) basta numa placa, de preferência a RTX.

## 1. Instalador e abertura

1. Baixe `erethea-setup-x86_64.exe` do release no GitHub e instale (sem
   assinatura por enquanto: o Windows mostra o aviso do SmartScreen; "Mais
   informações" → "Executar assim mesmo").
2. Abra a Erethea pelo menu Iniciar. Tem que abrir o gerenciador de projetos
   com o selo da Erethea, sem a barra de título do Windows.
3. Arraste a janela pelo topo até a borda da tela: ela encaixa (Snap do
   Windows 11), e os botões de minimizar, maximizar e fechar funcionam.

## 2. Projeto da Godot

1. Abra um projeto que você já tenha da Godot 4.x (ou um exemplo da Godot).
2. Ele tem que abrir sem erro, rodar com F5 e ficar igual à Godot.

## 3. Projeto novo e escolha pela placa

1. Crie um projeto novo (Forward+). Ele já começa com o preset de qualidade e o
   upscaler em `Automatic`.
2. Monte uma cena 3D simples (chão, cubo, `DirectionalLight3D`,
   `WorldEnvironment`) e rode com F5.
3. Na saída do editor, com *Verbose* ligado (ou `--verbose`), confira:
   - RX 580: preset `Balanced`, sem sombras por ray tracing.
   - RTX: preset `Cinematic` e sombras por ray tracing ligadas
     (`EretheaRayTracedShadows` aparece no `Compositor` do `WorldEnvironment`
     enquanto o jogo roda).

## 4. Upscalers (só com o 3D em menos de 100% da resolução)

1. No projeto do passo 3, *Rendering › Scaling 3D*: modo `FSR 2.2`, escala
   0,67.
2. RTX: com os arquivos do DLSS da NVIDIA ao lado do executável (veja
   "DLSS" no `erethea/README.md`) e o driver Direct3D 12, o automático usa o
   DLSS; sem eles, o AMD FSR.
3. RX 580: o automático usa o AMD FSR (FSR 3.1).
4. Ande pela cena: a imagem tem que ficar nítida, sem rastros fortes.

## 5. Ray tracing (só na RTX)

1. Abra a cena de referência: `erethea --path erethea/tests/reference_scene
   -- --rt=shadows` (a pasta vem no código-fonte do GitHub).
2. As sombras do sol ficam no mesmo lugar das do mapa de sombra, com bordas
   suaves longe do que as projeta e firmes onde encostam.
3. Anote o FPS com e sem `--rt=shadows`.
4. Na RX 580, o mesmo comando avisa uma vez que não há ray tracing e desenha
   com o mapa de sombra, sem erro.

## 6. Sombra de contato e pele (as duas placas)

1. Na cena do passo 3, acrescente `EretheaContactShadows` ao `Compositor` do
   `WorldEnvironment`: as coisas que encostam no chão ganham uma sombra curta e
   firme na base.
2. Num personagem com pele (material com *Subsurf Scatter* e *Skin Mode*), ligue
   `rendering/erethea/subsurface_scattering/follow_surface = 0.03`: não pode
   haver halo claro entre a mão e o rosto.

## 7. Terreno e cabelo

1. Rode `erethea/tests/terrain` e `erethea/tests/hair_strands` (no
   código-fonte): o terreno aparece com os dois materiais, e o cabelo, fio a
   fio, segue a cabeça quando ela gira.
2. Anote o FPS de cada um.

## 8. Modelo pesado

1. Rode `python erethea/tests/dense_geometry/test_dense_geometry.py
   <caminho do erethea.console.exe>` (precisa do Python). Ele gera uma paisagem
   de 2 milhões de triângulos, importa e mede.
2. O teste imprime `ERETHEA_DENSE_OK` e os tempos por quadro; anote os da
   placa.

## 9. Painel de IA

1. No editor, abra o painel de IA.
2. Com o Ignis rodando no PC: peça "crie um cubo vermelho na cena". O cubo
   aparece, e Ctrl+Z desfaz.
3. Com a chave da API do Claude: o mesmo pedido.
4. Com o Claude Code instalado e logado com a sua assinatura: o mesmo pedido.

## 10. Teste conjunto com a Fornalha 3D

1. Instale a Fornalha 3D de desktop e aponte para a Erethea (ela acha em
   `%LOCALAPPDATA%\Erethea`).
2. Peça por um prompt um jogo curto: um personagem que anda e ataca, um
   cenário com vila e floresta, um inimigo, efeitos, música e dublagem.
3. Abra o projeto na Erethea: as cenas se montam sozinhas, o personagem anda,
   a música troca no combate, a fala tem eco na caverna.
4. Na Galeria da Fornalha, a prévia do asset mostra o modelo com o
   renderizador da Erethea (pedido nº 9; só se a Fornalha já tiver feito).
5. Procure falhas e o que pode melhorar; anote tudo.

## 11. Atualização

1. Com uma versão nova publicada no GitHub, abra a Erethea: ela avisa da
   versão nova e atualiza sozinha, reabrindo o editor.

## O que mandar de volta

Uma lista com, para cada passo: placa, passou ou não, FPS quando o passo pede,
e o que achou feio ou lento. Pode colar no chat do Claude do jeito que estiver.
