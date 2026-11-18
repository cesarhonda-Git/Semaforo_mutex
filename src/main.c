#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#define MODO_NORMAL  0
#define MODO_NOTURNO 1
#define MODO_BOTAO   2

/* Escolha o modo aqui: MODO_NORMAL, MODO_NOTURNO ou MODO_BOTAO */
#define MODO_ATUAL MODO_BOTAO

// Descomente caso não esteja definindo via platformio.ini (-DPLACA1 / -DPLACA2)
// #define PLACA1
// #define PLACA2

#if !defined(PLACA1) && !defined(PLACA2)
#error "Defina PLACA1 ou PLACA2 antes de compilar!"
#endif

// --- LEDs internos via DeviceTree ---
#define LEDR_NODE DT_ALIAS(led2)
#define LEDG_NODE DT_ALIAS(led0)
#define LEDB_NODE DT_ALIAS(led1)

#if DT_NODE_HAS_STATUS(LEDR_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDG_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDB_NODE, okay)

static const struct gpio_dt_spec ledr = GPIO_DT_SPEC_GET(LEDR_NODE, gpios);
static const struct gpio_dt_spec ledg = GPIO_DT_SPEC_GET(LEDG_NODE, gpios);
static const struct gpio_dt_spec ledb = GPIO_DT_SPEC_GET(LEDB_NODE, gpios);

#else
#error "LEDs não encontrados"
#endif

// --- Portas GPIO ---
static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));

// --- Pinos Externos e Botão ---
#define PIN_LED_VERDEP    4   // PTD4  (J1-6) - Verde Pedestre
#define PIN_LED_VERMELHOP 12  // PTA12 (J1-8) - Vermelho Pedestre
#define PIN_BOTAO         13  // PTA13 (J2-2) - Botão de Pedestre (ligado ao GND)

// --- Pinos de Sincronização entre Placas (Porta A) ---
#define PIN_SYNC_OUT 17  // PTA17 (J2-11, fileira interna) - Saída
#define PIN_SYNC_IN  16  // PTA16 (J2-9,  fileira interna) - Entrada

// --- Prioridades e Tempos ---
#define PRIO_THREAD_VERDE_CARRO       7
#define PRIO_THREAD_AMARELO_CARRO     7
#define PRIO_THREAD_VERMELHO_CARRO    7
#define PRIO_THREAD_VERDE_PEDESTRE    7
#define PRIO_THREAD_VERMELHO_PEDESTRE 7
#define PRIO_THREAD_NOTURNO_PEDESTRE  7
#define PRIO_THREAD_BOTAO             6

#define TEMPO_VERDE_CARRO_MS       3000
#define TEMPO_AMARELO_CARRO_MS     1000
#define TEMPO_VERMELHO_CARRO_MS    4000
#define TEMPO_VERDE_PEDESTRE_MS    4000
#define TEMPO_VERMELHO_PEDESTRE_MS 4000
#define TEMPO_EXTRA_BOTAO_MS       2000 // +2 segundos no vermelho do carro / verde do pedestre

// --- Definição dos Mutexes ---
K_MUTEX_DEFINE(mutex_carro);
K_MUTEX_DEFINE(mutex_pedestre);

// --- Estado Inicial Oposto para Evitar Colisão entre as Vias ---
#if defined(PLACA1)
volatile int vez = 0;    // 0 -> verde | 1 -> amarelo | 2 -> vermelho
volatile int vezped = 1; // 0 -> verde | 1 -> vermelho
#elif defined(PLACA2)
volatile int vez = 2;    // começa no vermelho
volatile int vezped = 0; // começa no verde
#endif

volatile bool acao_botao_rodando = false;

static void set_rgb_veiculo(int r, int g, int b)
{
    gpio_pin_set_dt(&ledr, r);
    gpio_pin_set_dt(&ledg, g);
    gpio_pin_set_dt(&ledb, b);
}

