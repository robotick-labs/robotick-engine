// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include <M5Unified.h>

namespace robotick {

constexpr uint8_t BASEX_I2C_ADDR = 0x22;
constexpr uint8_t BASEX_PWM_DUTY_ADDR = 0x20;

struct BaseXConfig {
	double max_motor_speed = 1.0;
};
ROBOTICK_BEGIN_FIELDS(BaseXConfig)
ROBOTICK_FIELD(BaseXConfig, double, max_motor_speed)
ROBOTICK_END_FIELDS()

struct BaseXInputs {
	double motor1_speed = 0.0;
	double motor2_speed = 0.0;
	double motor3_speed = 0.0;
	double motor4_speed = 0.0;
};
ROBOTICK_BEGIN_FIELDS(BaseXInputs)
ROBOTICK_FIELD(BaseXInputs, double, motor1_speed)
ROBOTICK_FIELD(BaseXInputs, double, motor2_speed)
ROBOTICK_FIELD(BaseXInputs, double, motor3_speed)
ROBOTICK_FIELD(BaseXInputs, double, motor4_speed)
ROBOTICK_END_FIELDS()

struct BaseXOutputs {
	double motor1_speed = 0.0;
	double motor2_speed = 0.0;
	double motor3_speed = 0.0;
	double motor4_speed = 0.0;
};
ROBOTICK_BEGIN_FIELDS(BaseXOutputs)
ROBOTICK_FIELD(BaseXOutputs, double, motor1_speed)
ROBOTICK_FIELD(BaseXOutputs, double, motor2_speed)
ROBOTICK_FIELD(BaseXOutputs, double, motor3_speed)
ROBOTICK_FIELD(BaseXOutputs, double, motor4_speed)
ROBOTICK_END_FIELDS()

struct BaseXWorkload {
	BaseXInputs inputs;
	BaseXOutputs outputs;
	BaseXConfig config;

	~BaseXWorkload()
	{
		set_motor_speed(0, 0.0, outputs.motor1_speed);
		set_motor_speed(1, 0.0, outputs.motor2_speed);
		set_motor_speed(2, 0.0, outputs.motor3_speed);
		set_motor_speed(3, 0.0, outputs.motor4_speed);		
	}

	void tick(const TickInfo&) {
		set_motor_speed(0, inputs.motor1_speed, outputs.motor1_speed);
		set_motor_speed(1, inputs.motor2_speed, outputs.motor2_speed);
		set_motor_speed(2, inputs.motor3_speed, outputs.motor3_speed);
		set_motor_speed(3, inputs.motor4_speed, outputs.motor4_speed);
	}

	void set_motor_speed(uint8_t index, double input_speed, double& output_ref) {
		if (index > 3) return;

		double clamped = std::max(-config.max_motor_speed, std::min(config.max_motor_speed, input_speed));
		int8_t duty = static_cast<int8_t>(clamped * 127.0);
		uint8_t reg = BASEX_PWM_DUTY_ADDR + index;

		constexpr uint32_t BASEX_I2C_FREQ = 400000;
		m5::In_I2C.writeRegister8(BASEX_I2C_ADDR, reg, static_cast<uint8_t>(duty), BASEX_I2C_FREQ);

		output_ref = clamped;
	}
};

ROBOTICK_DEFINE_WORKLOAD(BaseXWorkload, BaseXConfig, BaseXInputs, BaseXOutputs)

} // namespace robotick
