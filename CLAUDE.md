# Erethea Engine: regras para agentes (Claude Code, Opus)

A Erethea Engine é um fork da Godot Engine (base: Godot 4.8) com nome próprio,
janela moderna no Windows e gráficos melhores, ligado à Fábrica de jogos.
O plano completo está no "Plano 2 · Erethea Engine (fork da Godot)".

Para retomar o trabalho, comece por `HANDOFF.md`: o estado de cada fase, os
PRs abertos, as decisões já tomadas e o passo a passo até o lançamento.

## Conversa

- Fale em português do Brasil com o dono do projeto, nos PRs e na
  documentação da Erethea (`README.md`, `erethea/`).
- Código, comentários e mensagens de commit seguem o estilo da Godot, em
  inglês, para que correções genéricas possam ser devolvidas à Godot.

## Regra de ouro: ficar perto da Godot

Fork que fica para trás morre. Cada mudança tem que deixar puxar as versões
novas da Godot sem reescrever nada.

- Código novo vai para `erethea/`, para módulos próprios (`modules/<nome>/`)
  ou para GDExtension, não espalhado pelo núcleo.
- O que precisar mexer em arquivo da Godot entra em commits pequenos e bem
  descritos. Marque o trecho com um comentário `// Erethea:` explicando o porquê.
- `upstream` espelha a Godot: nunca faça commit nele. `main` é a engine.
- A marca (nome, ícones, splash, textos) só muda por
  `erethea/tools/rebrand.py`. Nunca edite à mão o que o script troca; depois de
  um merge da Godot, rode o script de novo e confira com `--check`.
- Não quebre a compatibilidade: projetos da Godot (`project.godot`, GDScript,
  C#) têm que abrir como estão. Não renomeie `res://`, `user://`, o namespace
  `Godot` do C#, a interface GDExtension, classes ou formatos de arquivo.
  Recurso novo que mude o comportamento entra desligado nas configurações do
  projeto.
- Correção de bug genérica deve ser proposta também para a Godot.

## Como trabalhar

- Uma tarefa por vez, pequena e testável ("barra de título no Windows",
  "integrar FSR 3.1"), cada uma no seu branch e no seu PR.
- Não publique (merge, release, instalador) sem os testes passarem.
- Não diga que algo funciona numa placa de vídeo sem ter provado nela. As
  placas de referência são a RX 580 8 GB (sem ray tracing) e a RTX 4060 Ti ou
  5060 Ti de 16 GB.
- Windows vem primeiro (janela, instalador, testes), mas o código continua
  compilando no Linux.
- A Erethea completa a Fornalha 3D, não repete o que ela já faz: antes de um
  recurso para projetos da Fornalha, confira no repositório dela
  (`kaikyolf29-hub/fornalha`, só leitura). A Erethea soma o que é de engine.
- Tarefas bem delimitadas podem ir para sessões Sonnet em paralelo (no máximo
  duas), e a sessão principal revisa cada diff antes do merge. Veja
  `HANDOFF.md`, "Como trabalhar com sessões em paralelo".

## Onde mexer

| Assunto | Pasta |
| --- | --- |
| Janela, barra de título, Windows | `platform/windows/` (`display_server_windows.cpp`) |
| Gráficos, renderizador | `servers/rendering/renderer_rd/` |
| Presets de qualidade | `modules/erethea_quality/` |
| Pós-processamento (LUT `.cube`, lens flare, motion blur, grão) | `modules/erethea_post/` |
| Ray tracing e sombra de contato do sol (depois reflexos e GI) | `modules/erethea_raytracing/` |
| Upscalers (AMD FSR 3.1/4, XeSS e DLSS) | `modules/erethea_upscale/` |
| Streaming de texturas 3D | `modules/erethea_streaming/` |
| Servidor MCP do editor (agentes como o Claude Code) | `modules/erethea_mcp/` |
| Modo de teste para agentes (`--agent-test`) | `modules/erethea_agent_test/` |
| Validações antes de exportar (polígonos, texturas, escala, recibos) | `modules/erethea_export_checks/` |
| Ponte com a Fornalha (cenas montadas pelo `game.project.json`, elo ao vivo) | `modules/erethea_fornalha/` |
| Painel de IA do editor (Ignis, Claude pela API, servidor OpenAI, Claude Code) | `modules/erethea_assistant/` |
| Prévia do asset para a Fornalha (`--erethea-preview=`, LibGodot) | `modules/erethea_preview/` |
| Editor | `editor/` |
| Marca, instalador, testes da Erethea | `erethea/` |
| CI | `.github/workflows/` (`runner.yml`, `erethea_installer.yml`, `erethea_reference_scene.yml`, `erethea_mcp.yml`, `erethea_agent_test.yml`, `erethea_export_checks.yml`, `erethea_fornalha_import.yml`, `erethea_assistant.yml`, `erethea_preview.yml`) |

## Compilar e testar

```sh
scons platform=windows target=editor     # bin/erethea.windows.editor.x86_64.exe
scons platform=linuxbsd target=editor    # bin/erethea.linuxbsd.editor.x86_64

bin/erethea.<plataforma>.editor.x86_64 --test                                   # testes da Godot
bin/erethea.<plataforma>.editor.x86_64 --headless --import --path erethea/tests/smoke_project
bin/erethea.<plataforma>.editor.x86_64 --headless --path erethea/tests/smoke_project   # imprime ERETHEA_SMOKE_OK
python erethea/tools/rebrand.py --check
```

Antes de propor uma mudança visual, renderize a cena de referência e compare as
capturas com a Godot padrão (passo a passo em `erethea/README.md`, "Cena de
referência"). Com as configurações padrão, a Erethea tem que desenhar igual à
Godot; o CI confere isso em todo PR. Recurso visual novo entra desligado e é
comparado ligando-o na cena.

```sh
bin/erethea.<plataforma>.editor.x86_64 --path erethea/tests/reference_scene --fixed-fps 30 -- --capture="$PWD/capturas/erethea"
```

O estilo de código é o da Godot: rode `prek run --all-files` (ou `pre-commit`)
antes de abrir o PR. O CI roda as checagens de estilo, os builds de Linux e
Windows, os testes, a checagem da marca e o teste do instalador.
