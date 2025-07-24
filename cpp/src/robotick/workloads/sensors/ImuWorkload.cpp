// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#if defined(ROBOTICK_PLATFORM_ESP32)
#include <M5Unified.h>
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)

namespace robotick
{

	struct ImuConfig
	{
		// Currently unused, placeholder for future features
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(ImuConfig)
	ROBOTICK_REGISTER_STRUCT_END(ImuConfig)

	struct ImuInputs
	{
		// Currently unused, placeholder for future features (e.g. calibration trigger)
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(ImuInputs)
	ROBOTICK_REGISTER_STRUCT_END(ImuInputs)

	struct ImuOutputs
	{
		float accel_x = 0.0f;
		float accel_y = 0.0f;
		float accel_z = 0.0f;

		float gyro_x = 0.0f;
		float gyro_y = 0.0f;
		float gyro_z = 0.0f;

		float mag_x = 0.0f;
		float mag_y = 0.0f;
		float mag_z = 0.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(ImuOutputs)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, accel_x)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, accel_y)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, accel_z)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, gyro_x)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, gyro_y)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, gyro_z)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, mag_x)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, mag_y)
	ROBOTICK_STRUCT_FIELD(ImuOutputs, float, mag_z)
	ROBOTICK_REGISTER_STRUCT_END(ImuOutputs)

	struct ImuState
	{
#if defined(ROBOTICK_PLATFORM_ESP32)
		m5::IMU_Class* imu = nullptr;
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
	};

	struct ImuWorkload
	{
		ImuConfig config;
		ImuInputs inputs;
		ImuOutputs outputs;

		State<ImuState> state;

#if defined(ROBOTICK_PLATFORM_ESP32)
		void setup()
		{
			M5.begin();
			state->imu = &M5.Imu;
			m5::IMU_Class* imu = state->imu;

			if (!imu->isEnabled())
			{
				ROBOTICK_INFO("IMU not enabled — attempting init...");
				imu->begin();
			}

			if (!imu->isEnabled())
			{
				ROBOTICK_FATAL_EXIT("IMU still not enabled after init.");
			}
			else
			{
				ROBOTICK_INFO("IMU initialized successfully");
			}

			// ROBOTICK_INFO("Accel Enabled: %d", imu->accelEnabled());
			// ROBOTICK_INFO("Gyro Enabled: %d", imu->gyroEnabled());
			// ROBOTICK_INFO("Magnet Enabled: %d", imu->magnetEnabled());
		}

		void tick(const TickInfo& /*tick_info*/)
		{
			m5::IMU_Class* imu = state->imu;

			float ax, ay, az;
			if (imu && imu->getAccel(&ax, &ay, &az))
			{
				outputs.accel_x = ax;
				outputs.accel_y = ay;
				outputs.accel_z = az;
			}
			else
			{
				ROBOTICK_WARNING("IMU read failed (accelerometer)");
			}

			float gx, gy, gz;
			if (imu && imu->getGyro(&gx, &gy, &gz))
			{
				outputs.gyro_x = gx;
				outputs.gyro_y = gy;
				outputs.gyro_z = gz;
			}
			else
			{
				ROBOTICK_WARNING("IMU read failed (gyroscope)");
			}

			float mx, my, mz;
			if (imu && imu->getMag(&mx, &my, &mz))
			{
				outputs.mag_x = mx;
				outputs.mag_y = my;
				outputs.mag_z = mz;
			}
			else
			{
				ROBOTICK_WARNING("IMU read failed (magnetometer)");
			}
		}
#else
		void setup() {}
		void tick(const TickInfo& /*tick_info*/)
		{
			outputs.accel_x = 0.0f;
			outputs.accel_y = 0.0f;
			outputs.accel_z = 0.0f;
			outputs.gyro_x = 0.0f;
			outputs.gyro_y = 0.0f;
			outputs.gyro_z = 0.0f;
		}
#endif // ROBOTICK_PLATFORM_ESP32
	};

	ROBOTICK_REGISTER_WORKLOAD(ImuWorkload, ImuConfig, ImuInputs, ImuOutputs)

} // namespace robotick
