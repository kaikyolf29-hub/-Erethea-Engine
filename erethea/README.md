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
