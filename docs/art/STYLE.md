# Kobuleti (port do Batumi) — guia de estilo "Bone" (v2, minimalista)

Conceito: **instrumento de medição**. Cinza-gelo frio, tinta quase preta, uma placa escura para as 8 saídas e
**uma única cor de acento** (teal), usada só em LEDs de estado. Quatro colunas, quatro fases: tudo alinhado numa
grade de 4, sem ornamento.

Herda do módulo que inspirou o projeto o que importa: painel claro, knobs pretos, a faixa escura das saídas,
poucos rótulos. O desenho e a tipografia são próprios. O firmware (MIT, Matthias Puech) é de terceiros e está
creditado no README; a arte não copia o painel original.

Variantes: **Bone** (claro, principal) e **Slate** (escuro). Mesmo layout e mesmos tokens.

> **Paleta em revisão.** O teal sobre cinza-gelo ("Bone") não agradou. Há cinco alternativas lado a lado em
> `docs/art/palette_options.png` (A osso e ultramarino, B lilás e violeta, C areia e magenta, D sálvia e limão,
> E papel e preto). A tabela abaixo continua sendo a versão antiga até a escolha.

## 1. Paleta

> Paleta **adotada provisoriamente**: opção A ("osso e ultramarino") de `docs/art/palette_options.png`, no lugar do
> teal sobre cinza-gelo, que não agradou. Todas as cores do plugin estão em `kTheme()` e em `fms::colors()` em
> `src/Kobuleti.cpp`, então trocar é editar dois blocos.

| Token | Bone (claro) | Slate (escuro) | Uso |
|---|---|---|---|
| `panel` | `#E8E6E1` | `#17191B` | fundo do painel |
| `ink` | `#0F1216` | `#E6E8E8` | texto, contornos |
| `grey` | `#7C7F85` | `#7C7F85` | texto secundário, estados inativos |
| `group` | `#F3F1EC` | `#1E2124` | fundo dos grupos arredondados |
| `plate` | `#1B1E24` | `#0E1113` (contorno `#6B6F75`) | placa das 8 saídas e das pílulas |
| `accent` | `#2F4DFF` | `#5B74FF` | **o único acento**: LEDs, botão aceso, curva do logotipo |
| `negative` | `#F2C94C` | `#F2C94C` | metade negativa dos LEDs dos canais |

Regras: no máximo 3 cores visíveis (painel, tinta, acento) mais a placa escura. Sem degradê no painel.

## 2. Tipografia

Escolhidas por terem personalidade própria. Todas SIL OFL (podem ir em `res/fonts/`).

| Papel | Fonte | Peso | Regras |
|---|---|---|---|
| Wordmark | **Syne** | Bold (700) | minúsculas ("kobuleti"), tracking -1%, 10 pt; larga e de desenho incomum |
| Subtítulo | **Space Mono** | Regular | minúsculas, `grey`, 5,5 pt ("quadruple lfo") |
| Rótulos | **Space Mono** | Regular | MAIÚSCULAS, tracking +6%, 5 pt |
| Números dos canais | **Space Mono** | Bold | 1 a 4, 8 pt |

Arquivos: `Syne[wght].ttf` (variável, gerar instância 700), `SpaceMono-Regular.ttf`, `SpaceMono-Bold.ttf`.

## 3. Formas e grade

- **Grade:** quatro colunas; passo 11,6 mm (colunas em 7,6 / 19,2 / 30,8 / 42,4 mm), margem de 3 mm.
- **Seções:** só filete de 0,25 mm em `ink` entre blocos: seleção/modo, knobs, entradas, saídas, chaves.
- **Knobs:** pretos lisos, 10 mm, ponteiro branco de 0,4 mm; o número do canal (1–4) logo abaixo, em Space Mono.
- **Jacks:** furo preto, anel `ring`; os de entrada ficam sobre o painel, os de saída sobre a placa `plate`.
- **Placa das saídas:** retângulo de raio 1,4 mm, `plate`; linha de cima SINE, linha de baixo ASSIGN, com os
  rótulos centralizados entre as duas linhas, em `plate_ink` 5 pt.
