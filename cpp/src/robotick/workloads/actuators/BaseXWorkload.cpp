// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#if defined(ROBOTICK_PLATFORM_ESP32)
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
			set_motor_speed(0, 0.0f, outputs.motor1_speed);
			set_motor_speed(1, 0.0f, outputs.motor2_speed);
			set_motor_speed(2, 0.0f, outputs.motor3_speed);
			set_motor_speed(3, 0.0f, outputs.motor4_speed);
		}

		void tick(const TickInfo&)
		{
			set_motor_speed(0, inputs.motor1_speed, outputs.motor1_speed);
			set_motor_speed(1, inputs.motor2_speed, outputs.motor2_speed);
			set_motor_speed(2, inputs.motor3_speed, outputs.motor3_speed);
			set_motor_speed(3, inputs.motor4_speed, outputs.motor4_speed);

			// ROBOTICK_INFO("BaseXWorkload::tick() inputs:  m1=%.3f  m2=%.3f  m3=%.3f  m4=%.3f outputs: m1=%.3f  m2=%.3f  m3=%.3f  m4=%.3f",
			// 	inputs.motor1_speed, inputs.motor2_speed, inputs.motor3_speed, inputs.motor4_speed, outputs.motor1_speed, outputs.motor2_speed,
			// 	outputs.motor3_speed, outputs.motor4_speed);
		}

		void set_motor_speed(uint8_t index, float input_speed, float& output_ref)
		{
			if (index > 3)
				return;

			float clamped = std::max(-config.max_motor_speed, std::min(config.max_motor_speed, input_speed));

#if defined(ROBOTICK_PLATFORM_ESP32)
			int8_t duty = static_cast<int8_t>(clamped * 127.0);
			uint8_t reg = BASEX_PWM_DUTY_ADDR + index;

			constexpr uint32_t BASEX_I2C_FREQ = 400000;

			m5::In_I2C.writeRegister8(BASEX_I2C_ADDR, reg, static_cast<uint8_t>(duty), BASEX_I2C_FREQ);
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)

			output_ref = clamped;
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(BaseXWorkload, BaseXConfig, BaseXInputs, BaseXOutputs)

} // namespace robotick
