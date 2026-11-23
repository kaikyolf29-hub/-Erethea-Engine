# Erethea Engine: notas de passagem

Escrito em 22/11/2026 para continuar o trabalho de outra conta. Uma sessão nova
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
kaikyolf29-hub/-Erethea-Engine. Leia primeiro o HANDOFF.md (na raiz do main),
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

## Onde cada fase está

As fases e o "pronto quando" de cada uma vêm do Plano 2.

| Fase | Pronto quando | Estado |
| --- | --- | --- |
| 0 · Fork e marca | O instalador com o nome novo abre um projeto da Godot sem erro | Pronta (PR #3) |
| 1 · Janela nativa | O editor não tem mais a barra do sistema e encaixa nas bordas do Windows 11 | Pronta (PR #4 barra de título, PR #5 atualização automática). Falta só a assinatura (no lançamento) e dois extras opcionais, abaixo |
| 2 · Visual rápido | A cena de referência fica visivelmente melhor que na Godot padrão, no mesmo FPS | Pronta (PRs #6 a #14: cena de referência, presets, pós-processamento, AMD FSR 3.1/4, XeSS, DLSS, streaming de texturas) |
| 3 · Ponte com a Fábrica | Um personagem vindo do Plano 1 aparece na cena sozinho e anda | Em andamento: MCP (#15), validações de exportação (#16), logo (#17), importador do `game.project.json` (#18), elo ao vivo (#19), modo de teste (#20) e painel de IA (#22) e Claude Code no painel (#24) no `main`; som: WAV da dublagem e eco por área no PR #25; música interativa, boca com a fala e prévia com LibGodot por fazer |
| 4 · Visual avançado | A cena roda com ray tracing numa placa RTX e sem ele numa placa sem suporte | Não começou |
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
3. **Música que muda com o jogo** (PR aberto no branch
   `claude/erethea-som3-musica`): o `AudioStreamWAV` ganhou `bpm`, `beat_count`
   e `bar_beats` (como o Ogg; correção genérica a propor à Godot, teste em
   `tests/erethea/test_erethea_wav_bpm.cpp`), e o `build_fornalha_scenes` acerta
   o `.import` de cada `Audio/musica/<situacao>.wav` (laço, bpm, compassos × 4)
   e põe na `principal.tscn` o nó `Musica` do grupo `erethea_musica`
   (`AudioStreamInteractive`, troca no fim do compasso) que o `Sons` da
   Fornalha chama. Desliga em `editor/erethea_fornalha/interactive_music`.
   Falta ouvir de verdade (o CI não toca som). Veja `erethea/README.md`, "Som".
4. **Boca mexendo com a fala**: a curva do `.boca.json` move o osso da
   mandíbula ou a forma de boca aberta que o `<id>.forja.json` disser.

Os itens 2 a 4 dependem dos pedidos 1 a 5 para a Fornalha (abaixo), enviados
ao Kaiky em 22/11. Enquanto a Fornalha não mudar, siga com o que já existe e
confira no repositório público dela (`kaikyolf29-hub/fornalha`, só leitura).

### 4. Prévia com LibGodot (último item da Fase 3)

Plano 2: "A LibGodot, que chegou na 4.6, permite embutir a engine em outro
programa. A Fornalha pode mostrar o asset já com o renderizador da engine, do
jeito que vai ficar no jogo."

1. Pesquise como a LibGodot funciona na base 4.8 (opção do SCons para gerar a
   biblioteca compartilhada, a API C para criar e rodar uma instância, e o
   exemplo official). Não invente a API: leia o código (`SConstruct`, `main/`,
   `core/extension/`) e a documentação.
2. Uma cena de prévia própria da Erethea num módulo (por exemplo
   `modules/erethea_preview/`): carrega um `.glb`, enquadra a câmera, gira em
   volta, chão, luz e céu com o preset de qualidade da Erethea, e aceita
   trocar o asset sem reiniciar. Ela funciona também como executável
   (`--erethea-preview=<arquivo.glb>`), o que já serve à Fornalha antes da
   integração embutida.
3. Build da biblioteca no CI (Windows primeiro, Linux compilando) como
   artefato do release, e um programa C pequeno em `erethea/tests/libgodot/`
   que carrega a biblioteca, abre a prévia de um modelo de teste, renderiza
   alguns quadros e salva uma captura. O CI confere que a captura não está
   vazia.
4. Documente para a Fornalha como embutir (DLL, chamadas, arguments) e
   coloque o pedido na lista "Pedidos para a Fornalha" abaixo.

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
- Canal de testes da atualização: opção no editor para receber prereleases
  (as versões com traço já saem como prerelease).
- Geração de quadros (FSR 3.1 e DLSS): o plano cita "upscaling e geração de
  quadros"; hoje só há upscaling.
- DLSS no Vulkan (hoje só Direct3D 12).
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
2. **Ray tracing da Godot**: a 4.7 trouxe a base de ray tracing no
   `RenderingDevice` (só a "tubulação"). Montar as estruturas de aceleração da
   cena e fazer, nesta ordem, sombras suaves, reflexos e iluminação global por
   ray tracing, como opção em `servers/rendering/renderer_rd/` ou num módulo.
   Confira se o lavapipe do runner Linux e o WARP do Windows (DXR) têm ray
   tracing; se tiverem, o CI prova a imagem; se não, prova só o caminho sem
   ray tracing.
3. **Path tracing do fork RTX da NVIDIA** (MIT, lançado na GDC 2026): estudar e
   trazer como opção só para RTX, com a mesma configuração.
4. **Sombras**: cascatas melhores e sombras suaves por ray tracing (do item 2).
5. **Materiais em camadas** (terra, sujeira, molhado), pele com subsuperfície
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

## Falhas conhecidas do CI

- Barra de título no Windows: o editor fechou uma vez com `0xC0000409` cerca de
  10 s depois de abrir o projeto, duas vezes no mesmo artefato; um build novo
  do mesmo código passou. O `title_bar_test.ps1` agora imprime o fim dos logs e
  os eventos 1000/1001/1026 do Windows quando falha, então o log do job mostra o
  módulo que caiu se acontecer de novo.
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
  com a lista impressa no log. O job agora tem 20 minutos. Se travar de novo
  mesmo assim, a hipótese caiu: aí a suspeita passa a ser a própria máquina
  (o editor reaberto sem `--rendering-driver`, num executor sem placa de vídeo).
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
