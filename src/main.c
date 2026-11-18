#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#define MODO_NORMAL  1
#define MODO_NOTURNO 2
#define MODO_BOTAO   3

#define MODO_ATUAL MODO_NORMAL

// Descomente a placa correspondente caso não esteja definindo via platformio.ini (-DPLACA1 / -DPLACA2)
// #define PLACA1
// #define PLACA2

#if !defined(PLACA1) && !defined(PLACA2)
#error "Defina PLACA1 ou PLACA2 antes de compilar!"
#endif

// --- LEDs internos via DeviceTree ---
#define LED_VERDE_NODE    DT_ALIAS(led0)  // LED verde onboard
#define LED_VERMELHO_NODE DT_ALIAS(led2)  // LED vermelho onboard

static const struct gpio_dt_spec ledVERDE = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
static const struct gpio_dt_spec ledVERMELHO = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);

// --- LEDs externos direto pelos controladores GPIO ---
static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));

#define PIN_LED_VERDE_PEDESTRE    4   // PTD4 (Pino D2 Arduino) - Verde Pedestre
#define PIN_LED_VERMELHO_PEDESTRE 12  // PTA12 (Pino D3 Arduino) - Vermelho Pedestre

// --- Pinos de Sincronização (Porta A) ---
#define PIN_SYNC_OUT 17  // PTA17 - Saída
#define PIN_SYNC_IN  16  // PTA16 - Entrada

// --- Prioridades e tempos ---
#define PRIO_THREAD_VERDE_CARRO       7
#define PRIO_THREAD_AMARELO_CARRO     7
#define PRIO_THREAD_VERMELHO_CARRO    7
#define PRIO_THREAD_VERDE_PEDESTRE    7
#define PRIO_THREAD_VERMELHO_PEDESTRE 7

#define TEMPO_VERDE_CARRO_MS       3000
#define TEMPO_AMARELO_CARRO_MS     1000
#define TEMPO_VERMELHO_CARRO_MS    4000
#define TEMPO_VERDE_PEDESTRE_MS    4000
#define TEMPO_VERMELHO_PEDESTRE_MS 4000

// --- Definição dos Mutexes ---
K_MUTEX_DEFINE(mutex_carro);
K_MUTEX_DEFINE(mutex_pedestre);

#if defined(PLACA1)
int vez = 0;    // 0 -> verde | 1 -> amarelo | 2 -> vermelho
int vezped = 1; // 0 -> verde | 1 -> vermelho
#elif defined(PLACA2)
int vez = 2;    // começa no vermelho
int vezped = 0; // começa no verde
#endif

#if MODO_ATUAL == MODO_NORMAL

void thread_VERDE_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 0) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        gpio_pin_set_dt(&ledVERDE, 1);
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_VERDE_CARRO_MS);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        gpio_pin_set_dt(&ledVERDE, 0);
        vez = 1;
        k_mutex_unlock(&mutex_carro);
    }
}

void thread_AMARELO_CARRO(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&mutex_carro, K_FOREVER);
        if (vez != 1) {
            k_mutex_unlock(&mutex_carro);
            k_msleep(10);
            continue;
        }
        gpio_pin_set_dt(&ledVERMELHO, 1);
        gpio_pin_set_dt(&ledVERDE, 1);
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_AMARELO_CARRO_MS);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        gpio_pin_set_dt(&ledVERMELHO, 0);
        gpio_pin_set_dt(&ledVERDE, 0);
        vez = 2;
        k_mutex_unlock(&mutex_carro);
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
        gpio_pin_set_dt(&ledVERMELHO, 1);
        k_mutex_unlock(&mutex_carro);

        k_msleep(TEMPO_VERMELHO_CARRO_MS);

        k_mutex_lock(&mutex_carro, K_FOREVER);
        gpio_pin_set_dt(&ledVERMELHO, 0);
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
        gpio_pin_set(port_d, PIN_LED_VERDE_PEDESTRE, 1);
        k_mutex_unlock(&mutex_pedestre);

        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        gpio_pin_set(port_d, PIN_LED_VERDE_PEDESTRE, 0);
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
        gpio_pin_set(port_a, PIN_LED_VERMELHO_PEDESTRE, 1);
        k_mutex_unlock(&mutex_pedestre);

        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);

        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        gpio_pin_set(port_a, PIN_LED_VERMELHO_PEDESTRE, 0);
        vezped = 0;
        k_mutex_unlock(&mutex_pedestre);
    }
}

// SYS_FOREVER_MS impede que as threads rodem antes de k_thread_start()
K_THREAD_DEFINE(a_tid, 512, thread_VERDE_CARRO, NULL, NULL, NULL, PRIO_THREAD_VERDE_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO, NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(c_tid, 512, thread_VERMELHO_CARRO, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_CARRO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(d_tid, 512, thread_VERDE_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERDE_PEDESTRE, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(e_tid, 512, thread_VERMELHO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_PEDESTRE, 0, SYS_FOREVER_MS);

int main(void)
{
    if (!gpio_is_ready_dt(&ledVERDE) || !gpio_is_ready_dt(&ledVERMELHO) ||
        !device_is_ready(port_d) || !device_is_ready(port_a)) {
        return -1;
    }

    gpio_pin_configure_dt(&ledVERDE, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledVERMELHO, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_d, PIN_LED_VERDE_PEDESTRE, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_a, PIN_LED_VERMELHO_PEDESTRE, GPIO_OUTPUT_INACTIVE);

    // Saída inicia em 1 (repouso) e entrada em PULL_UP (lê 1 enquanto não houver sinal 0)
    gpio_pin_configure(port_a, PIN_SYNC_OUT, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure(port_a, PIN_SYNC_IN, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_set(port_a, PIN_SYNC_OUT, 1);
    k_msleep(20);

    printk("=== Aguardando sincronizacao entre placas ===\n");

#if defined(PLACA1)
    gpio_pin_set(port_a, PIN_SYNC_OUT, 0); // Envia sinal (0) para PLACA2
    while (1) {
        if (gpio_pin_get(port_a, PIN_SYNC_IN) == 0) { // Aguarda resposta (0) da PLACA2
            break;
        }
        k_msleep(5);
    }
    k_msleep(20); // Garante que PLACA2 leu o sinal antes de voltar ao repouso
    gpio_pin_set(port_a, PIN_SYNC_OUT, 1);
#endif

#if defined(PLACA2)
    while (1) {
        if (gpio_pin_get(port_a, PIN_SYNC_IN) == 0) { // Aguarda sinal (0) da PLACA1
            break;
        }
        k_msleep(5);
    }
    gpio_pin_set(port_a, PIN_SYNC_OUT, 0); // Responde (0) para PLACA1
    k_msleep(20); // Garante que PLACA1 leu a resposta antes de voltar ao repouso
    gpio_pin_set(port_a, PIN_SYNC_OUT, 1);
#endif

    printk("=== Sincronizado! Iniciando semaforo ===\n");

    // Libera todas as threads somente agora
    k_thread_start(a_tid);
    k_thread_start(b_tid);
    k_thread_start(c_tid);
    k_thread_start(d_tid);
    k_thread_start(e_tid);

    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}

#endif

#if MODO_ATUAL == MODO_NOTURNO
#endif
#if MODO_ATUAL == MODO_BOTAO
#endif  