#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#define LEDR_NODE DT_ALIAS(led2)
#define LEDG_NODE DT_ALIAS(led0)
#define LEDB_NODE DT_ALIAS(led1)

static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));

#define PIN_LED_VERDEP    4   // PTD4 (Pino D2 Arduino) - Verde Pedestre
#define PIN_LED_VERMELHOP 12  // PTA12 (Pino D3 Arduino) - Vermelho Pedestre

/* Prioridade 5 (menor que o main=0): garante que main configure os pinos antes */
#define PRIO_THREAD_NOTURNO_PEDESTRE 5
#define PRIO_THREAD_NOTURNO_VEICULO  5

#if DT_NODE_HAS_STATUS(LEDR_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDG_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDB_NODE, okay)

static const struct gpio_dt_spec ledr = GPIO_DT_SPEC_GET(LEDR_NODE, gpios);
static const struct gpio_dt_spec ledg = GPIO_DT_SPEC_GET(LEDG_NODE, gpios);
static const struct gpio_dt_spec ledb = GPIO_DT_SPEC_GET(LEDB_NODE, gpios);

#else
#error "LEDs não encontrados"
#endif

static volatile bool ModoNoturno = false;

static void set_rgb_veiculo(int r, int g, int b)
{
    gpio_pin_set_dt(&ledr, r);
    gpio_pin_set_dt(&ledg, g);
    gpio_pin_set_dt(&ledb, b);
}

static void set_rg_pedrestre(int r, int g)
{
    gpio_pin_set(port_a, PIN_LED_VERMELHOP, r);
    gpio_pin_set(port_d, PIN_LED_VERDEP, g);
}

static void pedestre_modo_noturno(void)
{
    if (ModoNoturno) {
        set_rg_pedrestre(1, 0);
        k_msleep(1000);
        set_rg_pedrestre(0, 0);
        k_msleep(1000);
    } else {
        k_msleep(50); 
    }
}

static void carro_modo_noturno(void)
{
    if (ModoNoturno) {
        set_rgb_veiculo(1, 1, 0); // Amarelo (Vermelho + Verde)
        k_msleep(1000);
        set_rgb_veiculo(0, 0, 0);
        k_msleep(1000);
    } else {
        k_msleep(50);
    }
}

void thread_noturno_pedestre(void *p1, void *p2, void *p3)
{
    while (1) {
        pedestre_modo_noturno();
    }
}

void thread_noturno_veiculo(void *p1, void *p2, void *p3)
{
    while (1) {
        carro_modo_noturno();
    }
}


K_THREAD_DEFINE(noturno_pedestre_tid, 1024, thread_noturno_pedestre,
                NULL, NULL, NULL, PRIO_THREAD_NOTURNO_PEDESTRE, 0, 0);

K_THREAD_DEFINE(noturno_veiculo_tid, 1024, thread_noturno_veiculo,
                NULL, NULL, NULL, PRIO_THREAD_NOTURNO_VEICULO, 0, 0);

int main(void)
{
    gpio_pin_configure_dt(&ledr, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledg, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure(port_d, PIN_LED_VERDEP, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_a, PIN_LED_VERMELHOP, GPIO_OUTPUT_INACTIVE);

#if defined(PLACA1)
    ModoNoturno = true;
    while (1) {
        k_msleep(500); // Libera a CPU para as threads rodarem
    }

#elif defined(PLACA2)
    while (1) {
        k_msleep(100);
    }
#endif

    return 0;
}