    #include <zephyr/kernel.h>
    #include <zephyr/drivers/gpio.h>
    #include <zephyr/sys/printk.h>
    #include <zephyr/device.h>

    // --- Configuração de LEDs via DeviceTree ---
    #define LED_A_NODE DT_ALIAS(led0)  // LED verde
    #define LED_C_NODE DT_ALIAS(led2)  // LED vermelho

    static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
    static const struct gpio_dt_spec ledC = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);

    // --- Prioridades e tempos ---
    #define PRIO_THREAD_A 5   // Maior prioridade (número menor)
    #define PRIO_THREAD_B 7   // Menor prioridade
    #define PRIO_THREAD_C 7
    #define TEMPO_A_MS   1500   // Thread A dorme
    #define TEMPO_B_MS   1000   // Thread B dorme

    volatile int vez = 1; // 0 = verde; 1 = vermelho

    // ----------------------------------------------------
    // THREAD A — Tarefa curta, alta prioridade
    // ----------------------------------------------------
    void thread_A(void *p1, void *p2, void *p3)
    {
        while (1) {
            if (vez != 0) {
                k_msleep(10);
                continue;
            }
            if (vez == 0)
            {
                gpio_pin_set_dt(&ledA, 1);  // Liga LED verde
                k_msleep(TEMPO_A_MS);       // Dorme — libera CPU
                gpio_pin_set_dt(&ledA, 0);  // Desliga LED verde
                vez = 1;   
            }
        }
    }

    void thread_B(void *p1, void *p2, void *p3)
    {
        while (1) {
            if (vez != 1) {
                k_msleep(10);
                continue;
            }
            if (vez == 1)
            {
                gpio_pin_set_dt(&ledC, 1);  // Liga LED vermelho
                gpio_pin_set_dt(&ledA, 1);  // Liga LED verde    ambos = amarelo
                k_msleep(TEMPO_B_MS);       // Dorme um pouco
                gpio_pin_set_dt(&ledC, 0);  // Desliga LED vermelho
                gpio_pin_set_dt(&ledA, 0);  // Desliga LED verde
                vez = 2;
            }
        }
    }
    void thread_C(void *p1, void *p2, void *p3)
    {
        while (1) {
            if (vez != 2) {
                k_msleep(10);
                continue;
            }
            if (vez == 2)
            {
                gpio_pin_set_dt(&ledC, 1);  // Liga LED vermelho
                k_msleep(TEMPO_B_MS);       // Dorme um pouco
                gpio_pin_set_dt(&ledC, 0);  // Desliga LED vermelho
                vez = 0;
            }
        }
    }

    // ----------------------------------------------------
    // Definição das threads
    // ----------------------------------------------------
    K_THREAD_DEFINE(a_tid, 512, thread_A, NULL, NULL, NULL,
                    PRIO_THREAD_A, 0, 0);
    K_THREAD_DEFINE(b_tid, 512, thread_B, NULL, NULL, NULL,
                    PRIO_THREAD_B, 0, 0);
    K_THREAD_DEFINE(c_tid, 512, thread_C, NULL, NULL, NULL,
                    PRIO_THREAD_C, 0, 0);

    // ----------------------------------------------------
    // Função principal
    // ----------------------------------------------------
    void main(void)
    {
        // Inicializa GPIOs dos LEDs
        if (!device_is_ready(ledA.port) || !device_is_ready(ledC.port)) {
            return;
        }
        gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&ledC, GPIO_OUTPUT_INACTIVE);

        printk("=== Demonstração de Preempção com LEDs ===\n");
        printk("Thread A (verde): prioridade %d\n", PRIO_THREAD_A);
        printk("Thread B (amarelo): prioridade %d\n", PRIO_THREAD_B);
        printk("Thread C (vermelho): prioridade %d\n", PRIO_THREAD_C);

        while (1) {
            k_sleep(K_FOREVER); // Main dorme para liberar CPU.
        }
    }