static void set_rg_pedestre(int r, int g)
{
    gpio_pin_set(port_a, PIN_LED_VERMELHOP, r);
    gpio_pin_set(port_d, PIN_LED_VERDEP, g);
}

/* Espera fracionada: permite interromper imediatamente quando o botão é apertado */
static bool esperar_ou_interromper(int total_ms)
{
    int decorrido = 0;
    while (decorrido < total_ms) {
        if (acao_botao_rodando) {
            return true;
        }
        k_msleep(20);
        decorrido += 20;
    }
    return acao_botao_rodando;
}

/* ================= THREADS DOS SEMÁFOROS ================= */

void thread_VERDE_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 0 || acao_botao_rodando) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        set_rgb_veiculo(0, 1, 0); // Liga LED VERDE
        k_mutex_unlock(&mutex_carro);

        if (esperar_ou_interromper(TEMPO_VERDE_CARRO_MS)) {
            continue; // Volta ao topo limpo se o botão interrompeu
        }

        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (!acao_botao_rodando) {
            set_rgb_veiculo(0, 0, 0);
            vez = 1;
        }
        k_mutex_unlock(&mutex_carro);
    }
}

void thread_AMARELO_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
#if MODO_ATUAL == MODO_NOTURNO
        k_mutex_lock(&mutex_carro, K_FOREVER);
        set_rgb_veiculo(1, 1, 0);
        k_mutex_unlock(&mutex_carro);
        k_msleep(1000);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        set_rgb_veiculo(0, 0, 0);
        k_mutex_unlock(&mutex_carro);
        k_msleep(1000);
#else
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 1 || acao_botao_rodando) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        set_rgb_veiculo(1, 1, 0); // Liga LED AMARELO
        k_mutex_unlock(&mutex_carro);

        if (esperar_ou_interromper(TEMPO_AMARELO_CARRO_MS)) {
            continue;
        }

        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (!acao_botao_rodando) {
            set_rgb_veiculo(0, 0, 0);
            vez = 2;
        }
        k_mutex_unlock(&mutex_carro);
#endif
    }
}

void thread_VERMELHO_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 2 || acao_botao_rodando) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        set_rgb_veiculo(1, 0, 0); // Liga LED VERMELHO
        k_mutex_unlock(&mutex_carro);

        if (esperar_ou_interromper(TEMPO_VERMELHO_CARRO_MS)) {
            continue;
        }

        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (!acao_botao_rodando) {
            set_rgb_veiculo(0, 0, 0);
            vez = 0;
        }
        k_mutex_unlock(&mutex_carro);
    }
}

void thread_VERDE_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        if (vezped != 0 || acao_botao_rodando) {
            k_mutex_unlock(&mutex_pedestre);
            k_msleep(10);
            continue;
        }
        set_rg_pedestre(0, 1); // Liga LED VERDE pedestre
        k_mutex_unlock(&mutex_pedestre);

        if (esperar_ou_interromper(TEMPO_VERDE_PEDESTRE_MS)) {
            continue;
        }

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        if (!acao_botao_rodando) {
            set_rg_pedestre(0, 0);
            vezped = 1;
        }
        k_mutex_unlock(&mutex_pedestre);
    }
}

void thread_VERMELHO_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        if (vezped != 1 || acao_botao_rodando) {
            k_mutex_unlock(&mutex_pedestre);
            k_msleep(10);
            continue;
        }
        set_rg_pedestre(1, 0); // Liga LED VERMELHO pedestre
        k_mutex_unlock(&mutex_pedestre);

        if (esperar_ou_interromper(TEMPO_VERMELHO_PEDESTRE_MS)) {
            continue;
        }

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        if (!acao_botao_rodando) {
            set_rg_pedestre(0, 0);
            vezped = 0;
        }
        k_mutex_unlock(&mutex_pedestre);
    }
}

void thread_NOTURNO_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        set_rg_pedestre(1, 0);
        k_mutex_unlock(&mutex_pedestre);
        k_msleep(1000);

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        set_rg_pedestre(0, 0);
        k_mutex_unlock(&mutex_pedestre);
        k_msleep(1000);
    }
}

