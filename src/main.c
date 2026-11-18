#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#define LEDR_NODE DT_ALIAS(led2)
#define LEDG_NODE DT_ALIAS(led0)
#define LEDB_NODE DT_ALIAS(led1)

static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));

#define PIN_LED_VERDEP    4   // PTD4  - Verde Pedestre
#define PIN_LED_VERMELHOP 12  // PTA12 - Vermelho Pedestre
#define PIN_BOTAO         13  // PTA13 - Botão de Pedestre (ligado ao GND)

#if DT_NODE_HAS_STATUS(LEDR_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDG_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDB_NODE, okay)

static const struct gpio_dt_spec ledr = GPIO_DT_SPEC_GET(LEDR_NODE, gpios);
static const struct gpio_dt_spec ledg = GPIO_DT_SPEC_GET(LEDG_NODE, gpios);
static const struct gpio_dt_spec ledb = GPIO_DT_SPEC_GET(LEDB_NODE, gpios);

#else
#error "LEDs não encontrados"
#endif

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

#define MODO_NORMAL  0
#define MODO_NOTURNO 1
#define MODO_BOTAO   2

#define MODO_ATUAL MODO_BOTAO

volatile int vez = 0;    // 0 -> verde, 1 -> amarelo, 2 -> vermelho
volatile int vezped = 1; // 0 -> verde, 1 -> vermelho

volatile bool acao_botao_rodando = false;
volatile bool ciclo_interrompido_carro = false;
volatile bool ciclo_interrompido_ped = false;

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

void thread_VERDE_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 0) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        set_rgb_veiculo(0, 1, 0); // Liga LED VERDE
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_VERDE_CARRO_MS);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        /* Se foi interrompida pelo botão durante o sleep, descarta o resto e reinicia */
        if (ciclo_interrompido_carro) {
            ciclo_interrompido_carro = false;
            k_mutex_unlock(&mutex_carro);
            continue;
        }
        set_rgb_veiculo(0, 0, 0);
        vez = 1;
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
        /* NÃO coloque '}' aqui antes do #else */
#else
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 1) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        set_rgb_veiculo(1, 1, 0); // Liga LED AMARELO
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_AMARELO_CARRO_MS);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        set_rgb_veiculo(0, 0, 0);
        vez = 2;
        k_mutex_unlock(&mutex_carro);
#endif
    } 
}

void thread_VERMELHO_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 2) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        set_rgb_veiculo(1, 0, 0); // Liga LED VERMELHO
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_VERMELHO_CARRO_MS);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        set_rgb_veiculo(0, 0, 0);
        vez = 0;
        k_mutex_unlock(&mutex_carro);
    }
}

void thread_VERDE_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        if (vezped != 0) {
            k_mutex_unlock(&mutex_pedestre);
            k_msleep(10);
            continue;
        }
        set_rg_pedestre(0, 1); // Liga LED VERDE pedestre
        k_mutex_unlock(&mutex_pedestre);

        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        set_rg_pedestre(0, 0);
        vezped = 1;
        k_mutex_unlock(&mutex_pedestre);
    }
}

void thread_VERMELHO_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        if (vezped != 1) {
            k_mutex_unlock(&mutex_pedestre);
            k_msleep(10);
            continue;
        }
        set_rg_pedestre(1, 0); // Liga LED VERMELHO pedestre
        k_mutex_unlock(&mutex_pedestre);

        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        /* Se foi interrompida pelo botão durante o sleep, descarta o resto e reinicia */
        if (ciclo_interrompido_ped) {
            ciclo_interrompido_ped = false;
            k_mutex_unlock(&mutex_pedestre);
            continue;
        }
        set_rg_pedestre(0, 0);
        vezped = 0;
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

#if MODO_ATUAL == MODO_NOTURNO
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO,    NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, 0);
K_THREAD_DEFINE(f_tid, 512, thread_NOTURNO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_NOTURNO_PEDESTRE, 0, 0);

#elif MODO_ATUAL == MODO_NORMAL
K_THREAD_DEFINE(a_tid, 512, thread_VERDE_CARRO,       NULL, NULL, NULL, PRIO_THREAD_VERDE_CARRO, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO,     NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, 0);
K_THREAD_DEFINE(c_tid, 512, thread_VERMELHO_CARRO,    NULL, NULL, NULL, PRIO_THREAD_VERMELHO_CARRO, 0, 0);
K_THREAD_DEFINE(d_tid, 512, thread_VERDE_PEDESTRE,    NULL, NULL, NULL, PRIO_THREAD_VERDE_PEDESTRE, 0, 0);
K_THREAD_DEFINE(e_tid, 512, thread_VERMELHO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_PEDESTRE, 0, 0);

#else // MODO_BOTAO
K_THREAD_DEFINE(a_tid, 512, thread_VERDE_CARRO,       NULL, NULL, NULL, PRIO_THREAD_VERDE_CARRO, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO,     NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, 0);
K_THREAD_DEFINE(c_tid, 512, thread_VERMELHO_CARRO,    NULL, NULL, NULL, PRIO_THREAD_VERMELHO_CARRO, 0, 0);
K_THREAD_DEFINE(d_tid, 512, thread_VERDE_PEDESTRE,    NULL, NULL, NULL, PRIO_THREAD_VERDE_PEDESTRE, 0, 0);
K_THREAD_DEFINE(e_tid, 512, thread_VERMELHO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_PEDESTRE, 0, 0);

void thread_BOTAO_PEDESTRE(void *p1, void *p2, void *p3)
{
    while (1) {
        /* Começa bloqueada e volta a se bloquear ao fim de cada acionamento */
        k_thread_suspend(k_current_get());

        /* 1. Passa o carro para AMARELO (pedestre continua vermelho) */
        k_mutex_lock(&mutex_carro, K_FOREVER);
        set_rgb_veiculo(1, 1, 0);
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

        /* 3. Encerra a prioridade do botão e devolve o controle ao ciclo normal */
        k_mutex_lock(&mutex_carro, K_FOREVER);
        k_mutex_lock(&mutex_pedestre, K_FOREVER);

        set_rgb_veiculo(0, 0, 0);
        set_rg_pedestre(0, 0);
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

K_THREAD_DEFINE(g_tid, 512, thread_BOTAO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_BOTAO, 0, 0);
#endif

int main(void)
{
    gpio_pin_configure_dt(&ledr, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledg, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure(port_d, PIN_LED_VERDEP, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_a, PIN_LED_VERMELHOP, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure(port_a, PIN_BOTAO, GPIO_INPUT | GPIO_PULL_UP);

#if defined(PLACA1)
    while (1) {
#if MODO_ATUAL == MODO_BOTAO
        /* Ativa apenas se o botão for pressionado (0) E o carro estiver verde (vez == 0) */
        if (gpio_pin_get(port_a, PIN_BOTAO) == 0 && vez == 0 && !acao_botao_rodando) {
            acao_botao_rodando = true;

            k_mutex_lock(&mutex_carro, K_FOREVER);
            k_mutex_lock(&mutex_pedestre, K_FOREVER);

            ciclo_interrompido_carro = true;
            ciclo_interrompido_ped = true;

            k_thread_suspend(a_tid);
            k_thread_suspend(b_tid);
            k_thread_suspend(c_tid);
            k_thread_suspend(d_tid);
            k_thread_suspend(e_tid);

            k_mutex_unlock(&mutex_pedestre);
            k_mutex_unlock(&mutex_carro);

            k_thread_resume(g_tid);
        }
#endif
        k_msleep(20);
    }

#elif defined(PLACA2)
    while (1) {
        k_msleep(20);
    }
#endif

    return 0;
}