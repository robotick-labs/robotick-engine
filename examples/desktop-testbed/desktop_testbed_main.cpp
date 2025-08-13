// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/model/WorkloadSeed.h"
#include "robotick/platform/EntryPoint.h"
#include "robotick/platform/Signals.h"
#include "robotick/platform/Threading.h"

robotick::AtomicFlag g_stop_flag;

void signal_handler()
{
	g_stop_flag.set();
}

void populate_model_hello_rc(robotick::Model& model)
{
	const float tick_rate_hz_main = 30.0f;	 //  slightly slower than the 30Hz at which the camera updates, so as to not need to wait for reads
	const float tick_rate_hz_console = 5.0f; //  no real benefit of debug-telemetry being faster than this

	const robotick::WorkloadSeed& remote_control = model.add("RemoteControlWorkload", "remote_control").set_tick_rate_hz(tick_rate_hz_main);

	const robotick::WorkloadSeed& face = model.add("FaceDisplayWorkload", "face").set_tick_rate_hz(tick_rate_hz_main);

	const robotick::WorkloadSeed& camera = model.add("CameraWorkload", "camera").set_tick_rate_hz(tick_rate_hz_main);

	// const robotick::WorkloadSeed& console_telem = model.add("ConsoleTelemetryWorkload", "console").set_tick_rate_hz(tick_rate_hz_console);

	const robotick::WorkloadSeed& root =
		model.add("SyncedGroupWorkload", "root_group").set_children({&remote_control, &face, &camera}).set_tick_rate_hz(tick_rate_hz_main);

	model.connect("camera.outputs.jpeg_data", "remote_control.inputs.jpeg_data");

	model.set_root_workload(root);
}

ROBOTICK_ENTRYPOINT
{
	robotick::setup_exit_handler(signal_handler);

	robotick::Model model;
	populate_model_hello_rc(model);

	robotick::Engine engine;
	engine.load(model);
	engine.run(g_stop_flag);

#if !defined(ROBOTICK_PLATFORM_ESP32)
	return 0;
#endif
}