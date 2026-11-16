#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

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

static void set_rgb(int r, int g, int b)
{
    gpio_pin_set_dt(&ledr, r);
    gpio_pin_set_dt(&ledg, g);
    gpio_pin_set_dt(&ledb, b);
}


int main(void){
    gpio_pin_configure_dt(&ledr, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledg, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_INACTIVE);

#if defined(PLACA1)
// codigo para a placa 1
    while(1){
        set_rgb(1,0,0);
        k_msleep(100);
        set_rgb(0,1,0);
        k_msleep(100);
        set_rgb(0,0,1);
        k_msleep(100);
    }


#elif defined(PLACA2)
// código para a placa 2
    while(1){
        set_rgb(1,1,0);
        k_msleep(100);
        set_rgb(0,1,1);
        k_msleep(100);
        set_rgb(1,0,1);
        k_msleep(100);
    }

#endif

    return 0;
    }


