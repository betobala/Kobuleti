# KOBULETI — manual

**betobullet** · 10 HP · LFO quádruplo

O Kobuleti roda o firmware aberto do Xaoc Batumi (MIT, Matthias Puech) sem alterar o motor de LFO.
São quatro LFOs com quatro modos de relação entre eles, duas saídas por canal (senoide e uma forma
escolhível) e entradas de CV e de reset/sync por canal.

---

## 1. Painel

| Seção | O que é |
|---|---|
| **SELECT** + 4 LEDs | Escolhe o modo: FREE, QUAD, PHASE, DIV. |
| **Knobs 1–4** | Frequência de cada canal, ou o parâmetro do modo (abaixo). |
| **CV 1–4** | CV de frequência, em 1 V/oitava. |
| **RESET / SYNC 1–4** | Gatilhos de reset ou de sync, conforme a chave SYNC. |
| **LEDs dos canais** | Branco na metade positiva e cinza na negativa. |
| **SINE 1–4** | Saídas senoidais, ±5 V. |
| **ASSIGN 1–4** | Saídas com a forma escolhida pelas chaves WAVE. |
| **SYNC / RESET** | Define se as entradas reiniciam a fase (RESET) ou sincronizam o período (SYNC). |
| **WAVE 1 / WAVE 2** | As duas chaves combinam 4 formas para ASSIGN: trapézio, rampa, serra, triângulo. |

---

## 2. Modos

| Modo | Knob 1 | Knobs 2–4 |
|---|---|---|
| **FREE** | frequência do canal 1 | frequência dos canais 2–4, cada um independente |
| **QUAD** | frequência comum | nível de cada canal; as fases ficam a 0°, 90°, 180°, 270° |
| **PHASE** | frequência comum | defasagem de cada canal em relação ao 1 |
| **DIV** | frequência comum | divisão inteira da frequência para cada canal |

Nos modos QUAD, PHASE e DIV, as entradas mudam de função:
- **RESET 2** congela o LFO.
- **RESET 3** inverte a direção.
- **RESET 4** avança a forma de onda.

---

## 3. SELECT

- **Toque curto**: passa ao próximo modo (FREE → QUAD → PHASE → DIV) e zera o ajuste fino.
- **Segurar 0,5 s**: entra no **ajuste fino**. Os knobs passam a ajustar em passos finos, e o LED do
  modo pisca. Um toque curto sai. Se um knob mudou durante o ajuste fino, o LED pisca até o knob
  voltar à posição antiga, para evitar saltos.

---

## 4. Tensões

- **CV de entrada**: 1 V/oitava.
- **Saídas**: ±5 V.
- **Reset/sync**: dispara por volta de 3 V.
- **Taxa interna**: o firmware roda em 16384 Hz, a mesma do hardware. Por isso as frequências, as
  curvas e as divisões são as do Batumi.

## 5. Menu de contexto

- **Theme**: Black ou White.
