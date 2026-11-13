/**
 * @file enc28j60_revid.c
 * @brief Log the ENC28J60 silicon revision (EREVID) once at boot.
 *
 * Zephyr's driver defines EREVID but never reads it, and the revision decides
 * which silicon errata (DS80349C) apply. This runs at POST_KERNEL priority
 * ENC_REVID_INIT_PRIO, after the SPI controller and before the ENC28J60
 * driver, so it is the chip's only SPI user while it runs. The driver
 * soft-resets the chip afterwards.
 */
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(enc_revid, LOG_LEVEL_INF);

#define ENC_REVID_INIT_PRIO 55

BUILD_ASSERT((CONFIG_SPI_INIT_PRIORITY < ENC_REVID_INIT_PRIO) &&
		     (ENC_REVID_INIT_PRIO < CONFIG_ETH_INIT_PRIORITY),
	     "EREVID read must run between SPI and ENC28J60 driver init");

/**
 * @name SPI opcodes (DS39662C Table 4-1), OR-ed with a 5-bit register address
 * @{
 */
#define ENC_OP_RCR 0x00U /**< Read Control Register */
#define ENC_OP_BFS 0x80U /**< Bit Field Set (ETH registers only) */
#define ENC_OP_BFC 0xA0U /**< Bit Field Clear (ETH registers only) */
/** @} */

#define ENC_REG_ECON1    0x1FU /**< ECON1, mapped in every bank */
#define ENC_ECON1_BSEL   0x03U /**< ECON1.BSEL<1:0>, bank select (Register 3-1) */
#define ENC_REG_EREVID   0x12U /**< EREVID, bank 3 (DS80349C, page 1) */
#define ENC_EREVID_MASK  0x1FU /**< EREVID<4:0> (DS39662C Table 3-2) */

static const struct spi_dt_spec enc_spi = SPI_DT_SPEC_GET(DT_NODELABEL(enc28j60), SPI_WORD_SET(8));

/**
 * @brief Run one two-byte command (opcode/address, data) with CS held low.
 * @param op ENC_OP_* opcode.
 * @param addr 5-bit register address.
 * @param data Second byte sent (bit mask for BFS/BFC, dummy for RCR).
 * @param out Second byte received (register value for RCR on an ETH register), or NULL.
 * @return 0 on success, negative errno from the SPI driver otherwise.
 */
static int enc_cmd(uint8_t op, uint8_t addr, uint8_t data, uint8_t *out)
{
	uint8_t tx[2] = {(uint8_t)(op | addr), data};
	uint8_t rx[2] = {0U, 0U};
	const struct spi_buf tx_buf = {.buf = tx, .len = sizeof(tx)};
	const struct spi_buf rx_buf = {.buf = rx, .len = sizeof(rx)};
	const struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1U};
	const struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1U};
	int ret = spi_transceive_dt(&enc_spi, &tx_set, &rx_set);

	if ((ret == 0) && (out != NULL)) {
		*out = rx[1];
	}

	return ret;
}

/**
 * @brief Map an EREVID value to the silicon revision name (DS80349C Table 1).
 * @param id EREVID<4:0>.
 * @return Revision name, or "unknown".
 */
static const char *enc_rev_name(uint8_t id)
{
	const char *name;

	switch (id) {
	case 0x02U:
		name = "B1";
		break;
	case 0x04U:
		name = "B4";
		break;
	case 0x05U:
		name = "B5";
		break;
	case 0x06U:
		name = "B7";
		break;
	default:
		name = "unknown";
		break;
	}

	return name;
}

/**
 * @brief Read and log EREVID. Failures are logged only, so boot continues.
 * @return 0 always.
 */
static int enc_revid_log(void)
{
	uint8_t id = 0U;
	int ret;

	if (!spi_is_ready_dt(&enc_spi)) {
		LOG_WRN("SPI bus not ready, EREVID not read");
		return 0;
	}

	/* ETH registers are readable before the oscillator start-up timer expires
	 * (DS39662C 2.2), so no CLKRDY wait is needed.
	 */
	ret = enc_cmd(ENC_OP_BFS, ENC_REG_ECON1, ENC_ECON1_BSEL, NULL);
	if (ret == 0) {
		ret = enc_cmd(ENC_OP_RCR, ENC_REG_EREVID, 0x00U, &id);
	}

	/* Back to bank 0 even after a failed read */
	(void)enc_cmd(ENC_OP_BFC, ENC_REG_ECON1, ENC_ECON1_BSEL, NULL);

	if (ret != 0) {
		LOG_WRN("EREVID read failed: %d", ret);
		return 0;
	}

	id = (uint8_t)(id & ENC_EREVID_MASK);
	LOG_INF("ENC28J60 EREVID 0x%02x: silicon %s", id, enc_rev_name(id));

	return 0;
}

SYS_INIT(enc_revid_log, POST_KERNEL, ENC_REVID_INIT_PRIO);
