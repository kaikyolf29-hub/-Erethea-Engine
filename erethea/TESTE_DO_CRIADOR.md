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
   - RX 580: preset `Balanced`, sem sombras por ray tracing, com a sombra de
     contato (`EretheaContactShadows` aparece no `Compositor` do
     `WorldEnvironment` enquanto o jogo roda).
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

## 6. Sombra de contato, pele e materiais do glTF (as duas placas)

1. Na RX 580, na cena do passo 3, as coisas que encostam no chão têm uma
   sombra curta e firme na base (a sombra de contato, ligada sozinha). Anote o
   FPS com ela e com `rendering/erethea/contact_shadows/sun` em `Disabled`.
2. Num personagem com pele (material com *Subsurf Scatter* e *Skin Mode*): o
   projeto novo já vem com `rendering/erethea/subsurface_scattering/follow_surface`
   em 0,03. Não pode haver halo claro entre a mão e o rosto; com 0, o halo
   aparece.
3. Importe um `.glb` da Fornalha com olhos e cabelo (ou um do Blender com
   *Coat* e *Anisotropic* no material): os olhos têm o brilho nítido da córnea
   e o reflexo do céu, e o cabelo o brilho esticado. Nas configurações do
   projeto (com *Advanced Settings* ligado), desligar
   `rendering/erethea/gltf/import_material_extensions` e reimportar tira os dois.

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

A Fornalha de 26/11 já tem a Erethea como engine padrão. Use a versão mais nova
dela (o roteiro da própria Fornalha, `TESTE_DO_CRIADOR.md` no repositório dela,
cobre as bancadas por dentro; aqui fica o que é da Erethea).

1. Instale a Fornalha 3D de desktop. Em **Máquina → Programas**, a Erethea
   aparece achada em `%LOCALAPPDATA%\Erethea`, com a versão.
2. Peça por um prompt um jogo curto: um personagem que anda e ataca, um mapa
   com vila, floresta e estrada (pela bancada de mapa), um inimigo, efeitos,
   música, dublagem e uma cena curta.
3. **Com a Erethea fechada**, mande montar (**Montar na engine**) e depois
   **Rodar teste**. **Esperado:** o relatório diz
   `fornalha + erethea --agent-test`, "passou" e 0 erros (os dois testes
   juntos: o da Fornalha anda com o jogador, o da Erethea mede desempenho e
   avisos).
4. Abra o projeto na Erethea e aperte F5. O mapa aparece com relevo, rio,
   estradas, vegetação e a vila; o personagem anda sem cair; a música troca no
   combate; a fala sai da cabeça de quem fala, com a boca mexendo; a cena curta
   toca com os personagens certos.
5. **Agora com a Erethea aberta no projeto**, peça na Fornalha uma mudança (um
   inimigo novo, ou gerar o mapa de novo) e monte. **Esperado:** sem fechar
   nada, a cena se atualiza sozinha no editor e o mapa continua lá. O relatório
   da montagem (no editor: *Projeto › Ferramentas › Build Scenes from
   game.project.json...*) diz que as cenas foram feitas pelo `montar.gd` da
   Fornalha. Se o mapa sumir ou o inimigo novo não aparecer, anote: é a ponte.
6. Confira que o projeto ganhou as configurações da Erethea (o `project.godot`
   passa a ter `erethea/quality_preset=4`, entre outras) e que o jogo roda com
   a escolha pela placa do passo 3: sombras por ray tracing na RTX, sombra de
   contato na RX 580, olhos e cabelo com o brilho do passo 6.
7. Abra a pasta do projeto: tem o `AGENTS.md` (e o `CLAUDE.md`) que a Fornalha
   grava para qualquer IA. Abra o Claude Code nessa pasta e peça uma mudança
   pequena no jogo; ele usa os dois servidores MCP (Fornalha na porta 11480,
   Erethea na 11481).
8. Na Galeria da Fornalha, a prévia do asset mostra o modelo com o
   renderizador da Erethea (pedido nº 9; só se a Fornalha já tiver feito).
9. Procure falhas e o que pode melhorar; anote tudo.

## 11. Atualização

1. Com uma versão nova publicada no GitHub (a 1.0.1 saiu em 26/11; se você
   instalou a 1.0.0, ela serve para isso), abra a Erethea: o gerenciador de
   projetos avisa da versão nova; clique no aviso, e ela baixa, instala por
   cima sem perguntas e reabre. Se falhar, mande o
   `%TEMP%\erethea-update.log`.

## O que mandar de volta

Uma lista com, para cada passo: placa, passou ou não, FPS quando o passo pede,
e o que achou feio ou lento. Pode colar no chat do Claude do jeito que estiver.
