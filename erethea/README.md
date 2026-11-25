# Pasta `erethea/`

Tudo o que é próprio da Erethea Engine e não existe na Godot fica aqui, para
que juntar versões novas da Godot não dê conflito.

| Pasta | O que tem |
| --- | --- |
| `branding/` | Logo, ícones (`.svg`, `.png`, `.ico`), splash e imagens do instalador da Erethea |
| `tools/rebrand.py` | Aplica a marca Erethea por cima do código da Godot |
| `tools/make_branding.py` | Gera os arquivos de `branding/` a partir do desenho do logo (`branding/source/`) |
| `installer/erethea.iss` | Instalador do Windows (Inno Setup 6), também usado nas atualizações |
| `tests/smoke_project/` | Projeto Godot simples que o CI abre com o instalador |
| `tests/reference_scene/` | Cena de referência (vila, floresta, personagem, dia e noite) para comparar com a Godot |
| `tests/mcp/` | Projeto vazio e teste de ponta a ponta do servidor MCP do editor |
| `tests/agent_test/` | Jogo pequeno e teste de ponta a ponta do modo de teste para agentes |
| `tests/export_checks/` | Projeto da Fornalha e teste das validações antes de exportar |
| `tests/fornalha_import/` | Projeto da Fornalha e teste das cenas montadas pelo `game.project.json` |
| `tests/fornalha_link/` | Teste do elo ao vivo com a Fornalha, com uma Fornalha de mentira |
| `tests/preview/` | Teste de ponta a ponta da prévia para a Fornalha (arquivos glTF gerados pelo teste) |
| `tests/libgodot/` | Programa em C que embute a engine pela LibGodot e teste da prévia embutida |
| `TESTE_DO_CRIADOR.md` | Roteiro do teste final do Kaiky no PC, nas placas de referência e com a Fornalha 3D |

## Atualizar a Godot

```sh
git fetch https://github.com/godotengine/godot.git master   # ou a tag da versão
git checkout upstream && git merge --ff-only FETCH_HEAD && git push origin upstream
git checkout main && git merge upstream
```

Se o merge der conflito numa linha que o script de marca troca (por exemplo
`version.py` ou um ícone), fique com a versão da Godot e rode o script de novo:

```sh
git checkout --theirs version.py editor/icons/Logo.svg   # os arquivos em conflito
python erethea/tools/rebrand.py
python erethea/tools/rebrand.py --check
```

O `--check` também roda no CI. Se uma regra parar de bater porque a Godot mudou
o texto, ajuste a regra em `tools/rebrand.py`, nunca o arquivo de destino à mão.

Depois de juntar uma versão nova da Godot, troque também `GODOT_BASE_VERSION`
em `.github/workflows/erethea_reference_scene.yml` pela versão da Godot em que
a Erethea passou a se basear (veja "Cena de referência").

## Cena de referência

`tests/reference_scene/` é um projeto Godot com uma vila, uma floresta e um
personagem, de dia e de noite, montado todo por código (sem assets
importados). Ele serve para medir cada mudança visual lado a lado com a Godot
padrão, com capturas e tempo por quadro.

Para olhar a cena, abra o projeto e rode: as teclas 1, 2 e 3 trocam a vista,
N troca dia e noite, e o título da janela mostra o FPS. Para capturar:

```sh
erethea --path erethea/tests/reference_scene --fixed-fps 30 -- --capture="$PWD/capturas/erethea"
```

Use um caminho absoluto em `--capture` (como `"$PWD/..."`, que funciona no
terminal do Linux e no PowerShell): um caminho relativo vai parar dentro da
pasta do projeto.

Isso salva `dia_vila.png`, `dia_floresta.png`, `dia_personagem.png` e as três
de noite, em 1280×720, e um `results.json` com a placa de vídeo e o tempo por
quadro de cada vista. Para comparar duas capturas (por exemplo, a mesma cena
na Godot e na Erethea):

```sh
erethea --headless --path erethea/tests/reference_scene --script res://compare_captures.gd -- \
    --baseline="$PWD/capturas/godot" --candidate="$PWD/capturas/erethea" --out="$PWD/capturas/comparacao"
```

Cada imagem em `capturas/comparacao` mostra a base à esquerda, a comparada no
meio e a diferença, aumentada, à direita. O `summary.md` traz a diferença
(RMSE da luminância, de 0 a 255) e os tempos por quadro.

No CI, o job `🔥 Erethea reference scene` faz isso em todo PR: renderiza a
cena com a Godot oficial da versão base (`GODOT_BASE_VERSION`) e com a
Erethea do PR, na CPU com o lavapipe (Vulkan do Mesa), e falha se a Erethea,
com as configurações padrão, desenhar diferente da Godot. Isso garante que
projetos da Godot continuam com a mesma cara. As imagens ficam no artefato
`reference-scene` do job, e a tabela aparece no resumo do job. Os tempos do CI
vêm de um renderizador na CPU e só servem para comparar uma execução com
outra; o FPS de verdade se mede rodando a captura numa placa de vídeo.

## Presets de qualidade

A configuração do projeto `rendering/erethea/quality_preset` (em *Configurações
do Projeto › Rendering › Erethea*) muda de uma vez as configurações de
qualidade da própria Godot. O código fica em `modules/erethea_quality/`.

| Preset | Para quê |
| --- | --- |
| Disabled | Nada muda, igual à Godot. É o padrão, então projetos da Godot abrem assim. |
| Low (Leve) | Placas fracas e integradas: sombras menores e mais duras, oclusão de ambiente mais simples, FXAA, e o 3D desenhado em 77% da resolução e ampliado com FSR 1. |
| Balanced (Equilibrado) | Perto do padrão da Godot, com texturas mais nítidas (filtro anisotrópico 8×) e SMAA. |
| Cinematic (Cinema) | Placas fortes: sombras maiores e mais suaves, oclusão de ambiente e luz indireta em tamanho cheio e qualidade alta, filtro 16×, névoa volumétrica mais detalhada e FSR 2 na resolução nativa como antialiasing. |
| Automatic (Automático) | Escolhe pela placa: integrada, sem placa ou renderizador Compatibility, Low; placa dedicada sem ray tracing, Balanced; placa dedicada com ray tracing, Cinematic. Projetos novos criados na Erethea já começam assim. |

Regras:

- O preset só muda o que o projeto deixou no valor padrão. O que o projeto
  escolheu de propósito continua valendo.
- Vale para o jogo rodando (F5 ou exportado), não para o editor.
- Nos renderizadores Mobile e Compatibility, o preset só mexe em sombras,
  filtro de textura e LOD; os outros efeitos são do Forward+.
- O jogo pode trocar o preset enquanto roda, por exemplo no menu de opções:
  `EretheaQuality.apply_preset(EretheaQuality.PRESET_CINEMATIC)`. Os detalhes
  estão na referência da classe `EretheaQuality`, dentro do editor.

Os valores exatos de cada preset estão em
`modules/erethea_quality/erethea_quality.cpp`. Para ver um preset na cena de
referência, acrescente `--quality=low`, `balanced`, `cinematic` ou `automatic`
depois do `--`; o CI captura os três e mostra a diferença para o padrão.

Ainda não testado nas placas de referência: a escolha do Automático (a RX 580
deve cair em Balanced, e a RTX 4060 Ti ou 5060 Ti em Cinematic) e o FPS de
cada preset.

## Pós-processamento

O código fica em `modules/erethea_post/`. Tudo começa desligado.

**LUT de cor (`.cube`).** A Erethea importa arquivos `.cube`, o formato que o
DaVinci Resolve, o Photoshop e o Premiere exportam. O arquivo vira uma textura
3D que vai direto em *Environment › Adjustments › Color Correction* (com
*Adjustments* ligado). Funciona nos três renderizadores. Para fazer uma LUT:
tire um print do jogo, faça a correção de cor no programa e exporte a LUT em
sRGB / Rec. 709 (a LUT entra depois do tone mapping, como as outras texturas de
correção de cor da Godot). A tabela é reamostrada para 65×65×65 do jeito que o
renderizador lê, então uma LUT neutra não muda a imagem (na cena de referência,
a diferença foi de 0,2 a 0,4 de 255).

**Lens flare, vinheta, aberração cromática e grão de filme
(`EretheaPostEffect`).** É um `CompositorEffect`: crie um `Compositor` no
`WorldEnvironment` ou na `Camera3D`, adicione um `EretheaPostEffect` e ajuste a
intensidade de cada efeito (0 desliga). O lens flare faz reflexos coloridos
(ghosts) e um anel (halo) a partir do que passa do limite de brilho, como
lâmpadas e janelas acesas à noite.

**Motion blur (`EretheaMotionBlur`).** Outro `CompositorEffect`, que borra
cada pixel pelo movimento desde o quadro anterior: objetos que se mexem e o
movimento da câmera, inclusive no céu. Coloque-o antes do `EretheaPostEffect`
na lista do `Compositor`, para o grão não ser borrado. A intensidade é a
fração do quadro em que o "obturador" fica aberto (0,5 = obturador de 180°).

