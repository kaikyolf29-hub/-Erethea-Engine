# Pasta `erethea/`

Tudo o que é próprio da Erethea Engine e não existe na Godot fica aqui, para
que juntar versões novas da Godot não dê conflito.

| Pasta | O que tem |
| --- | --- |
| `branding/` | Logo, ícones (`.svg`, `.png`, `.ico`) e splash da Erethea |
| `tools/rebrand.py` | Aplica a marca Erethea por cima do código da Godot |
| `tools/make_branding.py` | Gera os arquivos de `branding/` a partir do desenho do logo |
| `installer/erethea.iss` | Instalador do Windows (Inno Setup 6), também usado nas atualizações |
| `tests/smoke_project/` | Projeto Godot simples que o CI abre com o instalador |
| `tests/reference_scene/` | Cena de referência (vila, floresta, personagem, dia e noite) para comparar com a Godot |

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

Os dois só funcionam no Forward+; nos outros renderizadores não fazem nada.
Os efeitos entram antes do TAA, do upscaling e do tone mapping. Os detalhes
estão na referência das classes, dentro do editor.

Para ver na cena de referência, importe o projeto uma vez e acrescente
`--post=all` (ou uma lista com `lut`, `flare`, `vignette`, `aberration`,
`grain` e `motion`) depois do `--`. Com `motion`, a câmera gira e o personagem
vira de um lado para o outro, e os dois estão no lugar de sempre, em
velocidade máxima, no quadro capturado:

```sh
erethea --headless --import --path erethea/tests/reference_scene
erethea --path erethea/tests/reference_scene --fixed-fps 30 -- --post=all --capture="$PWD/capturas/pos"
```

O CI captura a cena com todos os efeitos e mostra a diferença para o padrão.
Ainda não medido nas placas de referência: o custo de cada efeito em FPS.

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

O logo é um "E" moderno com cortes inclinados dentro de um anel, em preto e
branco, com o letreiro ERETHEA / ENGINE em Inter (licença SIL OFL, já incluída
em `thirdparty/fonts/`). Para mudar, edite `tools/make_branding.py` e rode:

```sh
pip install fonttools brotli pillow
npm install playwright && npx playwright install chromium
python erethea/tools/make_branding.py
python erethea/tools/rebrand.py
```