/* ================= DEFINIÇÃO DAS THREADS POR MODO ================= */

#if MODO_ATUAL == MODO_NOTURNO
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO,    NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(f_tid, 512, thread_NOTURNO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_NOTURNO_PEDESTRE, 0, SYS_FOREVER_MS);

#elif MODO_ATUAL == MODO_NORMAL
K_THREAD_DEFINE(a_tid, 512, thread_VERDE_CARRO,       NULL, NULL, NULL, PRIO_THREAD_VERDE_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO,     NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(c_tid, 512, thread_VERMELHO_CARRO,    NULL, NULL, NULL, PRIO_THREAD_VERMELHO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(d_tid, 512, thread_VERDE_PEDESTRE,    NULL, NULL, NULL, PRIO_THREAD_VERDE_PEDESTRE, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(e_tid, 512, thread_VERMELHO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_PEDESTRE, 0, SYS_FOREVER_MS);

#elif MODO_ATUAL == MODO_BOTAO
#if defined(PLACA1)
K_THREAD_DEFINE(a_tid, 512, thread_VERDE_CARRO,       NULL, NULL, NULL, PRIO_THREAD_VERDE_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO,     NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(c_tid, 512, thread_VERMELHO_CARRO,    NULL, NULL, NULL, PRIO_THREAD_VERMELHO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(d_tid, 512, thread_VERDE_PEDESTRE,    NULL, NULL, NULL, PRIO_THREAD_VERDE_PEDESTRE, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(e_tid, 512, thread_VERMELHO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_PEDESTRE, 0, SYS_FOREVER_MS);

void thread_BOTAO_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        /* Bloqueia a si mesma até ser chamada pelo botão */
        k_thread_suspend(k_current_get());

        /* 1. Passa o carro para AMARELO (pedestre permanece vermelho) */
        k_mutex_lock(&mutex_carro, K_FOREVER);
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        set_rgb_veiculo(1, 1, 0);
        set_rg_pedestre(1, 0);
        k_mutex_unlock(&mutex_pedestre);
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_AMARELO_CARRO_MS);

        /* 2. Passa o carro para VERMELHO e o pedestre para VERDE */
        k_mutex_lock(&mutex_carro, K_FOREVER);
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        set_rgb_veiculo(1, 0, 0);
        set_rg_pedestre(0, 1);
        k_mutex_unlock(&mutex_pedestre);
        k_mutex_unlock(&mutex_carro);

        /* Fica 2 segundos a mais que o tempo normal (4000ms + 2000ms = 6000ms) */
        k_msleep(TEMPO_VERMELHO_CARRO_MS + TEMPO_EXTRA_BOTAO_MS);

        /* 3. Encerra a ação do botão e devolve o controle ao ciclo normal */
        k_mutex_lock(&mutex_carro, K_FOREVER);
        k_mutex_lock(&mutex_pedestre, K_FOREVER);

        vez = 0;    // Volta no verde do carro
        vezped = 1; // Volta no vermelho do pedestre
        acao_botao_rodando = false;

        k_thread_resume(a_tid);
        k_thread_resume(b_tid);
        k_thread_resume(c_tid);
        k_thread_resume(d_tid);
        k_thread_resume(e_tid);

        k_mutex_unlock(&mutex_pedestre);
        k_mutex_unlock(&mutex_carro);
    }
}

K_THREAD_DEFINE(g_tid, 512, thread_BOTAO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_BOTAO, 0, SYS_FOREVER_MS);
#endif
#endif

/* ================= MAIN E SINCRONIZAÇÃO ================= */

int main(void)
{
    if (!gpio_is_ready_dt(&ledr) || !gpio_is_ready_dt(&ledg) || !gpio_is_ready_dt(&ledb) ||
        !device_is_ready(port_d) || !device_is_ready(port_a)) {
        return -1;
    }

    // Configura LEDs RGB onboard e externos
    gpio_pin_configure_dt(&ledr, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledg, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure(port_d, PIN_LED_VERDEP, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_a, PIN_LED_VERMELHOP, GPIO_OUTPUT_INACTIVE);

    // Configura botão de pedestre (PTA13 ligado ao GND)
    gpio_pin_configure(port_a, PIN_BOTAO, GPIO_INPUT | GPIO_PULL_UP);

    // Configura pinos de sincronização (repouso em 1)
    gpio_pin_configure(port_a, PIN_SYNC_OUT, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure(port_a, PIN_SYNC_IN, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_set(port_a, PIN_SYNC_OUT, 1);
    k_msleep(20);

    printk("=== Aguardando sincronizacao entre placas ===\n");

#if defined(PLACA1)
    gpio_pin_set(port_a, PIN_SYNC_OUT, 0); // 1. Envia 0 para PLACA2
    while (gpio_pin_get(port_a, PIN_SYNC_IN) != 0) { // 2. Aguarda resposta 0 da PLACA2
        k_msleep(5);
    }
    k_msleep(20);
    gpio_pin_set(port_a, PIN_SYNC_OUT, 1); // 3. Libera linha

#elif defined(PLACA2)
    while (gpio_pin_get(port_a, PIN_SYNC_IN) != 0) { // 1. Aguarda 0 da PLACA1
        k_msleep(5);
    }
    gpio_pin_set(port_a, PIN_SYNC_OUT, 0); // 2. Responde 0 e segura até PLACA1 ler
    while (gpio_pin_get(port_a, PIN_SYNC_IN) == 0) { // 3. Espera PLACA1 voltar para 1
        k_msleep(5);
    }
    gpio_pin_set(port_a, PIN_SYNC_OUT, 1); // 4. Libera linha
#endif

    printk("=== Sincronizado! Iniciando semaforo ===\n");

#if MODO_ATUAL == MODO_NOTURNO
    k_thread_start(b_tid);
    k_thread_start(f_tid);

#elif MODO_ATUAL == MODO_NORMAL
    k_thread_start(a_tid);
    k_thread_start(b_tid);
    k_thread_start(c_tid);
    k_thread_start(d_tid);
    k_thread_start(e_tid);

#elif MODO_ATUAL == MODO_BOTAO
#if defined(PLACA1)
    k_thread_start(a_tid);
    k_thread_start(b_tid);
    k_thread_start(c_tid);
    k_thread_start(d_tid);
    k_thread_start(e_tid);
    k_thread_start(g_tid); // Inicia e se suspende aguardando o botão

    while (1) {
        /* Dispara apenas quando o botão é apertado (0) E o carro da PLACA1 está verde (vez == 0) */
        if (gpio_pin_get(port_a, PIN_BOTAO) == 0 && vez == 0 && !acao_botao_rodando) {
            printk("Botao acionado! Iniciando travessia estendida...\n");
            acao_botao_rodando = true;

            /* Aguarda 25ms para que as threads saiam de esperar_ou_interromper() */
            k_msleep(25);

            k_mutex_lock(&mutex_carro, K_FOREVER);
            k_mutex_lock(&mutex_pedestre, K_FOREVER);

            k_thread_suspend(a_tid);
            k_thread_suspend(b_tid);
            k_thread_suspend(c_tid);
            k_thread_suspend(d_tid);
            k_thread_suspend(e_tid);

            k_mutex_unlock(&mutex_pedestre);
            k_mutex_unlock(&mutex_carro);

            k_thread_resume(g_tid);
        }
        k_msleep(20);
    }

#elif defined(PLACA2)
    /* PLACA2 permanece fixa em Carro Vermelho e Pedestre Verde no MODO_BOTAO */
    set_rgb_veiculo(1, 0, 0);
    set_rg_pedestre(0, 1);
    while (1) {
        k_sleep(K_FOREVER);
    }
#endif
#endif

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}