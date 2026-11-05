/**
 * @file main.cpp
 * @brief Memory layout demo for STM32 using libopencm3.
 */

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include <cstddef>
#include <cstdint>

namespace {

constexpr uint32_t kLedPort = GPIOA;
constexpr uint16_t kLedPin = GPIO5;    // Nucleo LD2
constexpr uint32_t kUartPort = GPIOA;
constexpr uint16_t kUartTxPin = GPIO2; // USART2_TX (ST-LINK VCP)
constexpr uint16_t kUartRxPin = GPIO3; // USART2_RX (ST-LINK VCP)

/* .data: initialized read/write global in RAM (flash image copied at reset). */
uint32_t g_data_counter = 0x12345678;

/* .bss: zero-initialized globals in RAM (cleared at reset). */
uint32_t g_bss_counter;
uint8_t g_bss_buffer[32];

/* .rodata: constants in flash. */
const char kRodataBanner[] = "Memory layout demo (.data/.bss/.rodata/stack/heap)\r\n";

volatile uint32_t system_millis = 0;

extern "C" uint8_t _sdata;
extern "C" uint8_t _edata;
extern "C" uint8_t _sbss;
extern "C" uint8_t _ebss;
extern "C" uint8_t _heap_start;
extern "C" uint8_t _heap_end;
extern "C" uint8_t _stack_top;
extern "C" uint8_t _stack_limit;
extern "C" void *_sbrk(ptrdiff_t increment);

extern "C" void SysTick_Handler(void)
{
    system_millis = system_millis + 1;
}

void clock_setup()
{
    /*
     * Renode models the HSE/PLL path less reliably than HSI for the F103.
     * Using the HSI-driven PLL setup keeps the startup deterministic in the
     * emulator while still running the MCU at a useful clock rate.
     */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_48MHZ]);
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);
}

void gpio_setup()
{
    gpio_set_mode(kLedPort, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, kLedPin);
    gpio_set_mode(kUartPort, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, kUartTxPin);
    gpio_set_mode(kUartPort, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, kUartRxPin);
}

void uart_setup()
{
    usart_set_baudrate(USART2, 115200);
    usart_set_databits(USART2, 8);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_mode(USART2, USART_MODE_TX_RX);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
    usart_enable(USART2);
}

void systick_setup()
{
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    systick_set_reload(6000U - 1U); // 48 MHz / 8 / 6000 = 1 kHz
    systick_interrupt_enable();
    systick_counter_enable();
}

void uart_write(const char *text)
{
    while (*text != '\0') {
        usart_send_blocking(USART2, static_cast<uint16_t>(*text));
        ++text;
    }
}

void uart_write_hex32(uint32_t value)
{
    constexpr char digits[] = "0123456789ABCDEF";
    uart_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t nibble = (value >> shift) & 0xF;
        usart_send_blocking(USART2, static_cast<uint16_t>(digits[nibble]));
    }
}

void uart_write_dec(uint32_t value)
{
    char buffer[11] = {};
    int index = 10;
    if (value == 0U) {
        uart_write("0");
        return;
    }

    while (value > 0U && index > 0) {
        const uint32_t digit = value % 10U;
        buffer[--index] = static_cast<char>('0' + digit);
        value /= 10U;
    }

    uart_write(&buffer[index]);
}

void print_symbol(const char *name, const void *address)
{
    uart_write(name);
    uart_write(": ");
    uart_write_hex32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(address)));
    uart_write("\r\n");
}

void print_memory_layout()
{
    uint32_t stack_probe = 0xA5A5A5A5U;

    uart_write(kRodataBanner);

    print_symbol("&g_data_counter (.data)", &g_data_counter);
    print_symbol("&g_bss_counter (.bss)", &g_bss_counter);
    print_symbol("kRodataBanner (.rodata)", kRodataBanner);
    print_symbol("&stack_probe (stack)", &stack_probe);

    print_symbol("_sdata", &_sdata);
    print_symbol("_edata", &_edata);
    print_symbol("_sbss", &_sbss);
    print_symbol("_ebss", &_ebss);
    print_symbol("_heap_start", &_heap_start);
    print_symbol("_heap_end / _stack_limit", &_heap_end);
    print_symbol("_stack_top", &_stack_top);

    const uint32_t data_size = static_cast<uint32_t>(&_edata - &_sdata);
    const uint32_t bss_size = static_cast<uint32_t>(&_ebss - &_sbss);
    const uint32_t heap_capacity = static_cast<uint32_t>(&_heap_end - &_heap_start);

    uart_write(".data bytes: ");
    uart_write_dec(data_size);
    uart_write("\r\n");
    uart_write(".bss bytes: ");
    uart_write_dec(bss_size);
    uart_write("\r\n");
    uart_write("heap capacity bytes: ");
    uart_write_dec(heap_capacity);
    uart_write("\r\n");
}

void heap_demo()
{
    void *heap_block = _sbrk(64);
    if (heap_block == reinterpret_cast<void *>(-1)) {
        uart_write("heap alloc failed\r\n");
        return;
    }

    uart_write("heap alloc 64 bytes at ");
    uart_write_hex32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(heap_block)));
    uart_write("\r\n");
}

void delay_ms(uint32_t delay)
{
    const uint32_t target = system_millis + delay;
    while (system_millis < target) {
    }
}

} // namespace

extern "C" int main()
{
    g_bss_counter = g_bss_counter + 1U;
    g_data_counter = g_data_counter + 1U;
    g_bss_buffer[0] = 0x42U;

    clock_setup();
    gpio_setup();
    uart_setup();
    systick_setup();

    uart_write("STM32F103RB startup OK\r\n");
    print_memory_layout();
    heap_demo();

    while (true) {
        gpio_toggle(kLedPort, kLedPin);
        uart_write("tick data=");
        uart_write_hex32(g_data_counter);
        uart_write(" bss=");
        uart_write_dec(g_bss_counter);
        uart_write("\r\n");

        g_data_counter = g_data_counter + 1U;
        g_bss_counter = g_bss_counter + 1U;
        delay_ms(500);
    }
}