**Raios de luz (`EretheaLightShafts`).** Um `CompositorEffect` barato para o
sol: borra radialmente o céu claro em direção à posição do sol na tela (técnica
clássica de *light scattering* em espaço de tela), então os raios aparecem onde
árvores, casas ou um personagem cobrem parte do céu perto do sol. O sol é a
primeira `DirectionalLight3D` visível com energia positiva, ou a direção
configurável `custom_direction`. Só o céu (onde nada foi desenhado) acende os
raios, e só o que passa do limite de brilho (`threshold`); objetos
emissivos, como janelas, não fazem raios. Os raios somem de forma suave quando o
sol sai da tela e quando a câmera vira de costas para ele (aí o efeito nem
roda). Propriedades: `intensity` (0 desliga), `samples`, `density`, `decay`,
`tint` e `threshold`. Coloque-o antes do `EretheaPostEffect`, para o grão não
ser coberto. O custo é fixo por pixel, não depende da cena: `samples`
amostras (padrão 24), duas leituras de textura cada, mais uma cópia da imagem.
A prova de velocidade na RX 580 ainda não foi feita (o CI não tem placa de
vídeo); fica para o teste final.

**Névoa de altura.** A Erethea não reimplementa a névoa: a da Godot já
tem névoa de altura e a névoa volumétrica. Para ligar a de altura, no
`Environment` marque *Fog › Enabled* e dê um valor diferente de zero a *Height
Density* (a névoa fica mais densa abaixo de *Height*, a altura onde ela começa);
*Sun Scatter* clareia a névoa na direção do sol. A névoa volumétrica (*Volumetric
Fog*) faz a luz atravessar a névoa de verdade, mas custa mais; numa placa fraca
como a RX 580, a névoa de altura com os raios de luz acima é a opção barata
(ainda sem medida na placa).

Os efeitos de compositor só funcionam no Forward+; nos outros renderizadores
não fazem nada.
Os efeitos entram antes do TAA, do upscaling e do tone mapping. Os detalhes
estão na referência das classes, dentro do editor.

Para ver na cena de referência, importe o projeto uma vez e acrescente
`--post=all` (ou uma lista com `lut`, `flare`, `vignette`, `aberration`,
`grain`, `motion` e `shafts`) depois do `--`. Com `motion`, a câmera gira e o
personagem vira de um lado para o outro, e os dois estão no lugar de sempre, em
velocidade máxima, no quadro capturado. Com `shafts`, o sol é movido para ficar
baixo na frente da câmera, por cima das casas, das árvores e do
personagem (o mesmo vale para `--sun=front`, sem nenhum efeito, para comparar a
mesma cena com e sem os raios):

```sh
erethea --headless --import --path erethea/tests/reference_scene
erethea --path erethea/tests/reference_scene --fixed-fps 30 -- --post=all --capture="$PWD/capturas/pos"
```

O CI captura a cena com todos os efeitos e mostra a diferença para o padrão.
Ainda não medido nas placas de referência: o custo de cada efeito em FPS.

## Ray tracing

O código fica em `modules/erethea_raytracing/`. Tudo começa desligado e só
funciona no Forward+, numa placa com ray tracing (ray query): RTX da NVIDIA,
RX 6000 ou mais nova da AMD, Arc da Intel. Na RX 580, que não tem, nada muda.

**Sombras do sol por ray tracing (`EretheaRayTracedShadows`).** É um
`CompositorEffect`, como os de pós-processamento: crie um `Compositor` no
`WorldEnvironment` ou na `Camera3D` e adicione um `EretheaRayTracedShadows`.
Depois da passada de profundidade, sai de cada pixel um raio até o primeiro sol
(a primeira `DirectionalLight3D` com energia), contra a geometria até
`max_distance` metros da câmera, visível ou não (o que está fora da tela também
faz sombra). A sombra que sai disso vai para essa luz no lugar do mapa de
sombra; desligue *Shadow* na luz para não calcular os dois. Com `light_size`
(o tamanho do sol no céu, 0,5° por padrão) e `samples` raios por pixel, a
sombra fica firme onde encosta no chão e suave longe dele, como a do sol de
verdade. Com TAA ou FSR, o ruído das bordas some em poucos quadros.

Fazem sombra as malhas comuns (`MeshInstance3D` sem esqueleto) e as
`MultiMeshInstance3D` (árvores, grama, pedras); personagens com esqueleto e
partículas ainda não. Materiais com transparência por recorte (folhas) fazem a
sombra da superfície inteira.

Para ver na cena de referência, acrescente `--rt=shadows` depois do `--` (troca
o mapa de sombra do sol pelas sombras por ray tracing). O lavapipe do Linux
(Mesa 25.2) tem ray query, então o CI prova a imagem: de dia, ela tem que mudar
em relação à Erethea sem o efeito, e sem o efeito continua igual à Godot. A
velocidade numa placa de verdade (RTX) fica para o teste final.

```sh
erethea --path erethea/tests/reference_scene --fixed-fps 30 -- --rt=shadows --capture="$PWD/capturas/rt"
```

## Upscaling (AMD FSR 3.1 e 4, XeSS, DLSS)

A Godot tem o FSR 2.2 próprio. A Erethea pode trocá-lo pelo **AMD FSR**, por
meio do runtime oficial da AMD (FSR SDK 2.4.0, licença MIT): o runtime escolhe o
**FSR 4** nas placas que a AMD suporta (Radeon RX 6000 em diante, segundo a
AMD) e o **FSR 3.1** nas outras, inclusive a RX 580 e as GeForce.

Para ligar: *Projeto › Configurações do projeto › Rendering › Erethea ›
Temporal Upscaler* = `AMD FSR`, e no Viewport (ou em *Rendering › Scaling 3D*)
o modo `FSR 2.2` com a escala que quiser. A Godot ignora essa configuração e
continua no FSR 2.2, então o projeto abre igual nela.

Precisa de Windows e do Forward+, com o driver Vulkan ou o Direct3D 12 (o
padrão dos projetos novos no Windows), e do runtime da AMD para esse driver ao
lado do executável: `amd_fidelityfx_loader_vk.dll` e
`amd_fidelityfx_upscaler_vk.dll` para o Vulkan,
`amd_fidelityfx_loader_dx12.dll` e `amd_fidelityfx_upscaler_dx12.dll` para o
Direct3D 12. O instalador coloca os quatro, e a exportação para Windows copia
para o jogo só os do driver do projeto (*Rendering › Rendering Device › Driver*),
quando o projeto usa o AMD FSR. Num build próprio, baixe-os com:

```sh
python erethea/tools/fetch_amd_fsr.py bin
```

No Direct3D 12, o AMD FSR precisa de um driver de vídeo com *enhanced barriers*;
sem elas, avisa e volta para o FSR 2.2 (atualize o driver ou use o Vulkan).
Ainda não conferido se o driver da RX 580 tem. No Vulkan, o runtime da AMD só
põe as texturas numa memória de vídeo que a CPU não enxerga, que toda placa de
vídeo tem; renderizadores por software (lavapipe, SwiftShader) não têm, e aí a
Erethea avisa e usa o FSR 2.2. Sem os arquivos, em outro
sistema, com OpenGL ou em XR, também volta para o FSR 2.2 com um aviso. Os arquivos da AMD usam o runtime do Visual C++
2015-2022, que quase todo PC com jogos já tem; sem ele, também volta para o
FSR 2.2.

Para ver na cena de referência, acrescente `--upscaler=amd_fsr` (ou `fsr2`)
depois do `--`; ela renderiza a 67% e reconstrói na resolução da janela.
O CI roda a cena no Windows com o AMD FSR em drivers de CPU (lavapipe no
Vulkan, WARP no Direct3D 12), o que prova a integração com o FSR 3.1. No
lavapipe, uma camada de teste (`erethea/tests/vulkan_memory_alias`, só do CI)
cria a memória que só a placa enxerga como apelido da memória do lavapipe.
**Não testado na placa:** a qualidade e o custo em FPS na RX 580 e na RTX 4060
Ti/5060 Ti, e o FSR 4 (precisa de uma Radeon RX 6000 ou mais nova).

### XeSS

A outra opção é o **XeSS** da Intel (XeSS Super Resolution 2.0, do XeSS 3 SDK),
pelo `libxess.dll` da própria Intel: *Temporal Upscaler* = `XeSS`, com o modo
`FSR 2.2` no Viewport como no AMD FSR. Ele roda nas placas Intel Arc e Iris Xe
e, pelo caminho para outras marcas, em qualquer placa com Shader Model 6.4
(DP4a), o que inclui a RX 580 e as GeForce. A Erethea escolhe o preset de
qualidade do XeSS pela escala do Viewport; abaixo de 1/3 (33%) o XeSS não
reconstrói e a Erethea usa o FSR 2.2.

Funciona no **Direct3D 12** (o padrão dos projetos novos no Windows) e no
**Vulkan**. No Vulkan o XeSS precisa de extensões e recursos ligados quando a
Erethea cria o dispositivo da placa, então a Erethea faz isso ao abrir um
projeto que já está com o XeSS escolhido. Se você trocar para o XeSS com o
projeto aberto no Vulkan, ele avisa e usa o FSR 2.2 até a próxima vez que
abrir. O instalador coloca o `libxess.dll` (78 MB) e a licença da Intel
(`XESS_LICENSE.txt`, que deixa redistribuir o arquivo sem modificação), e a
exportação para Windows copia o arquivo para o jogo quando o projeto usa o
XeSS. Num build próprio:

```sh
python erethea/tools/fetch_xess.py bin
```

Na cena de referência, `--upscaler=xess` (no Vulkan, com um `override.cfg`
que escolhe o XeSS antes de abrir; veja o comentário em
`reference_scene.gd`). O CI roda a cena com o XeSS no Vulkan do lavapipe e no
Direct3D 12 do WARP e compara com o FSR 2.2. **Não testado na placa:** a
qualidade e o custo em FPS na RX 580 e na RTX 4060 Ti/5060 Ti, e numa placa
Intel.

### DLSS

