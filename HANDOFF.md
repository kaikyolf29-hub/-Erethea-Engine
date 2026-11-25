# Erethea Engine: notas de passagem

Escrito em 22/11/2026 e atualizado a cada PR (última vez em 25/11/2026). Uma sessão nova
do Claude (Claude Code na web ou no computador) lê este arquivo, o `CLAUDE.md` e
o `erethea/README.md` e segue até a engine ficar pronta, sem precisar perguntar
nada ao Kaiky. As regras de trabalho estão no `CLAUDE.md`; aqui ficam o estado,
as decisões já tomadas e o passo a passo até o fim.

## Como começar (cole isto na conta nova)

Antes, conecte o GitHub na conta nova com um login que possa escrever no
repositório (`kaikyolf29-hub`, o dono, ou `kaikydev1-jpg`, colaborador) e abra
uma sessão do Claude Code no repositório `kaikyolf29-hub/-Erethea-Engine`.
Depois cole:

```text
Você vai continuar a Erethea Engine, o fork da Godot do repositório
kaikyolf29-hub/-Erethea-Engine. Leia primeiro o HANDOFF.md (na raiz do main;
se um PR aberto do Claude mexe no HANDOFF.md, o do branch dele é mais novo),
depois o CLAUDE.md e o erethea/README.md. Siga o passo a passo do HANDOFF.md a
partir de "O que fazer agora", na ordem, até a última fase e o lançamento.
Trabalhe sozinho: um PR pequeno por tarefa, testes passando antes do merge, e
você mesmo faz o merge no main quando o CI inteiro passar (nunca com o CI
vermelho). Não me peça para testar versões intermediárias: eu só testo o
instalador final. Só me chame para decisões que mudam o plano ou para coisas
que só eu posso fazer (contas, pagamentos, placa de vídeo). Fale comigo em
português do Brasil. Ao terminar cada PR, atualize o HANDOFF.md com o que
mudou.
```

Na conta nova, as sessões do Claude Code desta conta (inclusive as sessões
Sonnet que ajudavam em paralelo) não existem: os PRs abertos ficam no GitHub,
e quem continua termina cada um (veja "Estado em 25/11", logo abaixo).

## Estado em 25/11 (leia antes de tudo)

O trabalho voltou em 25/11 numa conta nova (a anterior é a da Fornalha 3D).
Nenhuma sessão Sonnet está rodando.

- **No `main`**: #25 (WAV da dublagem e eco por área), #26 (janela moderna nos
  jogos), #27 (canal de testes da atualização), #28 e #30 (teste do instalador
  e diagnóstico do crash do editor no CI), #29 (música que muda no fim do
  compasso), #32 (raios de luz `EretheaLightShafts`), #33 (HANDOFF para outra
  conta) e, desde 25/11, **#34 (sombras do sol por ray tracing)**, com o CI
  inteiro verde.
- **PR #31 (prévia com LibGodot), causa achada e corrigida, esperando o CI**
  (branch `claude/erethea-libgodot-preview`). O programa de exemplo saía com
  **0xE0464645** no Windows depois de destruir a instância e descarregar a
  DLL. Esse é o código do "fail fast" do **CoreMessaging** do Windows (não há
  texto oficial da Microsoft; relatos: o PR #1532 do `momo5502/sogen`, que
  mostra o CoreMessaging encerrando o processo com esse código, e o DWM, que
  sai com ele). Um fail fast não passa pelos manipuladores de exceção, por isso
  o diagnóstico do programa não imprimiu nada. A engine cria um
  `DispatcherQueueController` do CoreMessaging na thread principal
  (`WinRTUtils::create_queue`, para as notificações). Na LibGodot do Windows,
  diferente do `main()` da engine: (1) o controlador ficava num `ComPtr`
  global depois de desligado e só era solto no destrutor estático, dentro do
  `FreeLibrary`, com a trava do loader presa; (2) o `OS_Windows` nunca era
  destruído, então `CoUninitialize` e `WinRTUtils::cleanup` não rodavam.
  Correção no commit `a0db15db` (os dois pontos, com `// Erethea:`; o mesmo
  defeito está na Godot, vale propor), e o job agora imprime os eventos
  1000/1001/1026 do Windows quando o programa falha (`58706aa3`). Se o CI
  passar inteiro, faça o merge. Se o job "LibGodot on Windows" falhar de novo,
  leia os eventos impressos no log (módulo e deslocamento) e continue daí.
- **Depois do #31**: fechar a Fase 3 (passo 5) e seguir a Fase 4, item 2,
  "Próximo PR: personagens com esqueleto".
- **Falhas conhecidas do CI** ainda abertas: o travamento do passo "Update
  silently" do instalador (experimento 2 no `main` desde o #30; conte as
  execuções) e o crash 0xC0000409 do editor no teste da janela (diagnóstico
  pronto; espere acontecer de novo e leia o evento/minidump). Detalhes em
  "Falhas conhecidas do CI".
