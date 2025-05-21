#include "board.hpp"
#include <libopencm3/stm32/f3/memorymap.h> // For GD32F303, F3 series map is often compatible for peripherals like GPIO/SPI
                                          // Or use generic <libopencm3/stm32/memorymap.h> and ensure correct base addresses
#include <libopencm3/stm32/common/spi_common_v1.h> // For spi_set_bidirectional_transmit_only_mode etc.
                                                // GD32F30x is similar to STM32F1/F3 for SPI

// Define global instances
RFSW rfsw;
SoftSPI synthSPI(PIN_SYNTH_SCK, PIN_SYNTH_MOSI, PIN_SYNTH_MISO);
Si5351 si5351(0xC0); // Default I2C address
ADF4350 adf4350(PIN_ADF_CS, PIN_ADF_LD);
DMA_ADC dma_adc;

// <<< SPI Slave 追加 >>>
SPISlave spi_slave; // SPISlaveのインスタンスを定義
// <<< SPI Slave 追加終わり >>>


// LCD and Touch are removed, so related instances are commented out or removed
// ILI9341 lcd(PIN_LCD_DC, PIN_LCD_RESET, PIN_LCD_CS);
// XPT2046 touch(PIN_TP_CS, PIN_TP_IRQ);


void boardInitPre(void)
{
    // Configure system clock: 72MHz from 8MHz HSE for GD32F303
    // This is a typical setup; adjust if your board has a different crystal or PLL config.
    // For GD32F303, clock setup is usually more complex than simple STM32F1 rcc_clock_setup_in_hse_8mhz_out_72mhz.
    // It involves RCU (Reset and Clock Unit) registers.
    // Assuming a similar setup to STM32 for libopencm3 compatibility for now.
    // Detailed GD32 clock setup might be needed if issues arise.
    // rcc_clock_setup_in_hse_8mhz_out_72mhz(); // STM32 specific
    // For GD32F303xx:
    rcc_clock_setup_pll(&rcc_hxtal_8mhz_configs[RCC_CLOCK_PLL_120MHZ]); // Example for 120MHz SYSCLK from 8MHz HXTAL for GD32F303

    // Enable peripheral clocks
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC); // For LED
    rcc_periph_clock_enable(RCC_AFIO);  // AFIO clock for EXTI and other alternate functions on GD32
                                        // For GD32F30x, it's RCU_AFEN, not AFIO.
                                        // RCU_APB2EN |= RCU_APB2EN_AFEN; (libopencm3 might handle this via rcc_periph_clock_enable for specific peripherals)

    // <<< SPI Slave 追加 >>>
    // SPI1のクロックは spi_slave.init() 内で有効化されるか、ここでも良い
    rcc_periph_clock_enable(RCC_SPI1);
    // <<< SPI Slave 追加終わり >>>
}

void boardInit(void)
{
    // Initialize button and LED pins
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, PIN_BUTTON);
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PIN_LED);
    boardSetLED(false);

    // Initialize RF switches
    rfswInit();

    // Initialize synthesizers
    synthInit();

    // Initialize ADC
    adcInit();

    // LCD and Touch panel initialization is removed
    /*
    lcd_spi_pins_init();
    lcd_control_pins_init();
    lcd.init();
    lcd.setRotation(1);
    lcd.fillScreen(0); // Black

    touch_pins_init();
    touch.init();
    touch.setRotation(1);
    */

    // <<< SPI Slave 追加 >>>
    spi_slave.init(); // SPIスレーブを初期化
    // <<< SPI Slave 追加終わり >>>

    // Initialize SysTick for 1ms ticks
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB); // For GD32, AHB or AHB/8
    systick_set_reload(rcc_ahb_frequency / 1000 - 1); // Assuming rcc_ahb_frequency is correctly set for GD32
    systick_interrupt_enable();
    systick_counter_enable();
}

