// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#include <chrono>

#if defined(ROBOTICK_PLATFORM_ESP32)
#include "driver/i2c.h"
#include <M5Unified.h>
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)

namespace robotick
{

	constexpr uint8_t BASEX_I2C_ADDR = 0x22;
	constexpr uint8_t BASEX_PWM_DUTY_ADDR = 0x20;

	struct BaseXConfig
	{
		float max_motor_speed = 1.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(BaseXConfig)
	ROBOTICK_STRUCT_FIELD(BaseXConfig, float, max_motor_speed)
	ROBOTICK_REGISTER_STRUCT_END(BaseXConfig)

	struct BaseXInputs
	{
		float motor1_speed = 0.0f;
		float motor2_speed = 0.0f;
		float motor3_speed = 0.0f;
		float motor4_speed = 0.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(BaseXInputs)
	ROBOTICK_STRUCT_FIELD(BaseXInputs, float, motor1_speed)
	ROBOTICK_STRUCT_FIELD(BaseXInputs, float, motor2_speed)
	ROBOTICK_STRUCT_FIELD(BaseXInputs, float, motor3_speed)
	ROBOTICK_STRUCT_FIELD(BaseXInputs, float, motor4_speed)
	ROBOTICK_REGISTER_STRUCT_END(BaseXInputs)

	struct BaseXOutputs
	{
		float motor1_speed = 0.0f;
		float motor2_speed = 0.0f;
		float motor3_speed = 0.0f;
		float motor4_speed = 0.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(BaseXOutputs)
	ROBOTICK_STRUCT_FIELD(BaseXOutputs, float, motor1_speed)
	ROBOTICK_STRUCT_FIELD(BaseXOutputs, float, motor2_speed)
	ROBOTICK_STRUCT_FIELD(BaseXOutputs, float, motor3_speed)
	ROBOTICK_STRUCT_FIELD(BaseXOutputs, float, motor4_speed)
	ROBOTICK_REGISTER_STRUCT_END(BaseXOutputs)

	struct BaseXWorkload
	{
		BaseXInputs inputs;
		BaseXOutputs outputs;
		BaseXConfig config;

		~BaseXWorkload()
		{
			inputs.motor1_speed = 0.0f;
			inputs.motor2_speed = 0.0f;
			inputs.motor3_speed = 0.0f;
			inputs.motor4_speed = 0.0f;

			set_motor_speeds();
		}

		void tick(const TickInfo&) { set_motor_speeds(); }

		void set_motor_speeds()
		{
#if defined(ROBOTICK_PLATFORM_ESP32)

			inputs.motor1_speed = 0.0f;
			inputs.motor2_speed = 0.0f;

			// Create duty values
			uint8_t duties[4] = {
				static_cast<uint8_t>(std::clamp(inputs.motor1_speed, -config.max_motor_speed, config.max_motor_speed) * 127.0f),
				static_cast<uint8_t>(std::clamp(inputs.motor2_speed, -config.max_motor_speed, config.max_motor_speed) * 127.0f),
				static_cast<uint8_t>(std::clamp(inputs.motor3_speed, -config.max_motor_speed, config.max_motor_speed) * 127.0f),
				static_cast<uint8_t>(std::clamp(inputs.motor4_speed, -config.max_motor_speed, config.max_motor_speed) * 127.0f),
			};

			constexpr uint32_t BASEX_I2C_FREQ = 400000;

			// Begin I2C transaction manually
			m5::In_I2C.writeRegister(BASEX_I2C_ADDR, BASEX_PWM_DUTY_ADDR, duties, sizeof(duties), BASEX_I2C_FREQ);
#endif

			outputs.motor1_speed = inputs.motor1_speed;
			outputs.motor2_speed = inputs.motor2_speed;
			outputs.motor3_speed = inputs.motor3_speed;
			outputs.motor4_speed = inputs.motor4_speed;
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(BaseXWorkload, BaseXConfig, BaseXInputs, BaseXOutputs)

} // namespace robotick
