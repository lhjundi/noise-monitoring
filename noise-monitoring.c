/*
 * Project: Noise Monitoring
 * Author: Lucas Jundi Hikazudani
 * Description: This project uses a microphone to detect noise levels using 
 * periodic timer interrupts and controls a NeoPixel LED matrix accordingly.
 * The noise levels are divided into 5 levels, each represented by a different 
 * pattern and color on the LED matrix.

 * Implemented threshold levels:

| Level | Voltage Range (V) | Approx. ADC Value Range |
| ----- | ----------------- | ----------------------- |
| 0     | 0 – 0.033 V       | 0 – 41                  |
| 1     | 0.033 – 0.066 V   | 41 – 82                 |
| 2     | 0.066 – 0.099 V   | 82 – 123                |
| 3     | 0.099 – 0.132 V   | 123 – 164               |
| 4     | 0.132 V and above | 164+                    |

*/

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "neopixel.c"

// Microphone channel and pin
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// ADC settings
#define ADC_CLOCK_DIV 96.f
#define SAMPLES 200
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f)
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f / 5.f)

// NeoPixel LED settings
#define LED_PIN 7
#define LED_COUNT 25
#define abs(x) ((x < 0) ? (-x) : (x))

// Timer interrupt interval (30ms)
#define TIMER_INTERVAL_US -30000

// DMA channel and configuration
uint dma_channel;
dma_channel_config dma_cfg;

// ADC buffer for samples
uint16_t adc_buffer[SAMPLES];

// Timer and flags
repeating_timer_t timer;
volatile bool do_sample = false;    // Flag to indicate when to sample
volatile bool processing = false;   // Flag to prevent overlapping processing

// Function prototypes
bool timer_callback(repeating_timer_t *rt);
void select_noise_intensity();
void setup_timer();
void setup_dma();
void setup_adc();
void setup();
void level_one_noise();
void level_two_noise();
void level_three_noise();
void level_four_noise();
void first_led_ring();
void second_led_ring();
void third_led_ring();
void sample_mic();
float mic_power();
uint8_t get_intensity(float v);

int main()
{
    setup();

    while (true)
    {
        // Check if it's time to sample and not currently processing
        if (do_sample && !processing)
        {
            processing = true;  // Mark as processing to prevent overlapping
            do_sample = false;  // Reset the sampling flag
            
            select_noise_intensity();  // Process the noise level and update LEDs
            
            processing = false; // Release for next processing cycle
        }

        tight_loop_contents();
    }
}

// Timer interrupt callback
bool timer_callback(repeating_timer_t *rt)
{
    // Just set the flag to indicate sampling is needed
    // Processing will happen in the main loop
    do_sample = true;
    return true;
}

// Setup the timer interrupt
void setup_timer()
{
    add_repeating_timer_us(TIMER_INTERVAL_US, timer_callback, NULL, &timer);
}

// Process noise level and update LED patterns accordingly
void select_noise_intensity()
{
    sample_mic();
    float avg = mic_power();
    avg = 2.f * abs(ADC_ADJUST(avg));
    uint intensity = get_intensity(avg);

    npClear();

    switch (intensity)
    {
    case 0:
        break;  // No noise detected, keep LEDs off
    case 1:
        level_one_noise();  // Low noise level
        break;
    case 2:
        level_two_noise();  // Medium-low noise level
        break;
    case 3:
        level_three_noise();  // Medium-high noise level
        break;
    case 4:
        level_four_noise();  // High noise level
        break;
    }

    npWrite();  // Update the NeoPixels
    printf("%2d %8.4f\r", intensity, avg);  // Output to serial monitor
}

// Setup DMA for efficient ADC reading
void setup_dma()
{
    dma_channel = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(dma_channel);

    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, DREQ_ADC);
}

// Configure ADC for microphone reading
void setup_adc()
{
    adc_gpio_init(MIC_PIN);
    adc_init();
    adc_select_input(MIC_CHANNEL);

    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(ADC_CLOCK_DIV);
}

// Initialize all required hardware components
void setup()
{
    stdio_init_all();
    sleep_ms(2000); // Wait for serial monitor to open

    npInit(LED_PIN, LED_COUNT);
    setup_adc();
    setup_dma();
    setup_timer();
}

// LED animation patterns for different noise levels

// Highest noise level pattern (level 4)
void level_four_noise()
{
    npSetLED(12, 80, 0, 0);  // Center LED red
    first_led_ring();
    second_led_ring();
    third_led_ring();
}

// High noise level pattern (level 3)
void level_three_noise()
{
    npSetLED(12, 60, 60, 0);  // Center LED yellow
    first_led_ring();
    second_led_ring();
}

// Medium noise level pattern (level 2)
void level_two_noise()
{
    npSetLED(12, 0, 0, 120);  // Center LED blue
    first_led_ring();
}

// Low noise level pattern (level 1)
void level_one_noise()
{
    npSetLED(12, 0, 0, 80);  // Only center LED blue (dim)
}

// Outermost LED ring
void third_led_ring()
{
    npSetLED(1, 0, 0, 80);
    npSetLED(3, 0, 0, 80);
    npSetLED(5, 0, 0, 80);
    npSetLED(9, 0, 0, 80);
    npSetLED(15, 0, 0, 80);
    npSetLED(19, 0, 0, 80);
    npSetLED(21, 0, 0, 80);
    npSetLED(23, 0, 0, 80);
}

// Middle LED ring
void second_led_ring()
{
    npSetLED(2, 0, 0, 80);
    npSetLED(6, 0, 0, 80);
    npSetLED(8, 0, 0, 80);
    npSetLED(10, 0, 0, 80);
    npSetLED(14, 0, 0, 80);
    npSetLED(16, 0, 0, 80);
    npSetLED(18, 0, 0, 80);
    npSetLED(22, 0, 0, 80);
}

// Inner LED ring
void first_led_ring()
{
    npSetLED(7, 0, 0, 80);
    npSetLED(11, 0, 0, 80);
    npSetLED(13, 0, 0, 80);
    npSetLED(17, 0, 0, 80);
}

// Sample microphone input using DMA
void sample_mic()
{
    adc_fifo_drain();
    adc_run(false);

    dma_channel_configure(
        dma_channel, &dma_cfg,
        adc_buffer,
        &(adc_hw->fifo),
        SAMPLES,
        true
    );

    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
    adc_run(false);
}

// Calculate RMS power from microphone samples
float mic_power()
{
    float avg = 0.f;
    for (uint i = 0; i < SAMPLES; ++i)
        avg += adc_buffer[i] * adc_buffer[i];

    avg /= SAMPLES;
    return sqrt(avg);  // Return RMS value
}

// Convert voltage to noise intensity level (0-4)
uint8_t get_intensity(float v)
{
    uint count = 0;
    while ((v -= ADC_STEP / 20) > 0.f)
        ++count;

    return count;
}