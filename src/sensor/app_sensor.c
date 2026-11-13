/**
 * @file app_sensor.c
 * @brief Periodic BME280 temperature, pressure and humidity readings.
 *
 * The sensor is the devicetree node `bme280` on I2C1 (SCL PB8 / D15,
 * SDA PB9 / D14). Readings run in their own thread so the I2C transfers and
 * conversion time do not delay the LED loop in main().
 */
#include "app_sensor.h"
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

/* Readings are compiled in at DBG but hidden at boot: the shell log backend
 * starts every module at INF (prj.conf). `log enable dbg app_sensor` shows them.
 */
LOG_MODULE_REGISTER(app_sensor, LOG_LEVEL_DBG);

#define SENSOR_PERIOD_MS    2000
#define SENSOR_STACK_SIZE   1024
#define SENSOR_THREAD_PRIO  7

static const struct device *const bme280 = DEVICE_DT_GET(DT_NODELABEL(bme280));

K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread;

/** @brief Sensor value in thousandths, split for logging without float support. */
struct milli_parts {
	const char *sign;   /**< "-" for negative values, "" otherwise */
	uint32_t whole;     /**< Integer part of the magnitude */
	uint32_t frac;      /**< Thousandths of the magnitude, 0..999 */
};

/**
 * @brief Split a sensor value into sign, integer and thousandths.
 * @param v Value from sensor_channel_get().
 * @return The split value.
 */
static struct milli_parts to_parts(const struct sensor_value *v)
{
	int64_t milli = sensor_value_to_milli(v);
	struct milli_parts p = { .sign = "" };

	if (milli < 0) {
		p.sign = "-";
		milli = -milli;
	}

	p.whole = (uint32_t)(milli / 1000);
	p.frac = (uint32_t)(milli % 1000);

	return p;
}

/**
 * @brief Fetch one sample and read the three channels.
 * @param[out] temp Temperature in degrees Celsius.
 * @param[out] press Pressure in kPa.
 * @param[out] hum Relative humidity in percent.
 * @return 0 on success, otherwise the first error from the sensor API.
 */
static int read_sample(struct sensor_value *temp, struct sensor_value *press,
		       struct sensor_value *hum)
{
	int ret = sensor_sample_fetch(bme280);

	if (ret == 0) {
		ret = sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, temp);
	}

	if (ret == 0) {
		ret = sensor_channel_get(bme280, SENSOR_CHAN_PRESS, press);
	}

	if (ret == 0) {
		ret = sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, hum);
	}

	return ret;
}

/**
 * @brief Thread body: log one reading (debug level) every SENSOR_PERIOD_MS.
 * @param p1 Unused.
 * @param p2 Unused.
 * @param p3 Unused.
 */
static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
	struct sensor_value temp;
	struct sensor_value press;
	struct sensor_value hum;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		int ret = read_sample(&temp, &press, &hum);

		if (ret == 0) {
			const struct milli_parts t = to_parts(&temp);
			const struct milli_parts p = to_parts(&press);
			const struct milli_parts h = to_parts(&hum);

			LOG_DBG("T %s%" PRIu32 ".%02" PRIu32 " C  P %s%" PRIu32 ".%03" PRIu32
				" kPa  H %s%" PRIu32 ".%01" PRIu32 " %%RH",
				t.sign, t.whole, t.frac / 10U,
				p.sign, p.whole, p.frac,
				h.sign, h.whole, h.frac / 100U);
		} else {
			LOG_WRN("BME280 read failed (%d)", ret);
		}

		(void)k_msleep(SENSOR_PERIOD_MS);
	}
}

int app_sensor_start(void)
{
	if (!device_is_ready(bme280)) {
		LOG_ERR("BME280 not ready: check wiring, 3V3 and I2C address");
		return -ENODEV;
	}

	(void)k_thread_create(&sensor_thread, sensor_stack,
			      K_THREAD_STACK_SIZEOF(sensor_stack), sensor_thread_fn,
			      NULL, NULL, NULL, SENSOR_THREAD_PRIO, 0, K_NO_WAIT);
	(void)k_thread_name_set(&sensor_thread, "sensor");

	return 0;
}
