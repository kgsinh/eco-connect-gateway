/**
 * @file app_sensor.h
 * @brief Periodic BME280 temperature, pressure and humidity readings.
 */
#ifndef APP_SENSOR_H
#define APP_SENSOR_H

/**
 * @brief Check the BME280 and start the thread that logs a reading every 2 s.
 *
 * Readings are logged at debug level, hidden at boot; show them with
 * `log enable dbg app_sensor`, hide them with `log enable inf app_sensor`.
 *
 * A failed read is logged and retried on the next period. A BMP280 (chip ID
 * 0x58, no humidity sensor) probes successfully but every read reports
 * -ENOTSUP for humidity.
 *
 * @retval 0 Sensor ready, thread started.
 * @retval -ENODEV Sensor did not probe: wiring, power or I2C address.
 */
int app_sensor_start(void);

#endif /* APP_SENSOR_H */