- **Botão SELECT:** disco plano `panel` com anel `ink` de 0,3 mm; LEDs de modo em fila, pontos de 1,2 mm em `accent`.
- **LEDs dos canais:** pontos de 2 mm com halo de 20%.
- **Chaves:** as três CKSS, pretas, com os dois estados em minúsculas ao lado.

## Logo

O logo é um **logotipo com o nome** ("kobuleti"), não só um ícone. Referência de espírito: o título do Pamela's PRO
Workout, com um traço de personalidade no lettering. Aqui o desenho é próprio.

- **Composição:** o nome em minúsculas de **Syne Bold** e, logo abaixo, uma **trança** de quatro senoides defasadas de
  90 graus, da largura da palavra. São as quatro fases do LFO (modo quadratura). A fase 0 leva o acento.
- **Letras:** contornos de Syne convertidos em curvas. O plugin **não** carrega essa fonte; só o SVG.
- **Arquivos:** `docs/art/logo/logotype_mist.svg` e `logotype_slate.svg` (cores definidas pela paleta, hoje em revisão),
  mais `mark_*.svg` (só a trança em um ciclo).
- **Uso no painel:** 35 mm de largura (altura 15 mm), centralizado no topo.
  Tamanho mínimo: 26 mm de largura.

## Controles

Os controles do painel (folha em `docs/art/components.png`) estão em `src/Controls.hpp`, desenhados em NanoVG para mudar com o tema.

| Controle | Desenho |
|---|---|
| **Knob** (`fms::Knob`) | disco preto fosco, contorno ondulado quase invisível, **um risco branco** do centro à borda e um **anel de 15 traços grossos** em volta (a escala). Raios: 6,2 mm (grande), 4,6 mm (médio), 3,9 mm (compacto). |
| **Jack** (`fms::Jack`) | anéis concêntricos: borda cinza-clara, anel branco, anel escuro e furo preto. Raio 3,3 mm. |
| **Botão** (`fms::Button`) | disco chato com contorno grosso e brilho suave; aceso, preenche com o acento e ganha um halo. Pode ligar-se a uma luz do módulo. |
| **Chave** (`fms::Toggle`) | moldura redonda de anéis com alavanca em cápsula; valor 1 = alavanca para cima. |

As cores vêm de `fms::colors()`, definida em cada plugin e escolhida pelo tema ativo.

## 4. Tokens para o código (C++)

```cpp
// Mist
static const NVGcolor PANEL = nvgRGB(0xE1, 0xE3, 0xE4);
static const NVGcolor INK   = nvgRGB(0x14, 0x18, 0x1B);
static const NVGcolor GREY  = nvgRGB(0x7D, 0x85, 0x88);
static const NVGcolor PLATE = nvgRGB(0x1D, 0x23, 0x26);
static const NVGcolor PLATE_INK = nvgRGB(0xE6, 0xE8, 0xE8);
static const NVGcolor ACCENT = nvgRGB(0x17, 0xB3, 0xA6);
static const NVGcolor NEGATIVE = nvgRGB(0xF2, 0xC9, 0x4C);
// Slate: PANEL #17191B, INK #E6E8E8, PLATE #0E1113.
```

## 5. O que muda no plugin hoje

O painel atual já é claro, com knobs pretos e a faixa escura, então é um refino:
1. Fundo e filetes em tons frios (`#E1E3E4`, `#14181B`); sai o bege `#d9d8d2` e as faixas de topo e rodapé.
2. Nome em minúsculas ("kobuleti"), rótulos em maiúsculas pequenas e curtas.
3. LEDs de modo em `accent` (hoje amarelos) e LEDs dos canais com a dupla `accent`/`negative`.
4. Botão SELECT plano, sem o botão preto de sombra grande.
5. Tema Slate no menu de contexto.

## 6. Fazer e evitar

| Fazer | Evitar |
|---|---|
| Quatro colunas, tudo alinhado | Elementos fora da grade |
| Um acento só, em LEDs | Cor em rótulos ou faixas |
| Placa escura só para as saídas | Mais de uma placa ou moldura |
| Rótulos de uma palavra | Frases no painel |

## 7. Procedência

Concepts gerados para o projeto com o image_gen do Codex CLI em 25/10/2026 (prompts literais nos sidecars
`docs/art/source/style_v2/*.json`). São referência visual e não entram no plugin.
