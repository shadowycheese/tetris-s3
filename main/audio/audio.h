#ifndef AUDIO_H
#define AUDIO_H

#include <driver/i2s_std.h>
#include <cmath>
#include <esp_check.h>

#define I2S_BCLK GPIO_NUM_2
#define I2S_LRCK GPIO_NUM_1
#define I2S_DOUT GPIO_NUM_42
#define SAMPLE_RATE 44100
#define DURATION_SEC 10
#define FREQUENCY_HZ 440.0f
#define AMPLITUDE 15000 // Full-scale max for 16-bit signed is 32767
#define DMA_BUF_SAMPLES 256

class Audio
{
public:
    void init()
    {
        ESP_LOGI("AUDIO", "Configuring audio...");

        // 1. Explicitly zero-initialize channel config
        i2s_chan_config_t chan_cfg = {};
        chan_cfg.id = I2S_NUM_0;
        chan_cfg.role = I2S_ROLE_MASTER;
        chan_cfg.dma_desc_num = 6;
        chan_cfg.dma_frame_num = 240;
        chan_cfg.auto_clear = true;

        _tx_chan = NULL;
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &_tx_chan, NULL));
        ESP_LOGI("AUDIO", "new_channel OK");

        // 2. Configure std mode using explicit clock source PLL_160M
        i2s_std_config_t cfg = {};
        cfg.clk_cfg = {};
        cfg.clk_cfg.sample_rate_hz = 44100,
        cfg.clk_cfg.clk_src = I2S_CLK_SRC_PLL_160M;
        cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

        cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        cfg.gpio_cfg = {};
        cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
        cfg.gpio_cfg.bclk = I2S_BCLK;
        cfg.gpio_cfg.ws = I2S_LRCK;
        cfg.gpio_cfg.dout = I2S_DOUT;
        cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
        cfg.gpio_cfg.invert_flags = {};

        ESP_LOGI("AUDIO", "Configuring audio std mode...");
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(_tx_chan, &cfg));
        ESP_LOGI("AUDIO", "i2s_channel_init_std_mode OK!");

        ESP_ERROR_CHECK(i2s_channel_enable(_tx_chan));
        ESP_LOGI("AUDIO", "I2S enabled successfully!");

        _buffer = (int16_t *)heap_caps_malloc(256 * 1024, MALLOC_CAP_SPIRAM);

        if (_buffer == nullptr)
        {
            ESP_LOGE("AUDIO", "Failed to create buffer");
        }
    }

    void play(const uint8_t *wav, size_t size)
    {
        if (size < 12 ||
            memcmp(wav, "RIFF", 4) ||
            memcmp(wav + 8, "WAVE", 4))
        {
            return;
        }

        const uint8_t *data = NULL;
        uint32_t data_size = 0;

        size_t pos = 12;

        while (pos + 8 <= size)
        {
            const uint8_t *chunk = wav + pos;
            uint32_t chunk_size = le32(chunk + 4);

            if (memcmp(chunk, "fmt ", 4) == 0)
            {
            }
            else if (memcmp(chunk, "data", 4) == 0)
            {
                data = chunk + 8;
                data_size = chunk_size;
                break;
            }

            // WAV chunks are word aligned
            pos += 8 + chunk_size + (chunk_size & 1);
        }

        if (!data)
        {
            return;
        }

        int16_t *src = (int16_t *)data;
        for (int i = 0; i < data_size / 2; i++)
        {
            _buffer[i] = src[i] / 6;
        }

        data = (uint8_t *)_buffer;

        size_t written;

        while (data_size)
        {
            size_t n = data_size > 1024 ? 1024 : data_size;

            ESP_ERROR_CHECK(i2s_channel_write(
                _tx_chan,
                data,
                n,
                &written,
                portMAX_DELAY));

            data += written;
            data_size -= written;
        }
    }

    void play_440hz_tone()
    {
        size_t bytes_written = 0;
        int total_samples = SAMPLE_RATE * DURATION_SEC;

        // Allocate buffer for 256 stereo frames (Left + Right, 16-bit each = 4 bytes per frame)
        int16_t sample_buffer[DMA_BUF_SAMPLES * 2];

        float phase = 0.0f;
        float phase_increment = (2.0f * M_PI * FREQUENCY_HZ) / SAMPLE_RATE;

        ESP_LOGI("AUDIO", "Playing 440Hz tone for %d seconds...", DURATION_SEC);

        int samples_generated = 0;
        while (samples_generated < total_samples)
        {
            int samples_this_batch = (total_samples - samples_generated > DMA_BUF_SAMPLES)
                                         ? DMA_BUF_SAMPLES
                                         : (total_samples - samples_generated);

            // Generate 16-bit signed PCM stereo samples
            for (int i = 0; i < samples_this_batch; i++)
            {
                int16_t val = (int16_t)(sinf(phase) * AMPLITUDE);

                sample_buffer[i * 2] = val;     // Left Channel
                sample_buffer[i * 2 + 1] = val; // Right Channel

                phase += phase_increment;
                if (phase >= 2.0f * M_PI)
                {
                    phase -= 2.0f * M_PI;
                }
            }

            // Write buffer to I2S DMA with a 1-second timeout
            size_t bytes_to_write = samples_this_batch * 2 * sizeof(int16_t);
            ESP_ERROR_CHECK(i2s_channel_write(_tx_chan, sample_buffer, bytes_to_write, &bytes_written, pdMS_TO_TICKS(1000)));

            samples_generated += samples_this_batch;
        }

        ESP_LOGI("AUDIO", "Tone playback finished.");
    }

private:
    uint32_t le32(const uint8_t *p)
    {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    }

    uint16_t le16(const uint8_t *p)
    {
        return p[0] | (p[1] << 8);
    }

    i2s_chan_handle_t _tx_chan;

    int16_t *_buffer;

    float _volume = 1.0f;
};

#endif