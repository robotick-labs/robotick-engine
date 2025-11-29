#pragma once
#include "driver/ledc.h"

namespace robotick::esp32
{

	inline void ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits)
	{
		ledc_timer_config_t timer = {};
		timer.duty_resolution = static_cast<ledc_timer_bit_t>(resolution_bits);
		timer.freq_hz = freq;
		timer.speed_mode = LEDC_LOW_SPEED_MODE;
		timer.timer_num = LEDC_TIMER_0;
		timer.clk_cfg = LEDC_AUTO_CLK;
		ledc_timer_config(&timer);
	}

	inline void ledcAttachPin(int gpio, uint8_t channel)
	{
		ledc_channel_config_t ledc_channel = {};
		ledc_channel.channel = static_cast<ledc_channel_t>(channel);
		ledc_channel.duty = 0;
		ledc_channel.gpio_num = gpio;
		ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
		ledc_channel.hpoint = 0;
		ledc_channel.timer_sel = LEDC_TIMER_0;
		ledc_channel.intr_type = LEDC_INTR_DISABLE;
		ledc_channel.flags.output_invert = 0;
		ledc_channel_config(&ledc_channel);
	}

	inline void ledcWrite(uint8_t channel, uint32_t duty)
	{
		ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel), duty);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel));
	}

} // namespace robotick::esp32