A terceira opção é o **DLSS** da NVIDIA (DLSS Super Resolution), pelo
Streamline da própria NVIDIA (versão 2.14.16): *Temporal Upscaler* = `DLSS`,
com o modo `FSR 2.2` no Viewport como nos outros. Só roda nas placas NVIDIA RTX
(a RTX 4060 Ti/5060 Ti sim, a RX 580 não). A Erethea escolhe o modo do DLSS
(DLAA, Qualidade, Equilibrado, Desempenho, Ultra Desempenho) pela escala do
Viewport; fora das escalas do DLSS ela usa o FSR 2.2.

Por enquanto só no **Direct3D 12** (o padrão dos projetos novos no Windows); no
Vulkan avisa e usa o FSR 2.2. O Streamline tem que começar antes do dispositivo
da placa, então, como o XeSS no Vulkan, trocar para o DLSS com o projeto aberto
só vale depois de abrir de novo.

**A Erethea não vem com os arquivos do DLSS.** O código da integração e os
cabeçalhos do Streamline são MIT e estão aqui, mas as DLLs
(`sl.interposer.dll`, `sl.common.dll`, `sl.dlss.dll` e `nvngx_dlss.dll`) são da
NVIDIA, sob a licença *NVIDIA RTX SDKs License*, que só deixa distribuí-las
dentro do jogo de quem aceitou a licença. Então cada dev que quiser DLSS baixa
da NVIDIA, lendo e aceitando a licença:

```sh
python erethea/tools/fetch_streamline.py <pasta do erethea.exe> --accept-nvidia-license
```

Sem `--accept-nvidia-license`, o script só mostra o resumo da licença. Ele
baixa o pacote oficial da NVIDIA (280 MB), confere o SHA-256 de cada arquivo e
grava a licença ao lado (`nvngx_dlss.license.txt`). Com os arquivos ao lado do
editor, o projeto rodado pelo editor usa o DLSS, e a exportação para Windows
copia os arquivos para o jogo, avisando das obrigações da licença. Sem eles, a
exportação avisa e o jogo usa o FSR 2.2. A Erethea confere a assinatura da
NVIDIA no `sl.interposer.dll` antes de carregar.

Um jogo que sai com o DLSS tem que, segundo a licença da NVIDIA (leia a
licença inteira, isto é só um resumo):

- distribuir os arquivos só dentro do jogo, nunca sozinhos;
- citar o DLSS e pôr as marcas da NVIDIA na tela de abertura, no "Sobre" (se
  houver) e nos créditos;
