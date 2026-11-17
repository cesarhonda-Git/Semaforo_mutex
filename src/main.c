#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

// --- LEDs internos via DeviceTree ---
#define LED_A_NODE DT_ALIAS(led0)  // LED verde onboard
#define LED_C_NODE DT_ALIAS(led2)  // LED vermelho onboard

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledC = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);

// --- LEDs externos direto pelos controladores GPIO ---
static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));

#define PIN_LED_D 4   // PTD4 (Pino D2 Arduino) - Verde Pedestre
#define PIN_LED_E 12  // PTA12 (Pino D3 Arduino) - Vermelho Pedestre

// --- Prioridades e tempos ---
#define PRIO_THREAD_A 7 //verde carro
#define PRIO_THREAD_B 7 //amarelo carro
#define PRIO_THREAD_C 7 //vermelho carro
#define PRIO_THREAD_D 7 //verde pedestre
#define PRIO_THREAD_E 7 //vermelho pedestre
#define TEMPO_A_MS    3000
#define TEMPO_B_MS    1000
#define TEMPO_C_MS    4000
#define TEMPO_D_MS    4000
#define TEMPO_E_MS    4000

volatile int vez = 0; // comeca no verde
volatile int vezped = 1; // comeca no vermelho

void thread_A(void *p1, void *p2, void *p3)
{
    while (1) {
        if (vez != 0) {
            k_msleep(10);
            continue;
        }
        gpio_pin_set_dt(&ledA, 1);
        k_msleep(TEMPO_A_MS);
        gpio_pin_set_dt(&ledA, 0);
        vez = 1;
    }
}

void thread_B(void *p1, void *p2, void *p3)
{
    while (1) {
        if (vez != 1) {
            k_msleep(10);
            continue;
        }
        gpio_pin_set_dt(&ledC, 1);
        gpio_pin_set_dt(&ledA, 1);
        k_msleep(TEMPO_B_MS);
        gpio_pin_set_dt(&ledC, 0);
        gpio_pin_set_dt(&ledA, 0);
        vez = 2;
    }
}

void thread_C(void *p1, void *p2, void *p3)
{
    while (1) {
        if (vez != 2) {
            k_msleep(10);
            continue;
        }
        gpio_pin_set_dt(&ledC, 1);
        k_msleep(TEMPO_C_MS);
        gpio_pin_set_dt(&ledC, 0);
        vez = 0;
    }
}

void thread_D(void *p1, void *p2, void *p3)
{
    while (1) {
        if (vezped != 0) {
            k_msleep(10);
            continue;
        }
        gpio_pin_set(port_d, PIN_LED_D, 1); // Liga LED D (PTD4) (verde)
        k_msleep(TEMPO_D_MS);
        gpio_pin_set(port_d, PIN_LED_D, 0); // Desliga LED D
        vezped = 1;
    }
}

void thread_E(void *p1, void *p2, void *p3)
{
    while (1) {
        if (vezped != 1) {
            k_msleep(10);
            continue;
        }
        gpio_pin_set(port_a, PIN_LED_E, 1); // Liga LED E (PTA12) (v)
        k_msleep(TEMPO_E_MS);
        gpio_pin_set(port_a, PIN_LED_E, 0); // Desliga LED E
        vezped = 0;
    }
}

K_THREAD_DEFINE(a_tid, 512, thread_A, NULL, NULL, NULL, PRIO_THREAD_A, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_B, NULL, NULL, NULL, PRIO_THREAD_B, 0, 0);
K_THREAD_DEFINE(c_tid, 512, thread_C, NULL, NULL, NULL, PRIO_THREAD_C, 0, 0);
K_THREAD_DEFINE(d_tid, 512, thread_D, NULL, NULL, NULL, PRIO_THREAD_D, 0, 0);
K_THREAD_DEFINE(e_tid, 512, thread_E, NULL, NULL, NULL, PRIO_THREAD_E, 0, 0);

int main(void)
{
    if (!gpio_is_ready_dt(&ledA) || !gpio_is_ready_dt(&ledC) ||
        !device_is_ready(port_d) || !device_is_ready(port_a)) {
        return -1;
    }

    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledC, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_d, PIN_LED_D, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_a, PIN_LED_E, GPIO_OUTPUT_INACTIVE);

    printk("=== Demonstração de Preempção com LEDs ===\n");

    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}