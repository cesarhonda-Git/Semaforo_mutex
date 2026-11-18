#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#define LEDR_NODE DT_ALIAS(led2)
#define LEDG_NODE DT_ALIAS(led0)
#define LEDB_NODE DT_ALIAS(led1)

static const struct device *port_a = DEVICE_DT_GET(DT_NODELABEL(gpioa)); // Pinos PTA...
static const struct device *port_d = DEVICE_DT_GET(DT_NODELABEL(gpiod)); // Pinos PTD...


#define PIN_LED_VERDEP    4   // PTD4  - Verde Pedestre
#define PIN_LED_VERMELHOP 12  // PTA12 - Vermelho Pedestre


#if DT_NODE_HAS_STATUS(LEDR_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDG_NODE, okay) && \
    DT_NODE_HAS_STATUS(LEDB_NODE, okay)

static const struct gpio_dt_spec ledr = GPIO_DT_SPEC_GET(LEDR_NODE, gpios);
static const struct gpio_dt_spec ledg = GPIO_DT_SPEC_GET(LEDG_NODE, gpios);
static const struct gpio_dt_spec ledb = GPIO_DT_SPEC_GET(LEDB_NODE, gpios);

#else
#error "LEDs não encontrados"
#endif


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


int main(void)
{
    gpio_pin_configure_dt(&ledr, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledg, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure(port_d, PIN_LED_VERDEP, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_a, PIN_LED_VERMELHOP, GPIO_OUTPUT_INACTIVE);

#if defined(SAIDA)
    gpio_pin_configure(port_a, 17, GPIO_OUTPUT_INACTIVE);
    while (1) {
        gpio_pin_set(port_a, 17, 0);
        set_rgb_veiculo(1, 1, 1);
        k_msleep(100); // Libera a CPU para as threads rodarem
    }

#elif defined(ENTRADA)
    gpio_pin_configure(port_a, 16, GPIO_INPUT | GPIO_PULL_DOWN);
    int estado;
    while (1) {
        estado = gpio_pin_get(port_a, 16);
        if(estado == 0){ // recebendo sinal
            set_rgb_veiculo(0, 1, 0);
        }
        else{ // nao recebendo sinal
            set_rgb_veiculo(1, 0, 0);
        }
        printk("LEitura: %d\n", estado);
        k_msleep(100);
    }
#endif

    return 0;
}