- avisar a NVIDIA antes do lançamento comercial
  (<https://developer.nvidia.com/sw-notification>).

Na cena de referência, `--upscaler=dlss` com um `override.cfg` que escolhe o
DLSS antes de abrir (veja o comentário em `reference_scene.gd`). O CI baixa o
Streamline só para o teste, roda a cena no Direct3D 12 do WARP e confere que a
Erethea carrega o Streamline, que ele responde que a placa não tem DLSS, que a
Erethea volta para o FSR 2.2 com a mesma imagem e que fecha sem erro; os
arquivos da NVIDIA não vão para o artefato. **Não testado na placa:** o DLSS
reconstruindo de fato, a qualidade e o custo em FPS na RTX 4060 Ti/5060 Ti.

## Streaming de texturas

Com streaming, a placa de vídeo só guarda a resolução de textura que a câmera
está pedindo: uma parede longe fica com uma versão menor, e a textura sobe
para a resolução cheia quando a câmera chega perto. Isso economiza memória de
vídeo em cenas grandes. É o streaming de mipmaps da própria Godot 4.8
(`StreamedTexture2D`, ainda experimental na Godot); a Erethea só escolhe em
quais texturas ele entra. O código fica em `modules/erethea_streaming/`.

**Só nas texturas usadas em 3D.** A Godot só sabe a resolução que uma textura
precisa quando ela aparece num material 3D. Uma textura de interface ou de 2D
com streaming ficaria borrada, então essas continuam como na Godot. As texturas
são importadas do jeito normal; quando o editor vê uma delas num material 3D,
ela entra numa fila (o painel Saída avisa), e na próxima vez que o editor abrir
ela passa para o importador *Texture2D Streamed*. A troca espera o editor
reabrir porque trocar o tipo de um recurso aberto apagaria o histórico de
desfazer.

Uma textura só troca de importador se não usar nenhuma opção de importação que
o *Texture2D Streamed* não tem (por exemplo, limite de tamanho, inverter o Y do
normal map ou remapear canais), e se não tiver transparência: o importador de
streaming não corrige as bordas transparentes, o que deixaria contorno em
folhagem. O painel Saída diz por que uma textura ficou de fora.

**Ligar e desligar.** Projetos novos criados na Erethea já começam com
`rendering/textures/streaming/enabled` (da Godot) e
`rendering/erethea/stream_3d_textures` ligados, menos os que usam o
renderizador Compatibility, que não tem streaming. Projetos da Godot abrem com
os dois desligados, como estavam. Desligar o da Godot para o streaming de
todas as texturas (elas carregam inteiras); desligar o da Erethea só para de
trocar texturas novas. Uma textura que já trocou volta para o importador normal
no painel Importar (*Importar como › Texture2D*). Se a mesma imagem também
aparece na interface ou num `Decal`, vale fazer isso com ela.

**Teste.** `erethea/tests/texture_streaming` tem uma textura de tijolos num
material 3D e um ícone na interface. O CI abre a cena no editor, confere que
só os tijolos entraram na fila, reabre, e roda o projeto: os tijolos começam
com 64 pixels de largura, sobem até os 512 com a câmera perto, ficam iguais à
mesma imagem sem streaming (RMSE 0 no lavapipe) e descem quando somem da tela.
O ícone continua como na Godot.

Ainda não testado nas placas de referência: quanta memória de vídeo economiza
num jogo de verdade e se a textura demora a ficar nítida quando a câmera corre.

## Servidor MCP do editor

O editor da Erethea pode abrir um servidor MCP (Model Context Protocol) para
que um agente como o Claude Code mexa no projeto aberto do mesmo jeito que uma
pessoa: abrir e criar cenas, criar nós, escrever e anexar scripts, salvar,
rodar o jogo, ler os erros e os `print` do jogo e tirar capturas de tela. O
código fica em `modules/erethea_mcp/`. O MCP da Fornalha cuida do projeto e dos
assets; o do editor cuida do que só o editor aberto sabe fazer.

**Ligar.** Vem desligado. Ligue em *Configurações do Editor › Erethea › MCP*
(`erethea/mcp/enabled`); a porta padrão é 11481 (`erethea/mcp/port`). Também
dá para ligar só numa execução com a variável de ambiente `ERETHEA_MCP_PORT`,
que liga o servidor e escolhe a porta. Quando ele sobe, o painel Saída mostra
`Erethea MCP: server listening on http://127.0.0.1:11481/mcp`. Para conectar o
Claude Code:

```sh
claude mcp add --transport http erethea http://127.0.0.1:11481/mcp
```

| Ferramenta | O que faz |
| --- | --- |
| `get_editor_state` | Projeto, cena principal, cenas abertas e não salvas, se o jogo está rodando |
| `get_scene_tree` | Árvore de nós da cena aberta, com tipo, script e cena instanciada |
| `get_node_properties` | Propriedades de um nó |
| `open_scene`, `create_scene` | Abre uma cena ou cria uma nova com o nó raiz pedido |
| `create_node` | Cria um nó (classe ou `class_name`) ou instancia uma cena, já com propriedades |
| `set_node_properties` | Muda propriedades; aceita `[1, 2, 3]`, `"Vector3(1, 2, 3)"`, `"#ff8800"`, nomes de enum e recursos |
| `delete_node` | Apaga um nó (nunca a raiz) |
| `write_script` | Escreve um script e devolve os erros com linha e coluna |
| `list_files`, `read_file` | Lista os arquivos do projeto e lê arquivos de texto (scripts, cenas, `game.project.json`) aos pedaços |
| `edit_file` | Troca um trecho de um script ou de outro arquivo de texto (não cenas), conferindo o script como o `write_script` |
| `attach_script` | Anexa um script a um nó |
| `save_scene` | Salva a cena aberta |
| `run_game`, `stop_game` | Roda a cena principal, a aberta ou outra, e para |
| `get_errors` | Erros, avisos e `print` do editor e do jogo, a partir de um ponto |
| `take_screenshot` | Captura do jogo, do editor ou da vista 2D ou 3D, em PNG |
| `run_test` | Joga o jogo no modo de teste e devolve o relatório (veja abaixo) |
| `rescan_files` | Faz o editor ver arquivos criados fora dele |
| `check_export` | Roda as validações antes de exportar (veja abaixo) |
| `build_fornalha_scenes` | Monta as cenas do jogo pelo `game.project.json` (veja abaixo) |

Tudo o que muda a cena entra no histórico de desfazer com o nome
`MCP: ...`, então Ctrl+Z desfaz o que o agente fez. As mudanças só vão para o
disco quando o agente (ou a pessoa) salva.

**Segurança.** O servidor só escuta em `127.0.0.1`, recusa pedidos com `Host`
ou `Origin` que não sejam da própria máquina (o que impede um site aberto no
navegador de mexer no editor) e só aceita caminhos dentro do projeto.

**Para a Fornalha.** `GET http://127.0.0.1:11481/status` responde sem sessão
com o nome do projeto aberto, a pasta e se o jogo está rodando.

**Teste.** `erethea/tests/mcp/test_mcp.py` abre o editor com o servidor ligado
num projeto vazio e faz o que um agente faria: cria cenas, nós e um script,
salva, roda o jogo, lê o `print` e o erro do jogo (com arquivo e linha) e tira
capturas. O CI roda no Linux sem janela e com janela (Xvfb e lavapipe, onde as
capturas têm que ser imagens de verdade) e no Windows sem janela. As capturas
e os logs ficam no artefato `erethea-mcp-linux`.

```sh
python erethea/tests/mcp/test_mcp.py bin/erethea.linuxbsd.editor.x86_64 --out capturas/mcp
```

## Modo de teste para agentes

O jogo pode rodar sozinho por alguns segundos, com os controles apertados por
um roteiro, e escrever um relatório com o que aconteceu: os erros (com a linha
do script), os quadros por segundo e até onde o jogador foi. É o que um agente
(ou o `run_test` da Fornalha) precisa para saber se o jogo funciona sem ninguém
jogar. O código fica em `modules/erethea_agent_test/` e vale para qualquer
projeto: o jogo não precisa mudar nada.

```sh
erethea --path meu_jogo --headless --agent-test=relatorio --agent-test-seconds=10
erethea --path meu_jogo --agent-test=relatorio --agent-test-inputs=controles.json
```

| Opção | O que faz |
| --- | --- |
| `--agent-test=<pasta>` | Liga o modo de teste; o relatório vai para `<pasta>/report.json` (sem pasta, `user://agent_test`) |
| `--agent-test-seconds=<s>` | Quanto tempo o jogo roda (padrão 10) |
| `--agent-test-inputs=<arquivo>` | Roteiro dos controles, em JSON |
| `--headless` | Sem janela (a opção da Godot): mais rápido, sem capturas nem números de desenho |

**Controles.** O roteiro é uma lista de passos; cada um começa em `at`
segundos, fica apertado por `for` segundos (padrão 0,1) e é uma ação de input
(`action`, com `strength` de 0 a 1 se quiser), uma tecla (`key`, como `"W"` ou
`"Space"`), um botão do mouse (`mouse_button`: `left`, `right`, `middle`,
`wheel_up`, `wheel_down`) ou um movimento do mouse (`mouse_motion`: `[x, y]` em
pixels, espalhado pelo tempo do passo). `screenshots` diz em que segundos tirar
capturas (só com janela).

```json
{
  "screenshots": [1, 3.5],
  "steps": [
    {"at": 0.5, "for": 2, "action": "move_forward"},
    {"at": 3, "action": "jump"},
    {"at": 3.5, "for": 0.5, "key": "W"},
    {"at": 4, "for": 0.5, "mouse_motion": [200, 0]}
  ]
}
```

Sem roteiro, o teste aperta as ações de input do próprio projeto, uma de cada
vez. Num projeto da Fornalha, faz a mesma caminhada do teste da Fornalha
(frente, pula, vira e ataca).

**Relatório.** `report.json` é reescrito a cada segundo, então mesmo um jogo
que trava deixa o que aconteceu até ali (`finished: false`). No fim, a mesma
coisa sai numa linha da saída, depois de `ERETHEA_AGENT_TEST `.

| Campo | O que tem |
| --- | --- |
| `passed` | `true` se o teste chegou ao fim, sem erros e sem o jogador cair do mundo |
| `ended` | Por que acabou: `time`, `game_quit` (o jogo fechou sozinho), `bad_inputs`, `bad_options` ou `real_time_limit` |
| `errors`, `warnings` | Cada erro e aviso, com mensagem, arquivo, linha, `script_file` e `script_line` (onde o script estava), quantas vezes e quando apareceu pela primeira vez |
| `fps` | Média, mínimo, 1% mais lento, mediana e maior tempo de quadro (sem os primeiros quadros, que carregam a cena) |
| `rendering` | Chamadas de desenho, primitivas e memória de vídeo (só com janela) |
| `player` | O primeiro nó do grupo `player` (ou `jogador`, nos projetos da Fornalha): onde começou e terminou, quanto andou, se sumiu e se caiu do mundo |
| `inputs` | Cada passo do roteiro e se foi feito (`done`) ou pulado, com o motivo (ação que não existe) |
| `screenshots` | As capturas tiradas |
| `memory` | Pico de memória, objetos e nós |

O jogo sai com 0 se passou, 1 se não passou e 2 se o roteiro ou as opções
estão errados. Pelo servidor MCP do editor, a ferramenta `run_test` faz tudo
isso com a cena principal, a aberta ou outra, espera o fim e devolve o
relatório (com a última captura, se rodou com janela); dura até 40 segundos.

**Teste.** `erethea/tests/agent_test/test_agent_test.py` roda um jogo pequeno
(um jogador que anda e pula) com roteiro, com um erro, sem roteiro, com um
roteiro errado, fechando sozinho e com janela, e confere cada relatório. O CI
roda no Linux (com janela no Xvfb) e no Windows. O teste do servidor MCP chama
`run_test` também.

```sh
python erethea/tests/agent_test/test_agent_test.py bin/erethea.linuxbsd.editor.x86_64 --window
```

## Validações antes de exportar

Antes de exportar, a Erethea confere o que costuma estragar um jogo feito com
assets gerados: modelo pesado demais, textura grande demais, modelo na escala
errada e asset sem recibo de licença. O código fica em
`modules/erethea_export_checks/`. Os limites ficam em *Projeto › Configurações
do Projeto › Editor › Erethea Export Checks* (com *Configurações Avançadas*
ligado).

| Confere | Padrão | Configuração |
| --- | --- | --- |
| Triângulos de um personagem ou inimigo | até 100 mil | `max_character_triangles` |
| Triângulos de qualquer outro modelo | até 500 mil | `max_model_triangles` |
| Lado maior de uma textura (também as de dentro do `.glb`) | até 4096 px | `max_texture_size` |
| Altura de um personagem ou inimigo | de 0,3 a 4 m | `character_height` |
| Lado maior de qualquer outro modelo | de 5 mm a 5 km | `model_size` |
| Pastas de personagens | `Characters` e `Enemies` | `character_folders` |
| Recibo de licença de cada asset | ligado | `license_receipts` |

A escala pega o erro mais comum de exportação do Blender: um personagem de
180 m foi exportado em centímetros. Os triângulos contam o modelo inteiro, sem
os LODs que a Godot cria na importação.

**Recibos de licença.** Em projetos da Fornalha (com `game.project.json` ao
lado do `project.godot`), a Erethea lê `ai/recibos.jsonl`, o banco de recibos
que a Fornalha grava, e confere os mesmos arquivos que a Fornalha confere: as
imagens, modelos, sons e vídeos das pastas `World`, `Characters`, `Enemies`,
`Items`, `Weapons`, `Animations`, `Audio`, `VFX`, `Cutscenes` e `UI`. Uma
textura que a Godot tirou de dentro de um `.glb` usa o recibo do `.glb`.

- **Erro**, num projeto comercial (o padrão da Fornalha): asset sem recibo, ou
  feito com um modelo cuja licença não deixa vender.
- **Aviso**: licença com condições (aceite ou teto de receita, que ficam na
  Fornalha, então confira lá), ou arquivo que mudou depois do recibo (editado à
  mão ou limpo no Blender).

Os outros problemas (triângulos, texturas, escala) são avisos.

**Quando roda.**

- Na exportação, em projetos da Fornalha. Projetos da Godot exportam como antes;
  para ligar neles, troque `run_on_export` para *All Projects* (ou *Never* para
  desligar de vez). Cada problema aparece no log da exportação. A Godot não
  deixa um plugin parar a exportação, então o arquivo é gerado mesmo com erro,
  mas o log mostra *Falhou* e lista os erros.
- A qualquer hora em *Projeto › Ferramentas › Check Project for Export...*: uma
  lista com cada problema; dois cliques numa linha mostram o arquivo.
- Pelo agente, com a ferramenta `check_export` do servidor MCP, que pode
  conferir só o que um preset de exportação leva.

**Teste.** `erethea/tests/export_checks/test_export_checks.py` monta um projeto
como os da Fornalha, com um herói certo, um gigante em centímetros, um inimigo
pesado feito com um modelo que não deixa vender, uma casa sem recibo, uma
textura grande com licença condicional e uma textura que mudou depois do
recibo. Exporta um `.pck` pela linha de comando e confere que o log aponta cada
problema, e nada do herói. Depois tira o `game.project.json` e confere que a
exportação de um projeto da Godot não roda nenhuma validação. O CI roda no
Linux e no Windows.

```sh
python erethea/tests/export_checks/test_export_checks.py bin/erethea.linuxbsd.editor.x86_64
```

## Cenas montadas pelo `game.project.json`

Num projeto da Fornalha, a Erethea monta as cenas do jogo a partir do
`game.project.json` e do que a Forja deixou no projeto: os personagens em
`Characters/` e `Enemies/`, o blockout do mundo em `World/blockout.glb` (com
`World/blockout.info.json`, que diz onde fica cada área e onde nasce cada
inimigo) e as escolhas da última montagem em `.fornalha/cena.json`. O código
fica em `modules/erethea_fornalha/`.

| Cena | O que tem |
| --- | --- |
| `Characters/<id>/<id>.tscn` | O jogador: o modelo, a cápsula de colisão, a câmera por cima do ombro e o script `jogador.gd` da Fornalha |
| `Enemies/<id>/<id>.tscn` | O inimigo: o modelo, a colisão e o script `inimigo.gd` da Fornalha (persegue, ataca e morre) |
| `World/areas/<área>.tscn` | Uma por área do blockout (vila, floresta...), com os inimigos dela; é onde vai o que for daquela área |
| `World/mundo.tscn` | O blockout, o sol, o céu e as áreas, cada uma no seu lugar |
| `principal.tscn` | O mundo, o jogador no ponto de nascimento e o HUD |

São os mesmos nomes de nós e de arquivos que a montagem da Fornalha usa, então
o `run_test` da Fornalha continua funcionando. O modelo entra como instância
do `.glb`, então quando a Fornalha gera o modelo de novo a cena mostra o novo
sem perder colisão, câmera e script.

**O que o dev mudou fica.** A Erethea guarda em `res://.erethea/fornalha_import.json`
como deixou cada cena. Na próxima montagem, uma cena que alguém mudou depois,
ou que a Erethea não fez (como a `principal.tscn` da montagem da Fornalha),
fica como está, e o relatório diz isso. Montar com *force* (ou o botão
*Replace Kept Scenes*) monta essas também e guarda uma cópia das antigas em
`res://.erethea/backup/`. Uma cena aberta no editor com mudanças não salvas
nunca é trocada.

**Animações em loop.** O glTF não tem como dizer que uma animação é em loop,
então as animações de andar da Forja chegavam na Godot tocando uma vez só. Em
projetos da Fornalha, a importação deixa em loop as animações com os nomes de
`editor/erethea_fornalha/looping_animations` (padrão: `parado`, `andar`,
`correr` e `correr_rapido`). O que a pessoa escolher nas configurações de
importação da animação vale mais.

**Como montar.**

- Sozinha, quando muda o que as cenas usam, como um personagem novo da
  Fornalha (veja "Elo ao vivo com a Fornalha", abaixo).
- Em *Projeto › Ferramentas › Build Scenes from game.project.json...*, que só
  aparece em projetos da Fornalha: uma lista com o que foi criado, atualizado ou
  mantido; dois cliques abrem a cena.
- Pelo agente, com a ferramenta `build_fornalha_scenes` do servidor MCP.

**Teste.** `erethea/tests/fornalha_import/test_fornalha_import.py` monta um
projeto como os da Fornalha, com um herói, um lobo e um blockout de duas áreas
em `.glb`, e scripts que fazem o papel dos da Fornalha. Pelo servidor MCP,
monta as cenas, confere que a segunda montagem não muda nada, que a
`principal.tscn` que a Erethea não fez fica até montar com *force* (e que a
cópia vai para o backup) e que uma área mudada à mão fica. Depois roda o jogo
sem janela: o herói anda mais de 2 m no chão tocando `andar` em loop, e cada
lobo está na cena da sua área. O CI roda no Linux e no Windows.

```sh
python erethea/tests/fornalha_import/test_fornalha_import.py bin/erethea.linuxbsd.editor.x86_64
```

## Elo ao vivo com a Fornalha

Num projeto da Fornalha aberto no editor, o que a Fornalha gera aparece sozinho
na cena: ela avisa que gravou arquivos novos, o editor importa e, quando muda o
que as cenas usam (um personagem ou inimigo novo, um modelo gerado de novo, o
blockout, as áreas do `game.project.json`), monta as cenas de novo como em
"Cenas montadas pelo `game.project.json`". Cena que alguém mudou à mão fica como
está. As cenas abertas recarregam, e num projeto novo a `principal.tscn` abre
sozinha.

**Quando monta sozinha.** Se a Erethea já montou as cenas deste projeto alguma
vez (existe `res://.erethea/fornalha_import.json`), monta de novo sempre que o
que as cenas usam muda. Se nunca montou, só monta sozinha num projeto que ainda
não tem `principal.tscn`. Num projeto cujas cenas a Fornalha monta ela mesma
(`montar.gd`), a Erethea espera alguém montar uma vez pelo menu ou pelo agente.

**Configurações.** Só valem em projetos com `game.project.json`; projetos da
Godot não mudam.

| Onde | Configuração | Padrão |
| --- | --- | --- |
| Configurações do Projeto | `editor/erethea_fornalha/build_automatically`: montar as cenas sozinha | ligado |
| Configurações do Projeto | `editor/erethea_fornalha/live_link`: perguntar à Fornalha o que ela gravou | ligado |
| Configurações do Projeto | `editor/erethea_fornalha/area_acoustics`: eco de cada área (veja "Som") | ligado |
| Configurações do Projeto | `editor/erethea_fornalha/interactive_music`: música que muda no fim do compasso (veja "Som") | ligado |
| Configurações do Editor | `erethea/fornalha/port`: porta da API da Fornalha | `11480` |

A variável de ambiente `ERETHEA_FORNALHA_PORT` troca a porta, como nos testes.

**Como a Erethea fala com a Fornalha.** Só em `127.0.0.1`, na porta da API da
Fornalha.

1. A cada 2 segundos, como o add-on `fornalha_ponte` faz hoje:
   `GET /fornalha/ponte?projeto=<pasta do projeto>`, que responde
   `{"versao": 3, "arquivos": ["Characters/heroi/heroi.glb"]}`. Quando a
   `versao` muda, o editor importa os arquivos novos.
2. Quando essa resposta tiver `"websocket": "/fornalha/ponte/ws"`, o editor
   abre esse WebSocket e para de perguntar por HTTP. Se o WebSocket fechar,
   volta a perguntar por HTTP.

Mensagens pelo WebSocket, em texto JSON:

| De | Mensagem | Quando |
| --- | --- | --- |
| Erethea | `{"tipo": "ola", "programa": "Erethea Engine", "versao": "...", "projeto": "<pasta>", "monta_cenas": true}` | Ao abrir. `monta_cenas` diz que a Erethea monta as cenas, então a Fornalha não precisa rodar o `montar.gd` |
| Fornalha | `{"tipo": "ponte", "versao": 4, "arquivos": [...]}` | Ao abrir e sempre que gravar arquivos no projeto |
| Erethea | `{"tipo": "cenas", "ok": true, "summary": "...", "scenes": [{"file": "...", "result": "created"}], "warnings": [...]}` | Depois de montar sozinha; é o mesmo relatório da ferramenta `build_fornalha_scenes` |

Hoje a Fornalha só tem o `GET /fornalha/ponte`, então a Erethea pergunta por
HTTP. O WebSocket, o campo `"websocket"` na resposta e deixar de rodar o
`montar.gd` quando a Erethea disser `monta_cenas` são pedidos para a Fornalha.

**Teste.** `erethea/tests/fornalha_link/test_fornalha_link.py` sobe uma
Fornalha de mentira (o `GET /fornalha/ponte` e o WebSocket) e abre o editor num
projeto novo que tem só o herói e o blockout. Confere que o editor monta as
cenas sozinho; que, por HTTP, um lobo que a Fornalha grava é importado e ganha
a sua cena e o seu lugar nas áreas; e que, quando a Fornalha anuncia o
WebSocket, o editor diz `ola`, recebe o próximo inimigo por ele e devolve o
relatório das cenas. Depois roda o jogo sem janela e o herói anda. O CI roda no
Linux e no Windows.

```sh
python erethea/tests/fornalha_link/test_fornalha_link.py bin/erethea.linuxbsd.editor.x86_64
```

## Painel de IA

Uma aba **Assistant** no lado direito do editor (ao lado do Inspetor, pode ser
fechada) conversa com um modelo que enxerga a cena aberta e o
`game.project.json` e age no editor. Pedidos como "coloque 10 inimigos na
floresta" ou "faça o sistema de combate" viram nós, propriedades, cenas e
scripts GDScript, e cada resposta pode ser desfeita. O código fica em
`modules/erethea_assistant/`.

**Quem responde.** Escolha no topo do painel (ou em *Configurações do Editor ›
Erethea › Assistant*, `erethea/assistant/provider`):

| Provedor | Como fala | Configurações |
| --- | --- | --- |
| Ignis (padrão, neste computador) | `POST http://127.0.0.1:11470/v1/chat/completions`, modelo `default`. Como modelos pequenos podem não saber usar "tools", a resposta é um JSON preso por um esquema, como a Fornalha faz com o Ignis: `{"reply": "...", "actions": [{"tool": "...", "arguments": {...}}]}`. O painel tira o `<think>...</think>` do Qwen3, confere cada ação contra o esquema da ferramenta (argumento que não existe ou do tipo errado sai, ação sem os obrigatórios é deixada de lado e o painel avisa), executa e manda os resultados de volta, por até 4 rodadas. Só vão as últimas 6 mensagens, a árvore da cena até ~150 nós e o `game.project.json` até ~6000 caracteres | `erethea/assistant/ignis/url` |
| Claude (API da Anthropic) | `POST https://api.anthropic.com/v1/messages`, `anthropic-version: 2026-10-21`, modelo `claude-opus-5-5`, ferramentas nativas com `tool_choice` `auto`, cache de prompt e `fallbacks: "default"` (beta `server-side-fallback-2026-07-01`; se a API recusar, o painel tenta de novo sem). O histórico nunca é editado, porque o Opus 5.5 prende os blocos de pensamento à conversa: o prompt de sistema e as ferramentas ficam fixos, o contexto do editor vai em cada pergunta e a resposta do Claude volta byte a byte. Se a API disser que um pensamento é "bound to a different conversation", o painel tira os blocos de pensamento e tenta uma vez | `erethea/assistant/claude/api_key` (ou a variável `ANTHROPIC_API_KEY`), `erethea/assistant/claude/model` |
| Servidor compatível com OpenAI | `POST <url>/chat/completions` com `tools` do tipo `function` (LM Studio, Ollama, um serviço na nuvem) | `erethea/assistant/openai/url`, `erethea/assistant/openai/api_key`, `erethea/assistant/openai/model` |
| Claude Code (sua assinatura) | O Claude Code instalado no computador, com o login da assinatura do Claude, sem chave de API. O painel deixa um Claude Code aberto por conversa, sem janela, com mensagens em JSON por linha na entrada e na saída (`-p --input-format stream-json --output-format stream-json --verbose`, como o SDK oficial), ligado só ao servidor MCP do editor (`--mcp-config`, `--strict-mcp-config`), sem as ferramentas próprias de arquivos e terminal (`--tools=`), com as do editor liberadas (`--allowedTools mcp__erethea`) e o resto recusado sem perguntar (`--permission-mode dontAsk`). O servidor MCP liga sozinho se estiver desligado. Se o Claude Code fechar, a próxima pergunta abre outro na mesma sessão (`--resume`). A variável `ANTHROPIC_API_KEY` não passa para ele, para valer a assinatura | `erethea/assistant/claude_code/path` (vazio: procura em `~/.local/bin`, onde o instalador põe, e no `PATH`), `erethea/assistant/claude_code/arguments` (argumentos antes dos do Claude Code, como `claude` com `wsl` no caminho), `erethea/assistant/claude_code/model` (vazio: o modelo padrão do plano) |

Para usar o Claude Code: instale (no Windows, no PowerShell:
`irm https://claude.ai/install.ps1 | iex`; não precisa de Node nem de
administrador), rode `claude` uma vez num terminal para entrar com a conta do
Claude e escolha "Claude Code (your subscription)" no painel.

Em 429, 500, 502, 503 e 529 o painel tenta mais uma vez depois de 3 s. Uma
recusa do Claude aparece no painel e a pergunta sai do histórico.

**O que o modelo vê.** Em cada pergunta: o nome do projeto, se é da Fornalha,
as cenas abertas, a cena editada com a árvore resumida, os nós selecionados e
o `game.project.json` (para o Claude e o servidor OpenAI, só quando ele muda; o
Ignis recebe sempre). As ferramentas são as mesmas do servidor MCP (tabela
acima), chamadas sem passar pela rede, e o servidor MCP não precisa estar
ligado. No máximo 30 ferramentas por pergunta. O prompt de sistema pede que o
modelo responda na língua da pessoa, salve as cenas que mudar e, em projetos
da Fornalha, prefira mudar as propriedades exportadas nas cenas ou o
`game.project.json` e chamar `build_fornalha_scenes` em vez de mexer nos
scripts de `Scripts/fornalha/`.

**Usar.** Escreva no campo de baixo e aperte Enter (Shift+Enter quebra a
linha). Enquanto responde, o botão Send vira Stop. Cada ação aparece em cinza e
os erros em vermelho. *New* começa outra conversa.

**Desfazer.** *Undo last answer* desfaz a última resposta que mudou algo:

- nós e propriedades voltam pelo histórico de desfazer da cena (as mudanças
  entram nele com o nome `Assistant: ...`, então Ctrl+Z também funciona);
- cena salva pela resposta (por `save_scene`, `run_game` ou `run_test`) é
  salva de novo depois que o histórico volta;
- scripts e outros arquivos voltam ao conteúdo de antes, e os que a resposta
  criou são apagados;
- cena criada pela resposta é fechada e apagada;
- cenas que o `build_fornalha_scenes` regravou voltam ao arquivo antigo e
  recarregam.

Se a pessoa mexeu na mesma cena ou no mesmo arquivo depois da resposta, o
painel não desfaz nada e avisa, para não apagar o trabalho dela.

**Teste.** `erethea/tests/assistant/test_assistant.py` sobe um servidor de
mentira que responde como a API da Anthropic, um servidor OpenAI e o Ignis,
usa um Claude Code de mentira (`fake_claude.py`, que fala o mesmo formato e age
pelo servidor MCP), e
abre o editor num projeto com um plugin (`addons/assistant_driver`) que escreve
no painel e aperta Send, New e Undo. Pelo MCP e pelos arquivos do projeto, o
teste confere que as respostas criaram nós, salvaram a cena e escreveram e
mudaram scripts, e que o desfazer voltou tudo; pelo servidor de mentira, que a
pergunta levou o contexto do editor, que a resposta do Claude voltou byte a
byte e que os resultados das ferramentas chegaram certos. Com o Claude Code, o
teste confere a linha de comando, que a chave de API não chega a ele, que a
segunda pergunta vai para o mesmo processo, que ele volta na mesma sessão
depois de fechar e que o desfazer pega o que ele fez pelo MCP. O CI roda no
Linux e no Windows. Os testes de unidade (`--test --test-case="*Assistant*"`)
cobrem a montagem dos pedidos e a leitura das respostas dos quatro provedores.
A conversa de verdade com o Ignis, com a API do Claude e com o Claude Code fica
para o teste final no PC do criador.

```sh
python erethea/tests/assistant/test_assistant.py bin/erethea.linuxbsd.editor.x86_64
```

## Prévia para a Fornalha

A Fornalha pode mostrar o asset que gerou **já com o renderizador da Erethea**,
do jeito que vai ficar no jogo: o modelo em pé num chão, com luz de sol e
sombra, céu procedural e uma câmera que enquadra o modelo e gira devagar em
volta. O código fica em `modules/erethea_preview/` (a classe `EretheaPreview`)
e funciona de duas formas: como **executável**, com `--erethea-preview=`, e
**embutida** na Fornalha pela LibGodot. As duas mostram a mesma cena. Sem o
argumento, nada muda na engine.

### Como rodar o executável

```sh
erethea --erethea-preview=dragao.glb
erethea --erethea-preview=dragao.glb --erethea-preview-capture=captura.png
```

Não precisa de projeto (nem de `--path`): o `main.cpp` reconhece o argumento,
não abre o gerenciador de projetos e usa o laço principal `EretheaPreviewLoop`,
que monta a cena sozinho. O arquivo é lido em tempo de execução com
`GLTFDocument`, então **não precisa de importação do editor**. Aceita `.glb` e
`.gltf` (com o `.bin` e as texturas ao lado). Caminho relativo vale a partir da
pasta atual. Nos templates de exportação (sem o editor) a engine não acha um
projeto sozinha: aponte uma pasta com `project.godot` por `--path`.

| Opção | O que faz |
| --- | --- |
| `--erethea-preview=<arquivo>` | Liga a prévia e mostra o `.glb`/`.gltf` |
| `--erethea-preview-capture=<arquivo.png>` | Depois de alguns quadros, salva a imagem da janela, escreve `capture saved` e fecha. Sai com 0 se salvou e com 1 se não deu (arquivo ruim, `--headless`, sem imagem). A câmera fica parada, no mesmo ângulo sempre |
| `--erethea-preview-frames=<n>` | Quantos quadros esperar antes da captura (padrão 10, mínimo 3). Conta de novo a cada troca de asset |
| `--erethea-preview-control=<arquivo.txt>` | Arquivo de texto que a Fornalha reescreve com o caminho de **outro asset**: a prévia o troca sem reiniciar (veja abaixo) |
| `--erethea-preview-quality=<low\|balanced\|cinematic\|automatic>` | Aplica um preset de qualidade (veja "Presets de qualidade") |

Os argumentos da Godot valem também: `--resolution 1280x720`, `--position`,
`--rendering-driver vulkan`, `--fixed-fps 30`, `--audio-driver Dummy`.

**Trocar o asset sem reiniciar.** Há três jeitos, todos sem fechar a janela:

1. **Escrever o caminho novo** no arquivo de `--erethea-preview-control=`. A
   prévia lê o arquivo duas vezes por segundo; quando o texto muda, ela carrega
   o novo caminho. É o jeito para quem embute pela LibGodot, que não tem como
   chamar funções da cena por fora.
2. **Regravar o mesmo arquivo.** A prévia vê que a data do `.glb` mudou, espera
   ele parar de mudar (para não ler um arquivo pela metade) e recarrega. Serve
   para a Fornalha refazer o mesmo asset.
3. **Pelo código** (GDScript, C# ou GDExtension): `EretheaPreview.load_asset(caminho)`.
   Se o arquivo for ruim, devolve um erro e o asset anterior continua na tela.

Cada vez que um asset aparece, a prévia imprime
`Erethea preview: showing <caminho>`.

**Qualidade.** Por padrão a prévia não aplica nenhum preset: usa o preset do
projeto, que é "Disabled" (`EretheaQuality`), e portanto desenha como um jogo
novo com as configurações padrão (era o que a Fornalha pediu: ver o asset "como
vai ficar no jogo"). Para ver o resultado com mais qualidade, use
`--erethea-preview-quality=cinematic` (ou outro).

### Como embutir pela LibGodot

A base 4.8 já traz a LibGodot (`core/extension/libgodot.h`). A Erethea só
compila a biblioteca; nada foi mudado na API:

```sh
scons platform=windows target=editor library_type=shared_library   # bin/erethea.windows.editor.x86_64.dll
scons platform=linuxbsd target=editor library_type=shared_library  # bin/liberethea.linuxbsd.editor.x86_64.so
```

Use `target=editor` (a prévia usa o importador de glTF e o resto do editor
completo). O CI compila as duas e guarda como artefatos `erethea-libgodot-linux`
e `erethea-libgodot-windows`. O programa `erethea/tests/libgodot/libgodot_preview.c`
é um exemplo completo em C (a Fornalha faz o mesmo em Rust, com `libloading`):

1. **Carregar a biblioteca** (`LoadLibrary`/`dlopen`) e achar
   `libgodot_create_godot_instance` e `libgodot_destroy_godot_instance`.
2. **Criar a instância**: `libgodot_create_godot_instance(argc, argv, init)`.
   `argv[0]` é o nome do programa que embute (como no `main()`), os outros são os
   argumentos da engine:

   ```text
   argv = ["fornalha", "--audio-driver", "Dummy",
           "--erethea-preview=C:/assets/dragao.glb",
           "--erethea-preview-control=C:/temp/prévia.txt"]
   ```

   `init` é a função de entrada GDExtension do programa (assinatura
   `GDExtensionInitializationFunction`): a engine a chama durante a criação e
   entrega o `get_proc_address`, que dá acesso a toda a API C. Guarde esse
   ponteiro; a função deve preencher `r_initialization` com
   `minimum_initialization_level = GDEXTENSION_INITIALIZATION_CORE` e dois
   callbacks (`initialize` e `deinitialize`) que podem ficar vazios, e devolver 1.
3. **Iniciar**: com o `get_proc_address`, pegue `string_name_new_with_latin1_chars`,
   `classdb_get_method_bind` e `object_method_bind_ptrcall`; ache o método
   `GodotInstance.start` (`classdb_get_method_bind("GodotInstance", "start", hash)`)
   e chame com `object_method_bind_ptrcall(metodo, instancia, NULL, &resultado)`.
   O retorno é `bool`. Se falhar, a instância não abriu.
4. **Rodar quadro a quadro**: chame `GodotInstance.iteration` uma vez por
   quadro (é o que faz a janela desenhar) até devolver `true`, que quer dizer
   "a engine pediu para fechar" (na prévia, só acontece com
   `--erethea-preview-capture`). Quem embute decide o ritmo: em um programa com
   interface, chame de um temporizador (60 vezes por segundo) na mesma thread
   que criou a instância.
5. **Fechar**: `libgodot_destroy_godot_instance(instancia)`.

Os *hashes* dos métodos de `GodotInstance` vêm do despejo da API
(`erethea --dump-extension-api`, campo `hash` de cada método da classe
`GodotInstance`); a engine recusa um método cujo hash mudou, em vez de chamar
com argumentos errados. Os valores da base 4.8 estão no topo do
`libgodot_preview.c` (`HASH_START` e `HASH_ITERATION`).

**Janela.** A prévia abre a própria janela da engine (a LibGodot cria a janela
como o executável cria). Não há, nesta versão, um jeito de desenhar *dentro* de
uma janela ou widget da Fornalha: a Fornalha pode mostrar a janela da prévia
ao lado da dela (a posição vai por `--position` e o tamanho por `--resolution`) ou
usar o executável.

### Limitações

- **Uma instância por processo.** A LibGodot só permite criar uma
  (`Only one Godot Instance may be created`); depois de destruída, não dá para
  criar outra no mesmo processo (comentário no código da Godot:
  `// Note: When Godot Engine supports reinitialization...`). Para trocar de
  asset, use o arquivo de controle; para recomeçar, feche o processo.
- **Sem ponteiro da cena por fora.** A API C da LibGodot só expõe
  `start`, `iteration`, `focus_in/out` e `pause/resume`. A cena se controla
  pelos argumentos e pelo arquivo de controle (ou por uma GDExtension do
  programa, que pode chamar `EretheaPreview` pelo `ClassDB`).
- **Não provado em placa de vídeo.** O CI não tem GPU: a captura roda no Linux
  com o Vulkan por CPU (lavapipe) e no Windows com o renderizador de
  compatibilidade em ANGLE (WARP). Ela prova que a prévia desenha o modelo no
  centro, não como fica nem a velocidade numa RX 580 ou numa RTX 4060 Ti.
- **Só `.glb` e `.gltf`**, como `GLTFDocument` lê. Modelos com extensões que a
  Godot não conhece aparecem sem elas (a Godot avisa no log).
- A sombra e a câmera se ajustam ao tamanho do modelo (do tamanho de um dedo ao
  de uma casa); modelos sem malha mostram só o chão e o céu.
- O pedido do Windows (`erethea.console.exe`, que a Fornalha já usa) não muda:
  `erethea.console.exe --erethea-preview=...` funciona.
- Fora do CI, o executável e a DLL da prévia ainda não entram no instalador nem
  no release: por ora saem só como artefatos do CI.

### Testar

```sh
# Executável (precisa de um renderizador que desenhe: janela, ou xvfb-run no Linux)
python erethea/tests/preview/test_preview.py bin/erethea.linuxbsd.editor.x86_64 --out preview
# LibGodot (compile antes com library_type=shared_library)
python erethea/tests/libgodot/test_libgodot.py bin/liberethea.linuxbsd.editor.x86_64.so --out libgodot
```

O primeiro cria arquivos glTF pequenos (caixas), mostra, troca, regrava e
quebra o arquivo de propósito; o segundo compila o `libgodot_preview.c` e
confere a captura. Os dois conferem que o centro da imagem é a cor da caixa e
o canto é o céu. No CI: `.github/workflows/erethea_preview.yml`. Os testes de
unidade (`bin/erethea... --headless --test --test-case="*Erethea*Preview*"`)
cobrem os argumentos e o enquadramento da câmera.

## Janela moderna e barra de título (Windows)

No Windows, o editor e o Gerenciador de Projetos não usam a barra de título do
sistema: a janela continua com as bordas de redimensionar, a sombra, os cantos
arredondados do Windows 11 e o encaixe nas bordas da tela (Aero Snap), mas o
conteúdo sobe até o topo e os botões de minimizar, maximizar e fechar são
desenhados pelo próprio editor. O código está em
`platform/windows/display_server_windows_title_bar.cpp`, e o teste
(`erethea/tests/window/title_bar_test.ps1`, no CI `erethea_window.yml`) abre a
janela numa área de trabalho do Windows e confere cada comportamento.

### Janela moderna nos jogos

Os jogos rodando ou exportados no Windows podem usar a mesma janela com a
configuração de projeto `display/window/erethea/modern_title_bar` (em
**Configurações do Projeto > Display > Window > Erethea**, com as configurações
avançadas ligadas). Ela vem **desligada**: desligada, o jogo se comporta
exatamente como na Godot, com a barra do sistema. Em outros sistemas a opção
não faz nada. No editor ela também não muda nada: o editor sempre usa a barra
própria.

```ini
[display]

window/erethea/modern_title_bar=true
```

Ligada, o jogo **perde a barra do sistema, inclusive os botões**. Quem liga
tem que desenhar a própria barra de título no jogo:

- `DisplayServer.window_start_drag()` move a janela a partir dela (chame ao
  apertar o botão esquerdo do mouse na barra; o encaixe nas bordas vale);
- `DisplayServer.window_set_mode()` minimiza e maximiza, e `get_tree().quit()`
  fecha;
- `DisplayServer.window_set_color()` pinta a moldura da janela com a cor da
  barra (no Windows 11, a borda fica um tom mais clara);
- a borda de cima continua redimensionando a janela; deixe uns pixels de
  folga no topo da barra, e desligue `display/window/size/resizable` se o jogo
  não pode mudar de tamanho.

O jogo de exemplo está em `erethea/tests/window/game_project`. Esta opção é
independente de `display/window/size/extend_to_title`, que continua só do macOS.
O mesmo teste de CI roda esse jogo com a opção ligada (sem barra do sistema,
bordas de redimensionar, maximizar cobrindo a área de trabalho e o botão de
fechar do próprio jogo) e desligada (com a barra do sistema).

## Som

Os sons que a Fornalha gera (efeitos, música por situação e dublagem com a
curva da boca) entram no jogo pela Erethea. Por enquanto:

**WAV gravado como stream.** Um WAV escrito aos poucos, como a fala que o Ignis
devolve, não sabe o tamanho quando começa e deixa 0 ou 0xFFFFFFFF nos tamanhos
do cabeçalho. A Godot lia esses arquivos vazios, ou relia o som como se fosse
cabeçalho, com milhares de erros (e a importação travava em arquivos grandes).
A Erethea entende que o som vai até o fim do arquivo. A mudança está em
`scene/resources/audio/audio_stream_wav.cpp`, marcada com `// Erethea:`, e o
teste em `tests/erethea/test_erethea_wav_stream.cpp`
(`--test --test-case="*Erethea*AudioStreamWAV*"`).

**Eco de cada área.** Ao montar as cenas pelo `game.project.json`, cada área
ganha um `Area3D` chamado `Eco`, que manda os sons feitos dentro dela para um
barramento de áudio com reverberação. O alcance é metade do caminho até a área
mais perto (de 12 a 80 m). Os 15 tipos de área da Fornalha viram seis
acústicas:

| Acústica (barramento) | Tipos de área | Como soa |
| --- | --- | --- |
| `caverna` (`Eco_caverna`) | caverna, masmorra | pedra fechada: eco longo e claro |
| `salao` (`Eco_salao`) | castelo | salão grande |
| `ruinas` (`Eco_ruinas`) | ruínas, arena, coliseu | paredes de pedra a céu aberto |
| `vila` (`Eco_vila`) | vila, cidade, aldeia | casas perto: curto e suave |
| `floresta` (`Eco_floresta`) | floresta, bosque, pântano | as folhas abafam os agudos |
| `montanha` (`Eco_montanha`) | montanha, colina, vulcão | campo aberto: eco distante e atrasado |

Uma área pode escolher a sua com o campo opcional `"acustica"` no
`game.project.json` (por exemplo `"acustica": "caverna"` num castelo com
masmorra). Os barramentos que faltam entram no layout de barramentos do projeto
(`default_bus_layout.tres`, o da configuração `audio/buses/default_bus_layout`),
sem mexer nos que já existem, e o relatório do `build_fornalha_scenes` diz quais
entraram (`audio_buses`). Desligue em `editor/erethea_fornalha/area_acoustics`.
Os testes `--test --test-case="*EretheaFornalha*"` e
`erethea/tests/fornalha_import/test_fornalha_import.py` conferem a escolha, o
alcance, as cenas e o layout.

**Música que muda com o jogo.** A Fornalha grava a música de cada situação em
`Audio/musica/<situacao>.wav` (mono, 16 bits, emenda o fim no começo) e, ao
lado, `<situacao>.musica.json` com `bpm` e `compassos` (compasso de 4 tempos).
Ao montar as cenas, a Erethea:

1. acerta as opções de importação de cada WAV descrito no `.musica.json`, no
   `<situacao>.wav.import`: laço para a frente sobre o arquivo todo
   (`edit/loop_mode=2`, `edit/loop_begin=0`, `edit/loop_end=-1`), `bpm`,
   `beat_count` (compassos × 4) e `bar_beats=4`, e pede a reimportação ao
   editor. Só mexe no `.import` se algo mudou. Se o arquivo ainda não foi
   importado (não tem `.import`), não cria nada: o relatório avisa para esperar
   a importação e montar de novo;
2. põe na `principal.tscn` um nó `Musica` (`AudioStreamPlayer`, -8 dB, como o
   `Sons`) no grupo `erethea_musica`, com um `AudioStreamInteractive` que tem um
   clipe por música (o nome do clipe é a situação) e a transição de qualquer
   clipe para qualquer outro no fim do compasso, com dois tempos de fade. O
   clipe inicial é a música da cena principal (`musica`), se houver. Sem música
   em `Audio/musica`, o nó não é criado;
3. o `Sons.tocar_musica` da Fornalha chama `tocar(situacao)` nesse grupo em vez
   de tocar o arquivo: o nó começa pela situação, ou troca de clipe esperando o
   fim do compasso (situação desconhecida ou a que já toca não faz nada).

O relatório do `build_fornalha_scenes` lista os clipes (`music_clips`) e os
WAV que mudaram de opções (`music_imports`). Para o `AudioStreamInteractive`
trocar no compasso, o WAV precisa dizer o andamento. A Godot só sabia isso do
Ogg Vorbis e do MP3, então o `AudioStreamWAV` ganhou as propriedades `bpm`,
`beat_count` e `bar_beats` (iguais às do Ogg) e o importador de WAV, as opções
de mesmo nome (`scene/resources/audio/audio_stream_wav.cpp`, marcadas com
`// Erethea:`; teste em `tests/erethea/test_erethea_wav_bpm.cpp`). Isso é uma
correção genérica, a propor à Godot.

A cena continua abrindo na Godot comum: o script do nó é GDScript embutido e o
`AudioStreamInteractive` é da Godot. Lá o WAV não diz o bpm (a opção é ignorada),
então a música troca na hora, com o fade, sem esperar o compasso. Desligue em
`editor/erethea_fornalha/interactive_music`: nada é mexido (nem o `.import`, nem
a `principal.tscn`) e o `Sons` toca o arquivo em laço como antes. O som em si
não dá para ouvir no CI: os testes conferem as opções, as cenas e a troca de
clipe no compasso (`tests/erethea/test_erethea_wav_bpm.cpp`,
`--test --test-case="*EretheaFornalha*"` e
`erethea/tests/fornalha_import/test_fornalha_import.py`).

## Lançar uma versão

As versões da Erethea têm numeração própria (`0.2.0`, `0.2.0-beta.1`),
separada da versão da Godot em que se baseiam. Para lançar, crie a tag no
`main` e envie:

```sh
git tag v0.2.0 && git push origin v0.2.0
```

O workflow `🔥 Erethea Release` compila o editor do Windows com
`BUILD_NAME=erethea-0.2.0`, confere o projeto de teste, gera o instalador e
publica o release no GitHub com o arquivo `erethea-setup-x86_64.exe`. Uma
versão com traço (`-beta.1`) sai como prerelease. Rodar o workflow à mão
(`workflow_dispatch`) só compila e confere, sem publicar.

## Atualização automática

O gerenciador de projetos procura versões novas nos releases deste repositório
(`editor/project_manager/erethea_update.h`), quando o editor está no modo
online, como na Godot. Ao clicar no aviso "Update available", a Erethea
instalada pelo instalador baixa o instalador novo, confere o SHA-256 que o
GitHub informa, instala por cima sem perguntas e abre de novo. Builds de
desenvolvimento (sem `BUILD_NAME=erethea-<versão>`) não procuram atualizações.
O instalador deixa o que fez em `%TEMP%\erethea-update.log`, para descobrir por
que uma atualização falhou.

Quem quer testar as versões de teste (como `0.3.0-beta.1`) mesmo estando numa
versão estável liga, nas Configurações do Editor, a opção
`erethea/update/prereleases`. Ela vem desligada: sem ela, os modos "Check
Newest Stable" e "Check Newest Patch" (e o "Auto" numa versão estável) ignoram
as versões de teste, como sempre. Ligada, elas também são oferecidas.

## Assinatura do instalador

Por enquanto o instalador sai sem assinatura digital. Ele funciona do mesmo
jeito, mas quem baixa pelo navegador vê o aviso do Windows "O Windows protegeu
o computador" e precisa clicar em "Mais informações" e "Executar assim mesmo".
A atualização automática não mostra esse aviso: o editor baixa o instalador
sozinho e confere o SHA-256.

O workflow de release já tem os passos de assinatura com o
[Azure Artifact Signing](https://learn.microsoft.com/azure/artifact-signing/quickstart)
(antigo Trusted Signing), desligados. Com eles ligados, o release assina os
dois executáveis do editor antes de montar o instalador, depois assina o
próprio instalador, com carimbo de tempo, e confere as assinaturas antes de
publicar. O desinstalador fica sem assinatura; como ele não é baixado da
internet, o Windows não mostra o aviso para ele.

Antes de pagar, confira se a conta é aceita: hoje a Microsoft só valida
pessoa física nos Estados Unidos e no Canadá, e empresa numa lista de países
que não inclui o Brasil. Se não for aceita, só os dois passos `Sign` do
workflow mudam para outro serviço de assinatura.

Para ligar:

1. Na Azure, crie uma conta do Artifact Signing, faça a validação de
   identidade e crie um perfil de certificado "Public Trust". Anote o endpoint
   da região (por exemplo `https://brs.codesigning.azure.net/`), o nome da
   conta e o nome do perfil.
2. No Microsoft Entra ID, registre um aplicativo, crie um segredo para ele e
   dê a ele o papel "Artifact Signing Certificate Profile Signer" no perfil de
   certificado.
3. No GitHub, em *Settings › Secrets and variables › Actions*, crie:
   - em *Secrets*: `AZURE_TENANT_ID`, `AZURE_CLIENT_ID` e
     `AZURE_CLIENT_SECRET`, do aplicativo;
   - em *Variables*: `ARTIFACT_SIGNING_ENDPOINT`, `ARTIFACT_SIGNING_ACCOUNT`
     e `ARTIFACT_SIGNING_PROFILE`.
4. Rode o workflow `🔥 Erethea Release` à mão uma vez (não publica nada) e
   confira o passo "Check the signatures".

Os passos só rodam com a variável `ARTIFACT_SIGNING_ENDPOINT` preenchida. Para
desligar de novo, apague essa variável.

## O que a marca troca e o que não troca

Troca: nome da engine, nome curto (pasta de dados `%APPDATA%\Erethea`, nome do
executável `erethea.*`), site, ícone, splash, logos do editor e alguns textos
do editor que falam do próprio programa.

Não troca, para continuar abrindo projetos da Godot: `project.godot`, `res://`,
`user://`, GDScript, o namespace `Godot` do C#, a interface GDExtension, nomes
de classes e formatos de arquivo. Textos sobre versões, projetos e comunidade
da Godot também ficam como estão.

## Logo

O logo é um emblema medieval: um disco escuro com dois anéis finos, uma fileira
de pontos e uma cruz, e no meio um "C" em meia-lua atravessado por uma espada,
que forma o "E". Embaixo vai o letreiro ERETHEA / ENGINE. O desenho original
fica em `branding/source/`: `emblem.png` (só o emblema), `logo.png` (logo
completo, para fundo claro) e `logo_dark_background.png` (as cores trocadas,
para fundo escuro).

Onde ele aparece:

| Lugar | Arquivo |
| --- | --- |
| Ícone do executável e do instalador (16 a 256 px) | `branding/erethea.ico`, `branding/erethea_console.ico` |
| Ícone do app e de projetos novos | `branding/icon.svg`, `branding/app_icon.png` |
| Splash ao abrir a engine | `branding/splash.png` (logo para fundo escuro) |
| Tela "Sobre" e créditos | `branding/logo.svg` (emblema ao lado do letreiro) |
| Barra de título do Gerenciador de Projetos | `branding/title_bar_logo.svg` |
| Instalador (canto das páginas e última página) | `branding/installer_*.bmp` |
| `README.md` | `branding/logo_vertical.svg` e `branding/logo_vertical_dark.svg` |

Nos ícones pequenos (até 64 px) os anéis e os pontos virariam borrão: no lugar
deles vai um anel só, mais grosso, e o "E" fica um pouco maior.

Para mudar o logo, troque os PNGs de `branding/source/` (mesmo tamanho e
posição) e rode:

```sh
sudo apt install potrace          # ou: brew install potrace / choco install potrace
pip install pillow numpy
npm install playwright && npx playwright install chromium
python erethea/tools/make_branding.py
prek run svgo --files erethea/branding/*.svg
python erethea/tools/rebrand.py
```
