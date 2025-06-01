#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// #include "robotick/api.h"

extern "C" void app_main(void)
{
	// robotick::init(); // Replace with real API call if needed

	ESP_LOGI("Robotick", "Starting minimal robotick blink workload...");

	// Insert init call to Robotick here (if desired)
	while (true)
	{
		ESP_LOGI("Robotick", "Tick...");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
