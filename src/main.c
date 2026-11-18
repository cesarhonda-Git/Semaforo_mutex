#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

// --- LEDs internos via DeviceTree ---
#define LED_VERDE_NODE DT_ALIAS(led0)  // LED verde onboard
#define LED_VERMELHO_NODE DT_ALIAS(led2)  // LED vermelho onboard

static const struct gpio_dt_spec ledVERDE = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
static const struct gpio_dt_spec ledVERMELHO = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);

// --- LEDs externos direto pelos controladores GPIO ---
static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));

#define PIN_LED_VERDE_PEDESTRE 4   // PTD4 (Pino D2 Arduino) - Verde Pedestre
#define PIN_LED_VERMELHO_PEDESTRE 12  // PTA12 (Pino D3 Arduino) - Vermelho Pedestre

// --- Prioridades e tempos ---
#define PRIO_THREAD_VERDE_CARRO 7 //verde carro
#define PRIO_THREAD_AMARELO_CARRO 7 //amarelo carro
#define PRIO_THREAD_VERMELHO_CARRO 7 //vermelho carro
#define PRIO_THREAD_VERDE_PEDESTRE 7 //verde pedestre
#define PRIO_THREAD_VERMELHO_PEDESTRE 7 //vermelho pedestre
#define TEMPO_VERDE_CARRO_MS    3000
#define TEMPO_AMARELO_CARRO_MS    1000
#define TEMPO_VERMELHO_CARRO_MS    4000
#define TEMPO_VERDE_PEDESTRE_MS    4000
#define TEMPO_VERMELHO_PEDESTRE_MS    4000

// --- Definição dos Mutexes ---
K_MUTEX_DEFINE(mutex_carro);
K_MUTEX_DEFINE(mutex_pedestre);

volatile int vez = 0; // comeca no verde        vez = 0 -> verde  vez = 1 -> amarelo vez = 2 -> vermelho
volatile int vezped = 1; // comeca no vermelho  vez = 0 -> verde  vez = 1 -> vermelho

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
    k_mutex_lock(&mutex_pedestre, K_FOREVER);
    while (1) {
        if (vezped != 0) {
            k_mutex_unlock(&mutex_pedestre);
            k_msleep(10);
            continue;
        }
        gpio_pin_set(port_d, PIN_LED_VERDE_PEDESTRE, 1); // Liga LED VERDE (PTD4) (verde)
        k_mutex_unlock(&mutex_pedestre);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        gpio_pin_set(port_d, PIN_LED_VERDE_PEDESTRE, 0); // Desliga LED VERDE (PTD4)
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
        gpio_pin_set(port_a, PIN_LED_VERMELHO_PEDESTRE, 1); // Liga LED VERMELHO (PTA12) (vermelho)
        k_mutex_unlock(&mutex_pedestre);
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        k_mutex_lock(&mutex_pedestre, K_FOREVER);
        gpio_pin_set(port_a, PIN_LED_VERMELHO_PEDESTRE, 0); // Desliga LED VERMELHO (PTA12)
        vezped = 0;
        k_mutex_unlock(&mutex_pedestre);
    }
}

K_THREAD_DEFINE(a_tid, 512, thread_VERDE_CARRO, NULL, NULL, NULL, PRIO_THREAD_VERDE_CARRO, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_AMARELO_CARRO, NULL, NULL, NULL, PRIO_THREAD_AMARELO_CARRO, 0, 0);
K_THREAD_DEFINE(c_tid, 512, thread_VERMELHO_CARRO, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_CARRO, 0, 0);
K_THREAD_DEFINE(d_tid, 512, thread_VERDE_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERDE_PEDESTRE, 0, 0);
K_THREAD_DEFINE(e_tid, 512, thread_VERMELHO_PEDESTRE, NULL, NULL, NULL, PRIO_THREAD_VERMELHO_PEDESTRE, 0, 0);

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

    printk("=== Demonstração de Preempção com LEDs ===\n");

    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}