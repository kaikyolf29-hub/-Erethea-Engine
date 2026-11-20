# Erethea Engine

<p align="center">
  <img src="erethea/branding/logo.svg" width="420" alt="Erethea Engine">
</p>

**Erethea Engine** é uma engine de jogos 2D e 3D baseada na
[Godot Engine](https://godotengine.org), com nome próprio, janela moderna no
Windows e foco em gráficos 3D bonitos. Ela abre projetos da Godot
(`project.godot`, GDScript e C#) sem conversão, para quem vem da Godot migrar
em minutos.

*Erethea Engine is a 2D and 3D game engine based on Godot Engine. It opens
Godot projects as they are. It is not affiliated with the Godot Foundation.*

## Situação

O projeto está na **fase 0 (fork e marca)**: código da Godot 4.8 beta 4,
nome, logo, ícone, splash e instalador próprios, e CI de Windows e Linux.

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
