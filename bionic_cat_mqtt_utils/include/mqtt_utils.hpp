#ifndef MQTT_UTILS_HPP
#define MQTT_UTILS_HPP

#include <chrono>
#include <cstdint>

/**
 * @brief Gets the current timestamp in nanoseconds.
 * 
 * @return The current timestamp in nanoseconds.
 */
inline uint64_t getStamp()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    return nanoseconds;
}

#endif // MQTT_UTILS_HPP