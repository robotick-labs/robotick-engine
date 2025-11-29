#pragma once

#if defined(ROBOTICK_PLATFORM_ESP32)
#define ROBOTICK_ENTRYPOINT extern "C" void app_main()
#else
#define ROBOTICK_ENTRYPOINT int main()
#endif