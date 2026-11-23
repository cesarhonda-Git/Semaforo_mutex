# Atividade Semáforo de pedestres e veículos
**Erick Amorim Mafra Bomfim** — `17887849`
**César Akira Abe Honda** — `17884099`
---

## 1.1 Testes Unitários (Semáforos Isolados, Threads e Mutexes)
*Foco: Validar individualmente o semáforo de pedestres (`mutex_pedestre`) e o de veículos (`mutex_carro`), garantindo exclusão mútua entre LEDs e precisão dos temporizadores.*

| TESTES Unitários | Caso de Teste | Pré-condições | Procedimento de Execução | Resultado Esperado | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **1** | **Ciclo e Temporização — Pedestre** | `MODO_NORMAL` compilado; `PLACA1` energizada; cronômetro pronto. | Observar `thread_VERDE_PEDESTRE` (`PTD4`) e `thread_VERMELHO_PEDESTRE` (`PTA12`) por 3 ciclos completos. | LED Verde acende por **4,0 s** e apaga; em seguida LED Vermelho acende por **4,0 s**. | [ ✓ ] |
| **2** | **Exclusão Mútua — Pedestre** | Mesma do teste 1. | Inspecionar fisicamente e via log serial os estados de `PTD4` e `PTA12` durante as transições de `vezped`. | `PTD4` e `PTA12` **nunca** permanecem acesos simultaneamente (`mutex_pedestre` impede sobreposição). | [ ✓ ] |
| **3** | **Ciclo e Temporização — Veículos** | `MODO_NORMAL` compilado; `PLACA1` energizada. | Medir o tempo de cada estado do LED RGB onboard controlado por `thread_VERDE_CARRO`, `thread_AMARELO_CARRO` e `thread_VERMELHO_CARRO`. | Verde: **3,0 s**; Amarelo: **1,0 s**; Vermelho: **4,0 s** (ciclo total de **8,0 s**). | [ ✓ ] |
| **4** | **Exclusão Mútua — Veículos** | Mesma do teste 3. | Observar transições de `vez` (`0 -> 1 -> 2 -> 0`) sob proteção de `mutex_carro`. | Nenhuma thread sobrescreve o estado de outra; o LED Azul permanece sempre apagado e não há mistura indevida de cores. | [ ✓ ] |

---

## 1.2 Testes de Integração (Sincronismo entre Placas e Semáforos)
*Foco: Validar a comunicação física entre os dois microcontroladores (`PLACA1` e `PLACA2` via `PTA17`/`PTA16`) e a consistência cruzada entre fluxo de carros e travessia de pedestres.*

| TESTES de integração | Caso de Teste | Pré-condições | Procedimento de Execução | Resultado Esperado | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **1** | **Handshake Inicial entre Placas** | `PLACA1` e `PLACA2` interligadas (`GND`, `PTA17` e `PTA16` com resistores de 1kohm). | Energizar apenas `PLACA1` primeiro e observar; após 3 segundos, energizar `PLACA2`. | `PLACA1` aguarda com LEDs apagados até `PLACA2` responder `0` em `PTA16`; ambas iniciam os ciclos no mesmo instante. | [ ✓ ] |
| **2** | **Invariante de Segurança (Sem Colisão)** | Sistema sincronizado rodando em `MODO_NORMAL`. | Monitorar simultaneamente os LEDs das duas placas por 5 ciclos completos (40 segundos). | Enquanto `PLACA1` exibe Carro Verde/Amarelo, `PLACA2` exibe Carro Vermelho + Pedestre Verde (e vice-versa). Nunca há Verde para carros nas duas placas ao mesmo tempo. | [ ✓ ] |
| **3** | **Estabilidade de Concorrência (Sem Deadlock)** | Sistema rodando continuamente em `MODO_NORMAL`. | Manter ambas as placas operando ininterruptamente por 5 minutos, monitorando a saída serial (`printk`). | Nenhuma thread trava por contenção de `mutex_carro` ou `mutex_pedestre`; o ciclo de 8 s se mantém sem travamentos ou *stack overflow* (pilhas de 512 bytes). | [ ✓ ] |

---

## 1.3 Testes de Sistema (Modos de Operação e Requisitos Funcionais)
*Foco: Validar o comportamento ponta a ponta do produto nos três modos exigidos pela especificação.*

| TESTES | Caso de Teste | Pré-condições | Procedimento de Execução | Resultado Esperado | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **1** | **Modo 1 — Operação Normal (Dia)** | `MODO_ATUAL == MODO_NORMAL` gravado em ambas as placas. | Resetar ambas as placas e verificar a operação padrão completa. | Alternância contínua entre via 1 e via 2 respeitando todos os tempos (3s/1s/4s veículos; 4s/4s pedestres). | [ ✓ ] |
| **TS-02** | **Modo 2 — Modo Noturno** | `MODO_ATUAL == MODO_NOTURNO` gravado em ambas as placas. | Resetar as placas e medir o período de piscada dos LEDs de carros e pedestres. | Veículos piscam **Amarelo** (1 s aceso / 1 s apagado, período de 2 s); Pedestres piscam **Vermelho** (1 s aceso / 1 s apagado, período de 2 s). | [ ✓ ] |
| **TS-03** | **Modo 3 — Botão Acionado no Verde** | `MODO_ATUAL == MODO_BOTAO`; `PLACA2` fixa em Carro Vermelho/Pedestre Verde; botão em `PTA13`. | Pressionar o botão (`PTA13` ao `GND`) enquanto o semáforo de carros da `PLACA1` estiver **Verde** (`vez == 0`). | `PLACA1` interrompe o verde, passa para **Amarelo (1 s)**, fecha carros em Vermelho e abre Pedestre Verde por **6 s** (4 s normais + 2 s extras), retomando o ciclo normal em seguida. | [ ✓ ] |
| **TS-04** | **Modo 3 — Rejeição de Acionamento Indevido** | Mesma de TS-03. | Pressionar o botão `PTA13` quando o carro da `PLACA1` já estiver em Amarelo (`vez == 1`), Vermelho (`vez == 2`) ou durante uma travessia já em andamento (`acao_botao_rodando == true`). | O sistema ignora o toque repetido, evitando reinício abrupto do ciclo ou conflito de estado. | [ ✓ ] |
| **TS-05** | **Recuperação Após Retorno do Botão** | Execução concluída do teste TS-03. | Observar os 2 ciclos seguintes após o término dos 6 s de travessia do botão. | As threads `a_tid` a `e_tid` são retomadas (`k_thread_resume`) sem perda de sincronismo e `g_tid` volta ao estado suspenso. | [ ✓ ] |
