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
| Em aberto | Juntar a Fornalha dentro da Erethea no futuro ou manter dois apps ligados (a LibGodot deixa os dois caminhos abertos). Não precisa decidir agora. |

## Onde cada fase está

As fases e o "pronto quando" de cada uma vêm do Plano 2.

| Fase | Pronto quando | Estado |
| --- | --- | --- |
| 0 · Fork e marca | O instalador com o nome novo abre um projeto da Godot sem erro | Pronta (PR #3) |
| 1 · Janela nativa | O editor não tem mais a barra do sistema e encaixa nas bordas do Windows 11 | Pronta (PR #4 barra de título, PR #5 atualização automática). Falta só a assinatura (no lançamento) e dois extras opcionais, abaixo |
| 2 · Visual rápido | A cena de referência fica visivelmente melhor que na Godot padrão, no mesmo FPS | Pronta (PRs #6 a #14: cena de referência, presets, pós-processamento, AMD FSR 3.1/4, XeSS, DLSS, streaming de texturas) |
| 3 · Ponte com a Fábrica | Um personagem vindo do Plano 1 aparece na cena sozinho e anda | Em andamento: MCP (#15), validações de exportação (#16), logo (#17), importador do `game.project.json` (#18) no `main`; elo ao vivo (#19) e modo de teste (#20) abertos; painel de IA começado; prévia com LibGodot por fazer |
| 4 · Visual avançado | A cena roda com ray tracing numa placa RTX e sem ele numa placa sem suporte | Não começou |
| 5 · Geometria densa | Um modelo de milhões de polígonos roda sem LOD feito à mão | Não começou |

## O que fazer agora (na ordem)

### 1. PR #19, elo ao vivo com a Fornalha

Branch `claude/project-thread-ipvi6s`. O editor pergunta à Fornalha
(`GET 127.0.0.1:11480/fornalha/ponte`) a cada 2 s e, quando ela anunciar um
WebSocket, passa a usá-lo; quando o projeto muda, importa e monta as cenas
sozinho. Se o CI estiver verde, faça o merge (método "merge", para manter os
commits pequenos). Se algum job falhar, veja "Falhas conhecidas do CI" antes de
mexer.

### 2. PR #20, modo de teste para agentes

Branch `claude/erethea-agent-test`, em cima do #19 (por isso está como
rascunho). Depois do merge do #19: marque como pronto, espere o CI (inclui o
workflow novo `erethea_agent_test.yml` em Linux e Windows) e faça o merge. Se o
GitHub pedir, atualize o branch com o `main` por merge (não reescreva o
histórico). Detalhes no próprio PR e em `erethea/README.md`, "Modo de teste
para agentes".

### 3. Painel de IA (último item grande da Fase 3)

Plano 2: "chat com o cérebro (Ignis local ou API) que enxerga a cena aberta e o
`game.project.json`. Pedidos como 'coloque 10 inimigos na floresta' ou 'faça o
sistema de combate' viram ações no editor e scripts GDScript, sempre com
desfazer."

Já feito, no branch `claude/erethea-assistant-wip` (1 commit em cima do
`claude/erethea-agent-test`, compila sem avisos e o teste do MCP passa):
ferramentas MCP `list_files`, `read_file` e `edit_file`
(`modules/erethea_mcp/editor/erethea_mcp_file_tools.*`),
`EretheaMcpToolResult::to_dictionary()/from_dictionary()` e
`EretheaMcpSceneTools::set_action_source()` (nome de quem fez a mudança no
histórico de desfazer). Faça o rebase desse commit no `main` depois do #20.

Desenho decidido:

- **Módulo novo** `modules/erethea_assistant/` (só editor; `config.py` com
  `env.module_add_dependencies("erethea_assistant", ["erethea_mcp"])` e
  `can_build` só em `env.editor_build`). Um `EditorDock` chamado "Assistant",
  aberto como aba no lado direito (`DOCK_SLOT_RIGHT_UL`), fechável. Textos da
  interface em inglês com `TTR()`, como os outros módulos da Erethea.
- **Reusa as ferramentas do MCP** sem passar pela rede. Em `EretheaMcpPlugin`:
  um `get_singleton()`, e `call_tool_local(name, args, Callable on_finished,
  EretheaMcpToolResult &r_result)` que chama `call_tool(..., p_can_wait=true,
  ...)`; quando a ferramenta responde depois (`take_screenshot`, `run_test`),
  guarda o id num `HashMap<uint64_t, Callable>` e troca as chamadas diretas a
  `server.finish_call(...)` por um `_finish_call(id, result)` que entrega ao
  `Callable` (com `call_deferred(result.to_dictionary())`) ou ao servidor. O
  processamento de capturas, testes e depuradores em
  `NOTIFICATION_INTERNAL_PROCESS` hoje só roda com o servidor ligado; passe a
  rodar também quando houver um cliente local (`local_client = true`).
- **Três provedores** (configuração do editor `erethea/assistant/provider`):
  1. **Ignis** (padrão, local): `POST http://127.0.0.1:11470/v1/chat/completions`,
     modelo `"default"`. Como não se sabe se o Ignis aceita "tools", use o
     mesmo jeito que a Fornalha já provou com o Ignis 0.4.0 (Qwen3) em
     `crates/fornalha-core/src/plano.rs`: `response_format` com `json_schema`
     estrito pedindo `{"reply": "...", "actions": [{"tool": "<nome>",
     "arguments": {...}}]}`; o Qwen3 pode mandar `<think>...</think>` antes do
     JSON (corte até o último `</think>` e pegue do primeiro `{` ao último
     `}`). Confira cada ação contra o esquema da ferramenta (tire argumento que
     não existe ou do tipo errado, descarte ação sem os obrigatórios), execute,
     mande os resultados de volta e repita até 4 rodadas ou `actions` vazio.
     Errors do Ignis vêm em `error` ou `error.message` (códigos `sem_modelo`,
     `sem_motor`: "abra o Ignis e clique em Preparar"). Contexto curto
     (modelos pequenos): árvore da cena até ~150 nós e `game.project.json` até
     ~6000 characters, só as últimas 6 mensagens.
  2. **Claude pela API** (opcional, só com chave): `POST
     https://api.anthropic.com/v1/messages`, cabeçalhos `x-api-key`,
     `anthropic-version: 2026-10-21`, `content-type: application/json`; modelo
     padrão `claude-opus-5-5` (configurável); `max_tokens` 16000; ferramentas
     nativas (cada ferramenta MCP vira `{name, description, input_schema}`);
     `tool_choice` só `auto` (o Opus 5.5 recusa `any`/`tool` com error 400);
     não mande `thinking` (no Opus 5.5 ele sempre pensa; `{"type":"disabled"}`
     dá 400); `cache_control: {"type": "ephemeral"}` no topo do corpo; e
     `fallbacks: "default"` com o cabeçalho `anthropic-beta:
     server-side-fallback-2026-07-01` (se der 400 citando fallback, tente de
     novo sem). Chave na configuração `erethea/assistant/claude/api_key`
     (`PROPERTY_HINT_PASSWORD`) ou na variável `ANTHROPIC_API_KEY`. **O
     histórico não pode set editado**: no Opus 5.5 os blocos de pensamento
     ficam presos à conversa. Então o prompt de sistema e a lista de
     ferramentas são fixos durante a conversa, o contexto do editor vai no
     começo de cada mensagem do usuário (não no sistema), e a resposta do
     assistente volta igualzinha (guarde o trecho JSON cru de `content` e
     monte o corpo por concatenação). Se vier 400 com "bound to a different
     conversation", tire todos os blocos `thinking`/`redacted_thinking` do
     histórico e tente uma vez. `stop_reason`: `end_turn` terminou;
     `tool_use` executa todas as ferramentas e manda todos os `tool_result`
     numa só mensagem do usuário (com `is_error` quando falhar; capturas como
     bloco `image` base64 PNG); `max_tokens` não executa ferramentas da
     resposta cortada; `refusal` mostra que o modelo recusou e tira do
     histórico a última pergunta (remover do fim é permitido). Repetir uma
     vez depois de 3 s em 429, 500, 502, 503 e 529.
  3. **Servidor compatível com OpenAI** (opcional): URL base com `/v1`, chave
     `Bearer`, `tools` do tipo `function`, resultado como mensagens `tool`
     com `tool_call_id`.
- **Contexto do editor** em cada pergunta: nome do projeto, se é da Fornalha,
  cenas abertas, a cena editada com a árvore resumida (use
  `EretheaMcpSceneTools::get_scene_tree` e limit profundidade e nós), os nós
  selecionados e o `game.project.json` (mande de novo só quando mudar).
- **Prompt de sistema** (inglês, responde na língua da pessoa): assistente
  dentro do editor da Erethea (fork da Godot 4); use as ferramentas para agir;
  salve as cenas que mudar com `save_scene`; em projetos da Fornalha, os
  scripts em `Scripts/fornalha/` são da Fornalha (prefira mudar propriedades
  exportadas nas cenas ou o `game.project.json` e depois
  `build_fornalha_scenes`); "nessa área" é a cena aberta ou o nó selecionado;
  responda curto dizendo o que mudou.
- **Desfazer** ("sempre com desfazer"): um botão "Undo last answer". Antes de
  cada ferramenta que muda algo, guarde o estado: para `create_node`,
  `set_node_properties`, `delete_node` e `attach_script`, o id do histórico da
  cena (`EditorUndoRedoManager::get_history_id_for_object(root)`) e a versão
  (`get_history_undo_redo(id)->get_version()`); para `write_script`,
  `edit_file` e `create_scene`, o conteúdo anterior do arquivo (ou que ele não
  existia); para `save_scene`, `run_game` e `run_test`, os arquivos das cenas
  abertas (elas são salvas antes de rodar); para `build_fornalha_scenes`, os
  arquivos de texto do projeto (`.tscn .tres .gd .json .cfg` e
  `project.godot`, com limit de tamanho). Ao desfazer: chame
  `undo_history(id)` até voltar à versão guardada; cena aberta cujo arquivo
  mudou e cujo histórico voltou é salva de novo
  (`EditorNode::save_scene_if_open`); cena mexida pelo `build_fornalha_scenes`
  recebe o arquivo antigo e `EditorNode::reload_scene`; scripts e outros
  arquivos voltam ao conteúdo antigo (ou são apagados) e recarregam como no
  `write_script`; cena criada na resposta é fechada e apagada. Se a pessoa
  mexeu depois da resposta, avise em vez de desfazer o trabalho dela.
- **Interface**: selector de provedor, botão de nova conversa, conversa num
  `RichTextLabel` (pergunta, resposta e cada ação em cinza, errors em
  vermelho), campo de texto (Enter envia, Shift+Enter quebra linha), botão
  Send que vira Stop enquanto roda, e o botão de desfazer. Limit de 30
  ferramentas por pergunta.
- **Testes**: testes de unidade da montagem dos pedidos e da leitura das
  respostas dos três provedores (incluindo o trecho JSON cru, o corte do
  `<think>` e a conferência das ações do Ignis). Teste de ponta a ponta
  `erethea/tests/assistant/test_assistant.py` com um servidor false em Python
  que responde como Anthropic, OpenAI e Ignis; um plugin GDScript no projeto
  de teste muda as configurações do editor, escreve no campo do painel e
  aperta Send e Undo (procurando os nós pelo nome); o Python confere pelo MCP
  (`get_scene_tree`, `read_file`) que a mudança aconteceu e que o desfazer
  voltou tudo, e confere no servidor false que o contexto e os resultados das
  ferramentas chegaram certos. Workflow `erethea_assistant.yml` (Linux e
  Windows), linha no `runner.yml`, `CODEOWNERS`, `CLAUDE.md` e seção
  "Painel de IA" no `erethea/README.md`.
- O teste de verdade com o Ignis e com a API fica para o teste final do Kaiky
  (o CI só usa o servidor false): diga isso no PR.

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
- Mande ao Kaiky, numa mensagem só, os pedidos para a Fornalha (abaixo), para
  ele colar no chat da Fornalha.

### 6. Extras pendentes das Fases 1 e 2 (pequenos, opcionais)

- Janela moderna também nos jogos: configuração de projeto (desligada por
  padrão) para o jogo exportado no Windows não ter a barra do sistema.
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
   melhor, cabelo, tecido e olhos: recursos de material num módulo
   (`modules/erethea_materials/`), com exemplos na cena de referência.
6. **Terreno nativo** com pintura de materiais, criado a partir do blockout do
   Plano 1 (`World/blockout.glb` e as áreas do `game.project.json`): malha por
   clipmap, colisão `HeightMapShape3D`, pintura no editor. O Terrain3D (MIT)
   serve de referência.
7. **Vegetação**: espalhador com LOD, impostores e vento (MultiMesh,
   `visibility_range`, sombreador de vento).
8. **Animação**: retarget automático para o esqueleto padrão do Plano 1 (veja
   como a Forja/Fornalha monta o esqueleto) e locomoção com o MotionBricks da
   NVIDIA (conferir a licença antes).
9. Teste de pronto no CI com o que der (WARP/lavapipe); a prova na RTX e na RX
   580 entra na lista do teste final do Kaiky.

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

### 9. Godot 4.8 estável

Quando a Godot lançar a 4.8 estável, junte no `upstream` e no `main` como em
`erethea/README.md` ("Atualizar a Godot"), rode `rebrand.py --check`, troque o
`GODOT_BASE_VERSION` do workflow da cena de referência e deixe o CI inteiro
passar.

### 10. Lançamento

1. O Kaiky confere o nome no INPI e o domínio (só ele pode).
2. Assinatura do instalador (o Kaiky paga e cria a conta; você liga a variável
   e confere o workflow).
3. `erethea/TESTE_DO_CRIADOR.md`: lista curta do que o Kaiky testa no PC dele
   (instalador, abrir projeto da Godot, barra de título, presets, upscalers na
   RTX, ray tracing na RTX e sem ele na RX 580, Fornalha ligada, painel de IA
   com o Ignis e com a API, modelo pesado).
4. Tag `v1.0.0` (ou a versão que o Kaiky quiser) e o release com o
   instalador. Só então avise o Kaiky para baixar e testar.

## Pedidos para a Fornalha (o Kaiky cola no chat da Fornalha)

```text
Pedidos da Erethea para a Fornalha:
1. Elo ao vivo: um WebSocket em /fornalha/ponte/ws na API local (porta 11480)
   e o campo "websocket": "/fornalha/ponte/ws" na resposta do
   GET /fornalha/ponte. A Erethea manda {"tipo":"ola","programa","versao",
   "projeto","monta_cenas":true} e {"tipo":"cenas", ...relatório}; a Fornalha
   empurra {"tipo":"ponte","versao","arquivos"} quando grava algo. Quando a
   Erethea disser monta_cenas, a Fornalha não precisa rodar o montar.gd.
   Até lá a Erethea funciona pelo GET de hoje.
2. Teste: chamar a Erethea com --agent-test=<pasta> (e
   --agent-test-seconds=<s>, --agent-test-inputs=<arquivo.json>) e ler
   <pasta>/report.json; sai com 0 quando passa, 1 quando falha, 2 quando as
   opções estão erradas. O teste_automatico da Fornalha continua funcionando.
3. (Depois da prévia com LibGodot) embutir a prévia da Erethea como descrito
   em erethea/README.md.
```

## Como compilar e testar

```sh
# Linux (o que roda na nuvem; 4 núcleos, ~4 min incremental com scu_build)
scons platform=linuxbsd target=editor -j4 scu_build=yes tests=yes debug_symbols=no accesskit=no
E=bin/erethea.linuxbsd.editor.x86_64
$E --headless --test --test-case="*Erethea*"            # testes de unidade da Erethea
python3 erethea/tests/mcp/test_mcp.py $E                 # servidor MCP (imprime ERETHEA_MCP_OK)
python3 erethea/tests/agent_test/test_agent_test.py $E   # modo de teste
python3 erethea/tests/fornalha_link/test_fornalha_link.py $E
python3 erethea/tests/fornalha_import/test_fornalha_import.py $E
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
- Instalador, passo "Update silently": travou uma vez além do tempo e passou na
  nova tentativa. Se repetir, investigue o `/SILENT /update=1`.
- Rodar de novo um job reaproveita o artefato do build da mesma execução; para
  um build novo, suba um commit.

## Detalhes técnicos úteis

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