// NMI and HardFault handlers (basic implementation)
void boardNMI(void) { while(1); }
void boardHardfault(void) { while(1); }

// SysTick handler
volatile uint32_t system_millis = 0;
void boardSysTick(void) {
    system_millis++;
}

void boardSleep(uint32_t msec) {
    uint32_t target_millis = system_millis + msec;
    while(system_millis < target_millis) {
        __asm__("wfi"); // Wait for interrupt
    }
}

void boardSetLED(bool on) {
    if(on)
        gpio_clear(GPIOC, PIN_LED); // Assuming LED is active low
    else
        gpio_set(GPIOC, PIN_LED);
}

void rfswInit(void) {
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PIN_SW_CTL0 | PIN_SW_CTL1);
    rfsw.init(PIN_SW_CTL0, PIN_SW_CTL1);
}

void synthInit(void) {
    synthSPI.init();
    si5351.init(&synthSPI, PIN_SI_SDA, PIN_SI_SCL); // Software I2C via SPI pins if necessary, or use dedicated I2C
    adf4350.init(&synthSPI);
}

void synthSetFrequency(uint32_t freq) {
    // Simplified: assuming ADF4350 for higher frequencies, Si5351 for lower.
    // Actual logic might be more complex based on board design.
    if (freq > 200000000) { // Example threshold: 200 MHz
        adf4350.setFrequency(freq);
        si5351.setOutputEnable(0, false); // Disable Si5351
        si5351.setOutputEnable(1, false);
        si5351.setOutputEnable(2, false);
        adf4350.setOutputEnable(true);
    } else {
        si5351.setFrequency(0, freq); // Use CLK0 for VNA LO
        si5351.setOutputEnable(0, true);
        adf4350.setOutputEnable(false); // Disable ADF4350
    }
}
uint32_t synthGetFrequency(void) {
    // This needs to reflect which synth is active
    // For simplicity, returning ADF4350's freq if it's assumed to be the primary one used.
    return adf4350.getFrequency();
}
void synthSetPower(uint8_t power) {
    adf4350.setPower(power);
}
void synthSetReference(bool external) {
    // Placeholder - actual implementation depends on board's clocking
    (void)external;
}
void synthSetOutput(bool on) {
    adf4350.setOutputEnable(on);
    si5351.setOutputEnable(0, on); // Assuming CLK0 is used
}


void adcInit(void) {
    dma_adc.init(PIN_ADC_CH0, PIN_ADC_CH1, PIN_ADC_CH2);
}
void adcRead(uint16_t *sample_buf, int num_samples) {
    dma_adc.read_samples(sample_buf, num_samples);
}


// LCD and Touch panel related functions are removed as the hardware is not used.
/*
void lcd_spi_pins_init(void) {
    // This function is now replaced by SPISlave::gpio_init()
    // SPI1 pins: SCK=PA5, MISO=PA6, MOSI=PA7
    // NSS/CS for LCD was PA4
    // These pins will be configured for SPI Slave mode by SPISlave::gpio_init()
}

void lcd_spi_pins_deinit(void) {
    // No longer needed, or adapt if some GPIOs need to be explicitly deinitialized
}

void lcd_spi_write(uint8_t data) {
    // No longer used for LCD master
}

void lcd_spi_transfer_bulk(const uint8_t *data, int len) {
    // No longer used for LCD master
}

void lcd_control_pins_init(void) {
    // No longer needed for LCD
}

uint16_t touch_spi_readwrite(uint8_t data) {
    // No longer used for touch master
    return 0;
}

void touch_pins_init(void) {
    // No longer needed for touch
}
*/

// If a board-specific SPI slave initialization is needed beyond the SPISlave class,
// it can be placed here. For now, SPISlave::init() should handle it.
void spi_slave_init_board() {
    // This function can be used for any board-specific GPIO settings
    // or configurations that are not part of the generic SPISlave class.
    // For now, we assume SPISlave::init() handles all necessary setup.
}

