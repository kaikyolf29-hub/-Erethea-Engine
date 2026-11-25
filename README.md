# Erethea Engine

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="erethea/branding/logo_vertical_dark.svg">
    <img src="erethea/branding/logo_vertical.svg" width="260" alt="Erethea Engine">
  </picture>
</p>

**Erethea Engine** é uma engine de jogos 2D e 3D baseada na
[Godot Engine](https://godotengine.org), com nome próprio, janela moderna no
Windows e foco em gráficos 3D bonitos. Ela abre projetos da Godot
(`project.godot`, GDScript e C#) sem conversão, para quem vem da Godot migrar
em minutos.

*Erethea Engine is a 2D and 3D game engine based on Godot Engine. It opens
Godot projects as they are. It is not affiliated with the Godot Foundation.*

## Baixar

O instalador do Windows (`erethea-setup-x86_64.exe`) está nos
[releases](https://github.com/kaikyolf29-hub/-Erethea-Engine/releases). A
versão atual é a **1.0.0**, baseada na Godot 4.8 beta 4. O instalador ainda
não é assinado, então o Windows mostra o aviso do SmartScreen: clique em "Mais
informações" e depois em "Executar assim mesmo". Depois de instalada, a
Erethea avisa no gerenciador de projetos quando sai uma versão nova e se
atualiza com um clique.

## Situação

As cinco fases estão no `main` e saíram na 1.0.0, provadas no CI (Windows e
Linux, com o Vulkan do lavapipe e o Direct3D 12 do WARP). A prova nas placas
de vídeo de referência (RX 580 e RTX 4060 Ti ou 5060 Ti) e com a Fornalha 3D
de verdade vem do teste no PC do criador. A assinatura do instalador fica para uma versão seguinte.

| Fase | O que entrega | Pronto quando |
| --- | --- | --- |
| 0 · Fork e marca | Nome, logo, script de renomear, CI, instalador | O instalador abre um projeto da Godot sem erro |
| 1 · Janela nativa | Barra de título integrada no Windows 11, tema escuro nas bordas, executável assinado, atualização automática | O editor não tem mais a barra do sistema e encaixa nas bordas do Windows 11 |
| 2 · Visual rápido | Presets de qualidade, pós-processamento, FSR 3.1 e 4, DLSS, XeSS | A cena de referência fica visivelmente melhor que na Godot, no mesmo FPS |
| 3 · Ponte com a Fábrica | Painel de IA, servidor MCP, live link, importador do `game.project.json` | Um personagem vindo da Fábrica aparece na cena sozinho e anda |
| 4 · Visual avançado | Ray tracing opcional, terreno, vegetação, materiais em camadas | A cena roda com ray tracing numa RTX e sem ele numa placa sem suporte |
| 5 · Geometria densa | Meshlets e streaming de geometria | Um modelo de milhões de polígonos roda sem LOD feito à mão |

## Compilar

Os passos são os mesmos da Godot
([documentação](https://docs.godotengine.org/en/latest/engine_details/development/compiling/index.html)).
O executável sai em `bin/` com o nome `erethea`:

```sh
# Windows (Visual Studio 2022 e Python 3 com SCons)
scons platform=windows target=editor

# Linux
scons platform=linuxbsd target=editor
```

O instalador do Windows é feito com [Inno Setup](https://jrsoftware.org/isinfo.php)
a partir de `erethea/installer/erethea.iss`. O CI do GitHub já gera o
instalador e testa se ele abre um projeto da Godot.

## Como o fork se mantém perto da Godot

- O branch `upstream` espelha a Godot; o `main` é a Erethea. A cada versão nova
  da Godot, `upstream` é atualizado e juntado ao `main`.
- A troca de nome, ícones e textos é feita pelo script
  `erethea/tools/rebrand.py`, que roda de novo depois de cada atualização.
- O código próprio da Erethea fica em `erethea/`, em módulos (`modules/`) e em
  GDExtension, e o que precisa mexer no núcleo entra em commits pequenos.

Veja [erethea/README.md](erethea/README.md) para os detalhes.

## Licença

A Erethea Engine é software livre sob a [licença MIT](LICENSE.txt), a mesma da
Godot. O aviso de copyright da Godot e as licenças dos componentes de terceiros
([COPYRIGHT.txt](COPYRIGHT.txt)) continuam valendo e aparecem na tela "Sobre"
do editor.

"Godot" e o logo da Godot são marcas da Godot Foundation. A Erethea não usa o
nome nem o logo da Godot como seus; ela é *baseada na Godot Engine*, e agradece
a todos os [contribuidores da Godot](AUTHORS.md).