- **Lições**: o build local sem `dev_mode=yes` não pega avisos que o CI trata
  como erro; compile os objetos mudados com `dev_mode=yes`. Arquivos só do
  Windows também dá para compilar aqui com o MinGW (veja "Como compilar e
  testar").

## O que é a Erethea

- Fork da Godot com nome próprio, janela moderna no Windows, visual melhor e
  ponte com a Fornalha (a "Fábrica de jogos" do Plano 1). Licença MIT.
- Repositório: `kaikyolf29-hub/-Erethea-Engine` (o nome começa com hífen).
  `main` é a engine; `upstream` espelha a Godot (nunca faça commit nele).
- Base: Godot 4.8-beta4, commit `b070e63b161aa816afac71dcfc9d60859d38f220`
  (17/11/2026). Quando a 4.8 estável sair, junte como em `erethea/README.md`,
  "Atualizar a Godot".
- Versão da Erethea: numeração própria, `0.x` até todas as fases ficarem
  prontas. Releases saem por tag `v<versão>` (`erethea/README.md`, "Lançar uma
  versão").
- O Kaiky escreve em português do Brasil. Código, comentários e commits em
  inglês no estilo da Godot; PRs e documentação da Erethea em português.

## Decisões já tomadas (não pergunte de novo)

| Tema | Decisão |
| --- | --- |
| Modo de trabalho | Autônomo, fase por fase. O Kaiky autorizou (20/11) o merge no `main` por você quando o CI passa. Ele só testa o instalador final. |
| Sistema | Windows primeiro (janela, instalador, testes); Linux tem que continuar compilando. |
| Placas | Não diga que algo funciona numa placa sem prova nela. Referência: RX 580 8 GB (sem ray tracing) e RTX 4060 Ti ou 5060 Ti 16 GB. O CI não tem placa: o que depender de placa fica "não testado na placa" até o teste final do Kaiky. |
| Compatibilidade | Projetos da Godot abrem como estão. Recurso novo que muda comportamento entra desligado, ou só vale em projetos da Fornalha (com `game.project.json`). |
| Visual do editor | O Kaiky quer o editor quase igual ao da Godot: melhorar, sem mudar o jeito de usar. |
| Marca | Só por `erethea/tools/rebrand.py` (confira com `--check`). Logo final: o selo medieval (lua crescente em "E" com espada), já aplicado no app, instalador, splash e gerenciador de projetos (PR #17). Originais em `erethea/branding/source/`. |
| Streaming de texturas | Ligado por padrão em projetos novos só para texturas 3D; 2D e interface ficam como na Godot. |
| DLSS | Pelo Streamline no Direct3D 12; quem desenvolve baixa as DLLs da NVIDIA (licença), o CI testa o caminho sem DLL. |
| Assinatura do instalador | Sai sem assinatura por enquanto. No lançamento o Kaiky paga a assinatura (Azure Artifact Signing, ~US$ 10/mês; a Microsoft pode não aceitar pessoa do Brasil, então conferir; alternativas: SignPath Foundation, grátis para código aberto, ou certificado open source da Certum). Os passos de assinatura já estão no workflow de release, desligados até existir a variável `ARTIFACT_SIGNING_ENDPOINT` (`erethea/README.md`, "Assinatura do instalador"). |
| Execuções longas | Builds completos e suítes longas rodam no GitHub Actions (o repositório é público, os minutos são grátis): suba o branch e leia os checks. |
| Painel de IA | Quatro provedores: Ignis (padrão), Claude pela API, servidor OpenAI e, decidido pelo Kaiky em 22/11, o **Claude Code instalado no PC** (usa a assinatura dele, sem chave de API). |
| Erethea completa a Fornalha | A Erethea não só roda o que a Fornalha 3D gera: usa as ferramentas da engine para deixar o resultado melhor (visual, som, animação). Os pedidos da Fornalha para a Erethea entram nas fases abaixo. |
| Som | Bloco de som decidido em 22/11 (passo 3b): WAV da dublagem sem travar, eco por área, música que muda com o jogo e boca mexendo com a fala. |
| Teste conjunto | O CI da Fornalha testa com a Godot 4.6.1 enquanto a Erethea não fica pronta. No fim, o Kaiky instala a Erethea e a Fornalha 3D (versões de desktop) e pede as gerações por um prompt no PC dele; a qualidade da Fornalha é provada já ligada à Erethea. |
| Em aberto | Juntar a Fornalha dentro da Erethea no futuro ou manter dois apps ligados (a LibGodot deixa os dois caminhos abertos). Não precisa decidir agora. |
| Não duplicar a Fornalha (23/11) | Antes de cada recurso, confira no repositório da Fornalha (`kaikyolf29-hub/fornalha`, só leitura) se ela já faz. Ela já faz no script do jogo: a boca mexendo com a fala, piscar e olhar (`movimento.gd`), cabelo em cards com brilho anisotrópico (`cabelo.gdshader`), grama com MultiMesh e vento (`principal.gd`). A Erethea soma o que é de engine: renderização, desempenho, ferramentas. Por isso o item "boca com a fala" do som saiu do plano. |
| Trabalho em paralelo (23/11) | O Kaiky pediu para usar sessões mais baratas (Sonnet) em paralelo, com a sessão principal (Opus) coordenando e revisando. No máximo duas sessões Sonnet ao mesmo tempo além da principal: mais que isso esgota o limite de 5 horas da conta dele. Veja "Como trabalhar com sessões em paralelo". |
| Ordem do fim (22/11) | Lançamento (instalador final e o teste do Kaiky no PC, com a Fornalha 3D) **antes** de juntar a Godot 4.8 estável, que é o último passo. |

## Onde cada fase está

As fases e o "pronto quando" de cada uma vêm do Plano 2.

| Fase | Pronto quando | Estado |
| --- | --- | --- |
| 0 · Fork e marca | O instalador com o nome novo abre um projeto da Godot sem erro | Pronta (PR #3) |
| 1 · Janela nativa | O editor não tem mais a barra do sistema e encaixa nas bordas do Windows 11 | Pronta (PR #4 barra de título, PR #5 atualização automática; extras: janela moderna nos jogos #26 e canal de testes da atualização #27). Falta só a assinatura (no lançamento) |
| 2 · Visual rápido | A cena de referência fica visivelmente melhor que na Godot padrão, no mesmo FPS | Pronta (PRs #6 a #14: cena de referência, presets, pós-processamento, AMD FSR 3.1/4, XeSS, DLSS, streaming de texturas) |
| 3 · Ponte com a Fábrica | Um personagem vindo do Plano 1 aparece na cena sozinho e anda | Quase pronta: MCP (#15), validações (#16), logo (#17), importador do `game.project.json` (#18), elo ao vivo (#19), modo de teste (#20), painel de IA (#22, #24) e o bloco de som (#25, #29; a boca é da Fornalha) no `main`. Falta a prévia com LibGodot (PR #31) e fechar a fase (passo 5) |
| 4 · Visual avançado | A cena roda com ray tracing numa placa RTX e sem ele numa placa sem suporte | Em andamento: raios de luz (#32) e sombras do sol por ray tracing (#34) no `main`; próximo: sombras dos personagens com esqueleto (item 2) |
| 5 · Geometria densa | Um modelo de milhões de polígonos roda sem LOD feito à mão | Não começou |

## O que fazer agora (na ordem)

### 1 a 3. Elo ao vivo, modo de teste e painel de IA: feitos

PRs #19, #20 e #22 no `main`. O painel de IA está em `modules/erethea_assistant/`
e em `erethea/README.md`, "Painel de IA". O teste com o Ignis de verdade e com a
API do Claude de verdade fica para o teste final do Kaiky.

Depois do #22, o PR #23 fez o teste do instalador parar em 4 minutos no passo
"Update silently" (que travou duas vezes até o limite do job) e guardar o log do
instalador (veja "Falhas conhecidas do CI"), e trouxe este plano.

### 3a. Quarta opção do painel: Claude Code com a assinatura (PR #24, feito)

Decidido pelo Kaiky em 22/11 e no `main` pelo PR #24. O
provedor "Claude Code (your subscription)" chama o Claude Code instalado no PC,
que usa a assinatura do Claude, sem chave de API e sem Node.

- Como roda (conferido na documentação oficial e no pacote do Claude Agent
  SDK): um processo por conversa, `claude -p --input-format stream-json
  --output-format stream-json --verbose`, mensagens `{"type":"user","message":
  {"role":"user","content":...},"parent_tool_use_id":null}` na entrada e as
  linhas `system/init`, `assistant` e `result` na saída. Só o servidor MCP do
  editor (`--mcp-config` com um arquivo no cache do editor,
  `--strict-mcp-config`), sem as ferramentas próprias (`--tools=`), com as do
  editor liberadas (`--allowedTools mcp__erethea`) e o resto recusado
  (`--permission-mode dontAsk`), `--max-turns 30`. Se o processo fechar, a
  próxima pergunta abre outro com `--resume <session_id>`. Não use `--bare`
  (ele ignora o login da assinatura). A `ANTHROPIC_API_KEY` é tirada do
  ambiente só na hora de abrir o processo.
- Canos com espera (`execute_with_pipe(..., true)`): no Windows, um cano sem
  espera pode aceitar só parte de uma mensagem. A leitura só pega o que já
  chegou (`get_length()`), então não trava o editor. A Erethea não escapa aspas
  dentro de argumentos no Windows: por isso a pergunta vai pela entrada, o
  `--mcp-config` é um arquivo e o prompt de sistema vai sem aspas duplas.
- O servidor MCP liga sozinho (`EretheaMcpPlugin::ensure_server`) e o painel
  acompanha as ferramentas que chegam por ele
  (`set_server_call_observer`), para o desfazer funcionar.
- Teste: `erethea/tests/assistant/fake_claude.py` faz o papel do Claude Code
  (mesmo formato, age pelo MCP); o `test_assistant.py` confere linha de
  comando, chave fora do ambiente, segunda pergunta no mesmo processo, volta na
  mesma sessão e desfazer.
- O teste com o Claude Code de verdade fica para o teste final do Kaiky.

### 3b. Bloco de som (decidido em 22/11)

A Fornalha gera efeitos (`Audio/efeitos/<evento>_<n>.wav`), música por situação
(`Audio/musica/<situacao>.wav`: exploracao, vila, floresta, combate, chefe,
masmorra) e dublagem (`Audio/dublagem/<id>_<nn>.wav` com
`<id>_<nn>.boca.json`: `{"quadros_por_segundo": 30, "abertura": [...]}`). Um PR
por item, só em projetos da Fornalha ou desligado por padrão:

1. **WAV da dublagem sem travar** (PR #25): o leitor de WAV da Godot
   (`scene/resources/audio/audio_stream_wav.cpp`, trechos `// Erethea:`)
   aceita o cabeçalho de stream do Ignis (tamanhos do RIFF e do `data` em 0
   ou 0xFFFFFFFF: o som vai até o fim do arquivo) e calcula o próximo bloco em
   64 bits (em 32 bits, um tamanho perto de 0xFFFFFFFF voltava ao mesmo bloco e
   lia o som como cabeçalho, com milhares de erros). Teste em
   `tests/erethea/test_erethea_wav_stream.cpp`. Correção genérica: falta
   propor à Godot (issue e PR no `godotengine/godot`, quando o Kaiky quiser
   abrir com a conta dele).
2. **Eco por área** (PR #25): o `build_fornalha_scenes` dá a cada área um
   `Area3D` "Eco" que manda os sons para um barramento com reverberação pelo
   tipo (seis acústicas: caverna, salao, ruinas, vila, floresta, montanha) ou
   pelo campo opcional `acustica`; os barramentos que faltam entram no
   `default_bus_layout.tres`. Desliga em `editor/erethea_fornalha/area_acoustics`.
   Veja `erethea/README.md`, "Som".
3. **Música que muda com o jogo** (PR #29, feito): o `AudioStreamWAV` ganhou
   `bpm`, `beat_count` e `bar_beats` (como o Ogg; correção genérica a propor à
   Godot, teste em `tests/erethea/test_erethea_wav_bpm.cpp`), e o
   `build_fornalha_scenes` acerta
   o `.import` de cada `Audio/musica/<situacao>.wav` (laço, bpm, compassos × 4)
   e põe na `principal.tscn` o nó `Musica` do grupo `erethea_musica`
   (`AudioStreamInteractive`, troca no fim do compasso) que o `Sons` da
   Fornalha chama. Desliga em `editor/erethea_fornalha/interactive_music`.
   Falta ouvir de verdade (o CI não toca som). Veja `erethea/README.md`, "Som".
4. **Boca mexendo com a fala**: já feito pela Fornalha, sem nada a fazer na
   Erethea (decidido em 23/11). O `Sons.falar()` dela toca a fala presa ao
   personagem e passa a curva do `.boca.json` ao `Rosto` do `movimento.gd`, que
   gira o osso `jaw` (e ainda pisca e mexe os olhos). A Erethea soma o eco da
   área à fala (item 2). Se um dia a Fornalha mandar modelos com forma de boca
   (blend shape) em vez de osso, aí sim cabe um complemento aqui.

Com isso o bloco de som está fechado. A Fornalha atendeu os pedidos 1 a 4
(`.musica.json`, grupo `erethea_musica`, `Sons.falar` com `personagem` e
`texto` no `.boca.json`, osso da boca no `forja.json`).

### 4. Prévia com LibGodot (último item da Fase 3, PR #31)

**Estado (25/11): PR #31 aberto**, revisado; a saída 0xE0464645 no Windows foi
corrigida, falta o CI verde (veja "Estado em 25/11"). O que ele traz: `modules/erethea_preview/` (`EretheaPreview` e
`EretheaPreviewLoop`), `--erethea-preview=<glb>` com captura, troca de asset
por arquivo de controle, `erethea/tests/preview/` e `erethea/tests/libgodot/`
(programa C que carrega a biblioteca), workflow `erethea_preview.yml` (os jobs
que compilam a biblioteca só rodam quando os arquivos ligados a ela mudam), e
o pedido nº 9 para a Fornalha. O plano original segue para referência:

Plano 2: "A LibGodot, que chegou na 4.6, permite embutir a engine em outro
programa. A Fornalha pode mostrar o asset já com o renderizador da engine, do
jeito que vai ficar no jogo."

Feito neste PR (documentado em `erethea/README.md`, "Prévia para a Fornalha"):

1. **LibGodot na base 4.8**: `scons ... library_type=shared_library` gera a
   biblioteca (`bin/liberethea.linuxbsd.editor.x86_64.so`, `.dll` no Windows).
   A API C é `core/extension/libgodot.h` (`libgodot_create_godot_instance` e
   `libgodot_destroy_godot_instance`) e a instância é um `GodotInstance` com
   `start()` e `iteration()`, chamados pela API GDExtension. Nada foi mudado.
2. **`modules/erethea_preview/`**: `EretheaPreview` (cena: `.glb`/`.gltf` lido
   em tempo de execução, câmera enquadrada pelo AABB que gira, chão, sol com
   sombra, céu procedural) e `EretheaPreviewLoop` (laço principal sem
   projeto). Entra por `--erethea-preview=<arquivo>`; a única mudança em
   arquivo da Godot é `main/main.cpp` (marcada `// Erethea:`). Opções:
   `--erethea-preview-capture=`, `-frames=`, `-control=` (troca de asset sem
   reiniciar), `-quality=`. Sem os argumentos, nada muda.
3. **Testes**: `erethea/tests/preview/test_preview.py` (Linux e Windows) e
   `erethea/tests/libgodot/` (programa C + teste), no `erethea_preview.yml`.
4. **Pedido para a Fornalha** nº 9 (abaixo).

Limites: uma instância por processo; sem placa de vídeo no CI (lavapipe e
ANGLE/WARP); a biblioteca e o executável da prévia ainda não entram no
instalador nem no release (só artefatos do CI): falta decidir com o Kaiky
se o instalador leva a DLL. Fora do escopo: desenhar dentro de um widget da
Fornalha (a LibGodot abre a própria janela).

### 5. Fechar a Fase 3

- Confira o "pronto quando": num projeto da Fornalha recém-gerado, abrir na
  Erethea monta as cenas sozinho e o personagem anda (o teste
  `erethea/tests/fornalha_link/test_fornalha_link.py` e o `run_test` provam
  isso no CI; a prova com a Fornalha de verdade fica para o teste final).
- Os pedidos para a Fornalha (abaixo) foram mandados ao Kaiky em 22/11. Mande
  de novo só o que mudar.

### 6. Extras pendentes das Fases 1 e 2 (pequenos, opcionais)

- ~~Janela moderna também nos jogos~~ (pronto no PR da tarefa): configuração
  de projeto `display/window/erethea/modern_title_bar`, desligada por padrão;
  o jogo que a liga desenha a própria barra. Documentada em `erethea/README.md`
  ("Janela moderna e barra de título") e testada no `erethea_window.yml`.
- ~~Canal de testes da atualização~~ (PR #27): configuração do editor
  `erethea/update/prereleases`, desligada por padrão.
- Geração de quadros (FSR 3.1 e DLSS): o plano cita "upscaling e geração de
  quadros"; hoje só há upscaling.
- DLSS no Vulkan (hoje só Direct3D 12). Atenção: o Streamline da NVIDIA, no
  Vulkan, precisa interceptar a criação da instância e do dispositivo Vulkan
  (o `sl.interposer` no lugar do carregador, ou `slSetVulkanInfo` com as
  extensões e recursos que ele pede), o que mexe no driver Vulkan da Godot
  (`drivers/vulkan/`). Não é tarefa para sessão barata e não dá para provar sem
  placa NVIDIA: faça com cuidado, desligado por padrão, e deixe a prova para o
  teste final.
- Compressão automática na importação dos assets da Fornalha (conferir se o
  importador da Fase 3 já liga a compressão VRAM certa).

### 7. Fase 4 · Visual avançado

Pronto quando: a cena de referência roda com ray tracing numa placa RTX e sem
ele numa placa sem suporte. Um PR por item, cada um desligado por padrão e
comparado na cena de referência:

1. **Escolha por placa** ("melhor resultado em cada placa"): no primeiro uso,
   medir a placa (fabricante, VRAM, suporte a ray tracing pelo
   `RenderingDevice`) e escolher o caminho de iluminação e o upscaler: path
   tracing da NVIDIA numa RTX forte, ray tracing da Godot em AMD/Intel com
   suporte, SDFGI e lightmaps sem ray tracing (RX 580). O dev pode travar a
   escolha no projeto; o jogo exportado faz a mesma escolha no PC de quem joga.
   Estenda o preset "Automatic" de `modules/erethea_quality/`.

   **Feito (PR da escolha por placa, 25/11)**, sem mexer no núcleo:
   - `rendering/erethea/temporal_upscaler` ganhou `Automatic` (valor 4):
     DLSS na NVIDIA e XeSS na Intel quando podem rodar, AMD FSR nas outras e
     quando aqueles não podem, FSR 2.2 onde nenhum roda. A cada quadro usa o
     primeiro que consegue e solta os outros
     (`EretheaUpscaler::get_automatic_order`, testado por fabricante).
   - `rendering/erethea/ray_tracing/sun_shadows` (`Disabled`, `Automatic` =
     padrão, `Enabled`): põe o `EretheaRayTracedShadows` no `Compositor` de
     cada `WorldEnvironment` (criado se não houver) e das câmeras com
     compositor próprio, só no jogo rodando. `Automatic` só liga quando o
     preset de qualidade é `Automatic` e escolhe `Cinematic` (placa dedicada
     com ray tracing); projetos da Godot (preset `Disabled`) não mudam.
     Teste: `erethea/tests/raytracing/auto_project` no CI (lavapipe); provado
     que falha com a configuração desligada.
   - Projetos novos da Erethea começam com o upscaler `Automatic` (além do
     preset `Automatic` que já tinham).
   - Falta: o path tracing da NVIDIA (item 3) entra na linha das RTX quando
     existir; a prova na RX 580 e na RTX é do teste final. VRAM não entrou
     na escolha (o preset já separa integrada de dedicada); se o teste final
     mostrar placa dedicada fraca sofrendo no Cinematic, use
     `RenderingDevice::get_memory_usage`/limites do driver para descer um nível.
2. **Ray tracing da Godot**: a 4.7 trouxe a base de ray tracing no
   `RenderingDevice` (só a "tubulação"). Montar as estruturas de aceleração da
   cena e fazer, nesta ordem, sombras suaves, reflexos e iluminação global por
   ray tracing, como opção em `servers/rendering/renderer_rd/` ou num módulo.
   Confira se o lavapipe do runner Linux e o WARP do Windows (DXR) têm ray
   tracing; se tiverem, o CI prova a imagem; se não, prova só o caminho sem
   ray tracing.

   **Estado (23/11): sombras do sol num PR aberto** (branch
   `claude/happy-dijkstra-gueto9`). Testado no lavapipe do contêiner: numa
   cena simples (`erethea/tests/raytracing/shadows_scene`) as sombras caem no
   lugar certo, presas ao que encosta no chão e suaves longe dele; na cena de
   referência, com `--rt=shadows`, ficam no mesmo lugar das do mapa de sombra
   (RMSE 2,5 a 3,5 de dia, só as bordas mudam). O que já está provado e
   decidido:
   - A base 4.8 tem a tubulação (experimental, Vulkan): `blas_create`/
     `blas_build`, `tlas_create`/`tlas_build`, pipelines de ray tracing,
     `hit_sbt_*`, `raytracing_list_*` e os recursos `SUPPORTS_RAY_QUERY` e
     `SUPPORTS_RAYTRACING_PIPELINE`. O D3D12 com ray tracing ainda é um PR
     aberto na Godot (#118654). A Godot não tem nenhum recurso de
     renderização que use isso (nem sombras, nem reflexos, nem GI): a Erethea
     é a primeira.
   - O lavapipe do Ubuntu 24.04 (Mesa 25.2, o mesmo do CI) tem ray query e
     pipeline de ray tracing. No contêiner do Claude Code dá para testar:
     `sudo apt-get install -y mesa-vulkan-drivers vulkan-tools` e rodar com
     `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json xvfb-run -a -s
     "-screen 0 1280x720x24" bin/erethea... --path <projeto> --audio-driver
     Dummy`. Um teste em GDScript (BLAS de um triângulo, TLAS, compute com
     `#extension GL_EXT_ray_query : require` compilado pela Godot) deu os
     acertos certos.
   - Pegadinha da API: o `tlas_build` recusa instância com `hit_sbt_range == 0`
     mesmo quando só se usa ray query (sem tabela de shaders). Use
     `1 << 32` (um grupo, a partir do 0). Vale propor à Godot aceitar 0.
   - Desenho: um `CompositorEffect` (`EretheaRayTracedShadows`, módulo
     `modules/erethea_raytracing/`), como os efeitos de `erethea_post`: quem
     quer adiciona ao `Compositor`, então fica desligado por padrão e não custa
     nada a quem não usa. Ele roda em `PRE_OPAQUE` (depois da passada de
     profundidade do Forward+), lança, de cada pixel, raios até o primeiro sol
     (cone do tamanho do disco solar, `samples` raios) e grava a máscara
     `erethea_rt`/`shadow_mask` (R8) nos buffers de renderização.
   - Mudanças no núcleo (pequenas, todas com `// Erethea:`):
     `RendererSceneRender` ganhou o pedido do efeito
     (`erethea_request_ray_tracing(raio)`, válido no quadro e no seguinte) e a
     lista de geometria; o `RendererSceneCull::_render_scene` junta, só quando
     há pedido, a geometria num raio em volta da câmera (visível ou não, para
     o que está fora da tela ainda fazer sombra) e entrega antes do
     `render_scene`; o Forward+ liga a flag
     `SCENE_DATA_FLAGS_USE_ERETHEA_RT_SHADOWS` quando há pedido e a máscara
     existe, põe a máscara no binding 39 do conjunto 1 (branca quando não há)
     e aplica `min(sombra, máscara)` na primeira luz direcional, depois da
     opacidade da sombra (funciona com o mapa de sombra da luz desligado).
   - No módulo: BLAS por malha (posições lidas uma vez pelo
     `mesh_create_arrays_from_surface_data`, em float 32), TLAS por quadro com
     as malhas sem esqueleto e as instâncias das MultiMesh (transformações
     lidas do buffer uma vez por segundo, filtradas pelo raio), normal tirada
     da profundidade, viés pela distância. Superfície de costas para o sol
     (pela normal tirada da profundidade) fica com a máscara em 1 e a
     iluminação cuida: escurecer ali fazia degraus nas faces de objetos low
     poly (o problema do terminador).
   - **Depois deste PR**: personagens com esqueleto (BLAS atualizada pela
     pose) e partículas; reflexos e GI pelo mesmo caminho; a escolha por placa
     (item 1) passa a ligar o efeito em placas com ray query; a prova de
     velocidade na RTX fica para o teste final.
   - **Próximo PR: personagens com esqueleto** (os da Fornalha são todos
     assim). Já estudado em 23/11:
     - Os vértices deformados pelo esqueleto (e pelas blend shapes) ficam em
       `MeshStorage::MeshInstance::Surface::vertex_buffer[current_buffer]`,
       gravados pelo compute `shaders/skeleton.glsl` em
       `update_mesh_instances()`, no começo do quadro (antes do `PRE_OPAQUE`).
       No 3D, as posições vêm primeiro, em float 32 (`index * vertex_stride`
       palavras, normalmente 3), depois as normais e tangentes.
     - Esse buffer é criado em `_mesh_instance_add_surface_buffer`
       (`mesh_storage.cpp`) só com `BUFFER_CREATION_AS_STORAGE_BIT`. Mudança
       de uma linha no núcleo (`// Erethea:`): acrescentar
       `BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT`
       quando o `RenderingDevice` tem ray query. E uma função pública na
       `MeshStorage` (`// Erethea:`) que devolve o RID desse buffer, a contagem
       e o passo dos vértices para (mesh instance, superfície), ou `RID()` se a
       superfície não tem buffer próprio (`uniform_set[0]` nulo).
     - `RenderGeometryInstanceBase::mesh_instance` é público: o módulo sabe
       qual instância da lista do cull tem esqueleto. Os índices saem do buffer
       próprio do módulo (lidos uma vez por malha, como hoje).
     - BLAS por mesh instance, refeita todo quadro com `blas_build` (a API do
       RD não tem atualização parcial). O `current_buffer` alterna quando há
       vetores de movimento (TAA, FSR): guarde até duas BLAS por mesh instance,
       pela RID do buffer, e recrie se a RID mudar. O grafo do RD cuida das
       barreiras entre o compute do esqueleto e a montagem da BLAS.
     - As posições estão no espaço da malha: a transformação da instância vai
       na TLAS, como nas malhas paradas.
     - Teste: na `erethea/tests/raytracing/shadows_scene`, uma malha com
       esqueleto montada em GDScript (ArrayMesh com ossos e pesos e um
       `Skeleton3D` girando um osso) e a sombra seguindo a pose.
3. **Path tracing do fork RTX da NVIDIA** (MIT, lançado na GDC 2026): estudar e
   trazer como opção só para RTX, com a mesma configuração.
4. **Sombras**: cascatas melhores e sombras suaves por ray tracing (do item 2).
   **Cascatas (PR do sol do mundo, 25/11):** no `World/mundo.tscn` que a
   Erethea monta para a Fornalha, o sol mistura as cascatas
   (`blend_splits`, sem a emenda visível) e alcança o mundo inteiro
   (`get_sun_shadow_distance`: o dobro da borda de área mais longe, de 100 a
   300 m). Ficou de fora o PCSS (`light_angular_distance`): custo alto na
   RX 580 sem prova; se o teste final pedir sombra mais suave sem ray tracing,
   ligue pelo preset `Cinematic` só.
5. **Pele: feito o que faltava (PR da pele, 25/11)**. A Godot já tem o perfil
   por canal (modo pele) e o mapa de espessura (transmitância com textura); a
   Erethea somou `rendering/erethea/subsurface_scattering/follow_surface`
   (0 = filtro da Godot): o filtro deixa de vazar luz entre peles em
   profundidades diferentes (Jimenez, "follow surface"; trechos `// Erethea:`
   no `subsurface_scattering.glsl`, `ss_effects.*` e `rendering_server.cpp`).
   Teste `erethea/tests/sss_scene` nos dois sentidos no CI. O pedido para a
   Fornalha (ligar no projeto e usar a transmitância) está no README, "Pele".
   **Materiais em camadas** (terra, sujeira, molhado), pele com subsuperfície
   melhor (perfil de espalhamento por canal, mapa de espessura e sombra de
   contato), cabelo (incluindo **fio a fio**: fitas viradas para a câmera ou
   curvas, alfa por hash e brilho anisotrópico, para a Fornalha mandar o
   penteado direto), tecido e olhos: recursos de material num módulo
   (`modules/erethea_materials/`), com exemplos na cena de referência. Pele e
   cabelo fio a fio são pedidos da Fornalha.
6. **Terreno nativo** com pintura de materiais, criado a partir do blockout do
   Plano 1 (`World/blockout.glb` e as áreas do `game.project.json`): malha por
   clipmap, colisão `HeightMapShape3D`, pintura no editor. O Terrain3D (MIT)
   serve de referência; a Fornalha pede compatibilidade com ele (32 materiais,
   detiling, triplanar).
7. **Vegetação**: espalhador com LOD, impostores e vento (MultiMesh,
   `visibility_range`, sombreador de vento).
8. **Animação**: retarget automático para o esqueleto padrão do Plano 1 (a
   Fornalha passou a usar o esqueleto de jogo do MakeHuman com nomes da Unreal;
   confira no repositório dela) e locomoção com o MotionBricks da NVIDIA
   (conferir a licença antes).
9. **Efeitos de compositor** prontos: névoa de altura e raios de luz (pedido da
   Fornalha). **Raios de luz: feito no PR #32** (`EretheaLightShafts` em
   `modules/erethea_post/`, `--post=shafts` e `--sun=front` na cena de
   referência, passo próprio no CI). **Névoa de altura: não precisa de código**,
   é a do `Environment` da Godot (`fog_enabled` + `fog_height_density`),
   documentada no README. Falta a prova de velocidade na RX 580 (lista do teste
   final).
10. Teste de pronto no CI com o que der (WARP/lavapipe); a prova na RTX e na
    RX 580 entra na lista do teste final do Kaiky.

### 8. Fase 5 · Geometria densa

Pronto quando: um modelo de milhões de polígonos roda sem LOD feito à mão.

1. Na importação, quebrar a malha em meshlets e montar a hierarquia de
   simplificação (o `meshoptimizer` já está em `thirdparty/`, com
   `meshopt_buildMeshlets` e `meshopt_simplify`).
2. Seleção e corte por cluster na placa (compute shader e desenho indireto; o
   `RenderingDevice` da Godot não tem mesh shaders, conferir na base actual).
3. Streaming das páginas de clusters do disco conforme a câmera.
4. Opção por malha (desligada por padrão), comparação na cena de referência
   e um teste com um modelo de milhões de polígonos medindo FPS e memória.
   É também o pedido da Fornalha de "geometria virtual" para cenário denso
   (castelos, vilas).

### 9. Lançamento

1. O Kaiky confere o nome no INPI e o domínio (só ele pode).
2. Assinatura do instalador (o Kaiky paga e cria a conta; você liga a variável
   e confere o workflow).
3. `erethea/TESTE_DO_CRIADOR.md`: lista curta do que o Kaiky testa no PC dele
   (instalador, abrir projeto da Godot, barra de título, presets, upscalers na
   RTX, ray tracing na RTX e sem ele na RX 580, Fornalha ligada, painel de IA
   com o Ignis, com a API e com o Claude Code, modelo pesado). Inclui o
   **teste conjunto**: o Kaiky instala a Erethea e a Fornalha 3D de desktop,
   pede por um prompt um jogo (personagem, movimento, cenário, efeitos,
   música, dublagem), e a Erethea abre, monta as cenas e roda o jogo; ele
   procura falhas e o que melhorar.
4. Tag `v1.0.0` (ou a versão que o Kaiky quiser) e o release com o
   instalador. Só então avise o Kaiky para baixar e testar.

### 10. Godot 4.8 estável (depois do lançamento)

Decidido pelo Kaiky em 22/11: vem depois do lançamento. Quando a Godot
lançar a 4.8 estável, junte no `upstream` e no `main` como em
`erethea/README.md` ("Atualizar a Godot"), rode `rebrand.py --check`, troque o
`GODOT_BASE_VERSION` do workflow da cena de referência e deixe o CI inteiro
passar.

## Pedidos para a Fornalha (o Kaiky cola no chat da Fornalha)

Mandados ao Kaiky em 22/11 (os itens 1 a 5 são do bloco de som).

```text
Pedidos da Erethea Engine para a Fornalha 3D (complemento; se preferir outro formato, a Erethea se adapta).

SOM: a Erethea vai cuidar do eco de cada área, da música que muda com o jogo e da boca mexendo com a fala. Para isso, precisa destes dados:

1. Música: junto de cada Audio/musica/<situacao>.wav, gravar Audio/musica/<situacao>.musica.json com {"situacao", "bpm", "compassos", "tonica", "loop": true}. Com isso a Erethea marca o laço certo e troca de música no fim do compasso.

2. Troca de música num lugar só: o jogo pede a música pela situação (Sons.tocar_musica("combate")), não pelo caminho do arquivo. Quando existirem nós no grupo "erethea_musica", o Sons só chama get_tree().call_group("erethea_musica", "tocar", situacao) e deixa a Erethea fazer a transição. Sem esse grupo (na Godot comum), toca como hoje.

3. Fala: no <id>_<nn>.boca.json, acrescentar "personagem": "<id de quem fala>" e "texto": "<a frase>". No jogo, uma função Sons.falar(personagem, arquivo) que toca a fala presa ao personagem (AudioStreamPlayer3D filho dele). Hoje a dublagem é gerada, mas o jogo não toca.

4. Boca no modelo: cada personagem precisa de um osso da mandíbula ou de uma forma (blend shape) de boca aberta, sempre com o mesmo nome, e esse nome no <id>.forja.json, por exemplo "boca": {"osso": "jaw"} ou "boca": {"forma": "jawOpen"}. A Erethea usa a curva da boca para mexer nele.

5. Áreas: nada a mudar. A Erethea escolhe o eco pelo tipo de cada área do game.project.json (caverna, floresta, castelo, vila...). Se quiser escolher à mão, um campo opcional "acustica" na área.

Sobre o WAV da dublagem: a Fornalha já conserta o cabeçalho que vem do Ignis. A Erethea também vai aceitar o WAV do Ignis direto, então o conserto pode ficar, não atrapalha.

JÁ PEDIDOS ANTES (continuam valendo):

6. Elo ao vivo: um WebSocket em /fornalha/ponte/ws na API local (porta 11480) e o campo "websocket": "/fornalha/ponte/ws" na resposta do GET /fornalha/ponte. A Erethea manda {"tipo":"ola","programa","versao","projeto","monta_cenas":true} e {"tipo":"cenas", ...relatório}; a Fornalha empurra {"tipo":"ponte","versao","arquivos"} quando grava algo. Quando a Erethea disser monta_cenas, a Fornalha não precisa rodar o montar.gd. Até lá a Erethea funciona pelo GET de hoje.

7. Teste: chamar a Erethea com --agent-test=<pasta> (e --agent-test-seconds=<s>, --agent-test-inputs=<arquivo.json>) e ler <pasta>/report.json; sai com 0 quando passa, 1 quando falha, 2 quando as opções estão erradas. O teste_automatico da Fornalha continua funcionando.

8. Depois (quando a Erethea tiver a prévia com LibGodot): embutir a prévia da Erethea na Galeria, como vai estar descrito no erethea/README.md.

9. Prévia (já existe, PR da LibGodot): para mostrar um asset com o renderizador da Erethea, chamar erethea.console.exe --erethea-preview=<arquivo.glb> (não precisa de projeto). Para trocar de asset sem reiniciar, passar também --erethea-preview-control=<arquivo.txt> e escrever o caminho do novo .glb nesse arquivo (ou regravar o mesmo .glb). Para embutir pela DLL (LibGodot), siga o erethea/README.md, "Prévia para a Fornalha" (calls em ordem, argumentos e o exemplo em C de erethea/tests/libgodot/).
```

A Fornalha, por sua vez, tem a lista "Pedidos para a Erethea Engine" no ESTADO.md
dela (cabelo fio a fio, pele, geometria virtual, terreno, FSR, névoa de altura e
raios de luz); esses pedidos estão nas Fases 4 e 5 acima. A Fornalha acha a
Erethea em `%LOCALAPPDATA%\Erethea` (`erethea.console.exe`), que é onde o
instalador põe.

## Como compilar e testar

```sh
# Linux (o que roda na nuvem; 4 núcleos, ~15 min do zero e ~4 min incremental com scu_build).
# Num contêiner novo: pip install scons zstandard prek. Sem wayland-scanner, acrescente
# wayland=no; linker=lld deixa o link mais rápido. O teste [IP] resolve_hostname falha
# em contêiner sem IPv6, o que não é problema do código.
scons platform=linuxbsd target=editor -j4 scu_build=yes tests=yes debug_symbols=no accesskit=no
E=bin/erethea.linuxbsd.editor.x86_64
$E --headless --test --test-case="*Erethea*"            # testes de unidade da Erethea
python3 erethea/tests/mcp/test_mcp.py $E                 # servidor MCP (imprime ERETHEA_MCP_OK)
python3 erethea/tests/agent_test/test_agent_test.py $E   # modo de teste
python3 erethea/tests/fornalha_link/test_fornalha_link.py $E
python3 erethea/tests/fornalha_import/test_fornalha_import.py $E
python3 erethea/tests/assistant/test_assistant.py $E      # painel de IA (imprime ERETHEA_ASSISTANT_OK)
python3 erethea/tools/rebrand.py --check
prek run --all-files                                     # estilo (ou pre-commit)
```

Os builds do CI usam `dev_mode=yes` (aviso vira erro), o que o build acima não
faz. Antes de subir C++, compile os arquivos mudados assim; os objetos ficam em
`bin/obj/` (por exemplo
`scons platform=linuxbsd target=editor dev_mode=yes accesskit=no bin/obj/modules/erethea_raytracing/`;
para o template, `target=template_release` e o objeto `.template_release.`;
para a precisão dupla, `precision=double` e `.editor.double.`).

Arquivos só do Windows (`platform/windows/`) compilam aqui com o MinGW, com o
aviso virando erro como no CI (`apt-get install -y g++-mingw-w64-x86-64-posix`
e `pip install scons`; pegue o nome do objeto na linha de comando do CI):

```sh
scons platform=windows use_mingw=yes target=editor dev_mode=yes d3d12=no angle=no accesskit=no \
    bin/obj/platform/windows/winrt_utils.windows.editor.x86_64.o
# com library_type=shared_library para o libgodot_windows.cpp
```

O CI (`.github/workflows/runner.yml`) roda as checagens de estilo, os builds de
Linux e Windows, os testes da Godot e um workflow por recurso da Erethea
(`erethea_*.yml`): marca, instalador, janela, cena de referência contra a Godot
padrão (falha se a diferença de luminância RMSE passar de 1,0), AMD FSR, XeSS,
DLSS, MCP, validações de exportação, importador e elo da Fornalha, modo de
teste. Os runners não têm placa: Vulkan roda no lavapipe e Direct3D 12 no WARP;
no Windows o editor em janela usa ANGLE (`--rendering-driver opengl3_angle`).

## Convenções dos PRs

- Branch por tarefa, PR pequeno, commits pequenos em inglês começando com
  `Erethea:` (estilo da Godot). Merge com o método "merge" para manter os
  commits.
- Corpo do PR em português, com os títulos do modelo do repositório: `### What
  problem(s) does this PR solve?` e `### Additional information`. Diga o que
  foi testado e o que não pôde set (placa, Fornalha de verdade).
- Nunca pule, desligue ou coloque em quarentena um teste para o CI passar.
  Nunca faça merge com o CI vermelho. No máximo uma nova tentativa por job que
  falhou por instabilidade; se repetir, investigue.
- Módulo novo precisa de linhas no `.github/CODEOWNERS` (e
  `/erethea/tests/**/*.py` já tem dono). Atualize a tabela "Onde mexer" e a
  linha de CI do `CLAUDE.md` e a seção do recurso em `erethea/README.md`.
- O `codespell` reescreve palavras em português (como "nome", "comercial"):
  acrescente em `ignore-words-list` no `pyproject.toml`.
- Nunca rode `--doctool` junto com `--gdscript-docs` (escreve XML na raiz).
  Classe nova exposta pede o XML em `doc/classes` ou no `doc_classes` do
  módulo.

## Como trabalhar com sessões em paralelo

Pedido do Kaiky em 23/11: a sessão principal (Opus, esforço extra) coordena e
revisa; tarefas bem delimitadas vão para sessões Sonnet, que custam menos da
cota dele. Assim rendeu: PRs #26, #27, #29, #31 e #32 foram feitos por elas e
revisados aqui antes do merge.

- **Abrir**: ferramenta `create_session` do servidor `claude-code-remote`, com
  `model: claude-sonnet-5-5`, `source_url` do repositório, `outcome_branch`
  próprio (`claude/erethea-<tarefa>`) e tags `erethea`, `erethea-worker`.
- **O que pôr nas instruções** (em português, completas, porque a sessão nova
  não sabe nada): ler o `CLAUDE.md` e o `HANDOFF.md`; a tarefa com os
  arquivos e as APIs já conferidos por você ("leia o código; não invente
  API"); o que já existe na Fornalha (para não duplicar); recurso novo
  desligado por padrão; código e commits em inglês com "Erethea:", trechos
  em arquivos da Godot com `// Erethea:`; toda classe, propriedade ou
  configuração nova (inclusive configuração do editor) precisa de doc no XML
  (`doc/classes/` ou `doc_classes` do módulo), conferida com
  `--doctool .` **sem** `--no-docbase`; teste que usa áudio precisa de
  `[Audio]` no nome; os comandos de build e de teste (só `*Erethea*` e os
  testes novos, para poupar cota); as pastas que outras sessões estão
  mexendo; PR para `main` em português com os títulos do modelo, dizendo o
  que foi testado e o que não; **não fazer merge**; acompanhar o CI até o
  fim; avisar a sessão principal com `send_message` (número do PR, CI e um
  resumo).
- **Revisar** (sempre, antes do merge): `git diff origin/main...<branch>`;
  mudanças em arquivos da Godot pequenas e marcadas; desligado por padrão;
  cena de referência igual à da Godot com as configurações padrão; testes que
  provam algo (que falhariam sem a mudança); depois de um `git merge` da
  main no branch, confira que o diff de três pontos só tem o que você
  revisou. Pequenos ajustes: peça à própria sessão (`send_message`).
- **Merge**: só com o CI inteiro verde (os 32 checks, mais os novos);
  `merge_method: merge`. Uma nova tentativa no máximo para as falhas
  conhecidas (abaixo), nunca para outra coisa.
- **Cota**: quando o limite de 5 horas acaba, todas as sessões param (aparece
  "usage_limit_reached"). Quando voltar, mande `send_message` para cada uma
  continuar de onde parou. No máximo duas sessões Sonnet de cada vez.
- **Bom para Sonnet**: tarefas com modelo pronto no repositório (um efeito de
  pós-processamento novo, uma opção de configuração, um passo de CI). **Faça
  você**: o que mexe fundo no renderizador ou no driver (ray tracing, DLSS no
  Vulkan, geometria densa) e o desenho de recursos novos.

## Falhas conhecidas do CI

- Barra de título no Windows: o editor fechou com `0xC0000409` cerca de 10 s
  depois de abrir o `smoke_project`, duas vezes no mesmo artefato antes e de
  novo em 23/11 (PR #29, que não mexe na janela); a nova tentativa passou. É um
  "fast fail" (abort, parâmetro inválido do CRT ou estouro de pilha), que o
  tratador de crash da Godot não vê, e o build do CI não tem PDB. Em 23/11 o
  teste não achou os eventos do Windows porque olhava logo depois da queda;
  agora espera até 30 s por eles, e o `erethea_window.yml` pede ao Windows um
  minidump (LocalDumps) em `dumps/` no artefato `erethea-window-test`. Se
  acontecer de novo, o evento 1000 diz o módulo e o deslocamento, e o 1001 o
  código do fast fail; comece por aí (tarefa aberta).
- Instalador, passo "Update silently": travou quatro vezes (duas antes do
  PR #23, duas em 23/11 mesmo com o limite de 4 minutos dele), sempre além do
  limite do passo e do job e sem log nenhum, nem dos passos anteriores; passou
  em todas as novas tentativas. Esse é o jeito do executor do GitHub quando um
  processo aberto pelo passo sobrevive a ele segurando a saída do passo: espera
  essa saída acabar para sempre. O `Start-Process` passava a saída do passo ao
  instalador (e ao que ele abre), e o editor reaberto ficava vivo quando o
  passo não o achava. Correção (PR #28): o instalador abre por ShellExecute,
  sem herdar nada do passo, o editor é procurado pelo caminho via
  `Win32_Process`, e tudo que a atualização abriu é fechado no fim (`finally`),
  com a lista impressa no log. O job agora tem 20 minutos. **Não resolveu**:
  em 23/11 travou de novo no PR #30, já com o #28 (no total, umas 4 em cada 10
  execuções, antes e depois). Então é o próprio executor que para de
  responder durante o passo (nem o limite do passo funciona), e não o script.
  Experimento 2 (PR #30): o editor que o instalador reabre é fechado assim que
  aparece, antes de montar a parte gráfica (ele abre com o renderizador padrão
  num executor sem placa de vídeo; o teste da janela força o ANGLE por isso).
  Conte as próximas execuções (a primeira, no próprio #30, passou; os PRs
  #31 e #32 passaram com o script anterior): se não travar mais em umas 8, a
  causa é essa inicialização gráfica; se travar, a suspeita passa ao instalador (o Restart
  Manager do `/CLOSEAPPLICATIONS`, que pode querer fechar o Explorer, que
  segura o ícone do `erethea.exe`).
- Rodar de novo um job reaproveita o artefato do build da mesma execução; para
  um build novo, suba um commit.

## Detalhes técnicos úteis

- Painel de IA (`modules/erethea_assistant/`): os provedores só montam pedidos e
  leem respostas (`erethea_assistant_providers.*`, testáveis sem editor); o painel
  (`editor/erethea_assistant_dock.*`) faz o laço pergunta → ferramentas →
  resultados com `HTTPRequest`; o desfazer fica em `editor/erethea_assistant_undo.*`.
  As ferramentas vêm do `EretheaMcpPlugin::get_singleton()->call_tool_local()`.
- Servidor MCP do editor (`modules/erethea_mcp/`): HTTP só em 127.0.0.1, porta
  11481 (a Fornalha usa 11480, o Ignis 11470), desligado por padrão
  (Configurações do Editor `erethea/mcp/enabled` ou variável
  `ERETHEA_MCP_PORT`). Mudanças passam pelo desfazer como "MCP: ...". Chamadas
  que respondem depois terminam por `server.finish_call`; o servidor desiste
  depois de 60 s.
- Projetos da Fornalha (repositório público `kaikyolf29-hub/fornalha`, branch
  `claude/confident-sagan-6m7nvt`): a pasta do projeto é um projeto Godot com
  `game.project.json`; personagens em `Characters/<id>/<id>.glb` (com
  `<id>.forja.json`, altura em `estatisticas.altura_m`), inimigos em
  `Enemies/<id>/<id>.glb`, blockout em `World/`, scripts da Fornalha em
  `Scripts/fornalha/` (jogador, inimigo, montar, `teste_automatico`), add-on
  `addons/fornalha_ponte`, recibos de licença em JSON por linha. A Fornalha
  acha a Erethea em `%LOCALAPPDATA%\Erethea` (ou `FORNALHA_ERETHEA`) e usa
  `--headless --import`, `--script` e `--write-movie`: avise se a Erethea mudar
  algum desses.
- Ignis: `kaikydev1-jpg/Ignis`, API compatível com OpenAI na porta 11470
  (`/v1/chat/completions`, `/v1/models`, `/motor/estado`).
- Godot 4.8 e armadilhas que já custaram tempo: `callable_mp` está em
  `core/object/callable_mp.h`; `Script` não tem `is_valid()`; o constructor de
  cópia de `LocalVector` é explícito; `ProcessID` fica em
  `core/os/process_id.h`; o GCC recusa atributo em definição de função inline
  (declare com `_PRINTF_FORMAT_ATTRIBUTE_2_0` e defina no `.cpp`);
  `MAX<T>(a, 1)` dá aviso de sinal, use `MAX(a, T(1))`; no `.tscn`, o
  `Transform3D(...)` é por linhas da base, não por colunas.
- Arguments desconhecidos `--foo=bar` ficam em `OS::get_cmdline_args()`; use
  sempre a forma com `=` (um argumento solto vira a cena a abrir).
- O proxy da nuvem bloqueia baixar artefatos do GitHub Actions: imprima o
  diagnóstico no log do job.
