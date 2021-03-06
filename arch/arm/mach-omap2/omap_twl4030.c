/**
 * OMAP and TWL PMIC specific intializations.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated.
 * Thara Gopinath
 * Copyright (C) 2009 Texas Instruments Incorporated.
 * Nishanth Menon
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/i2c/twl.h>

#include "voltage.h"

#include "pm.h"

#define OMAP3_SRI2C_SLAVE_ADDR		0x12
#define OMAP3_VDD_MPU_SR_CONTROL_REG	0x00
#define OMAP3_VDD_CORE_SR_CONTROL_REG	0x01
#define OMAP3_VP_CONFIG_ERROROFFSET	0x00
#define OMAP3_VP_VSTEPMIN_VSTEPMIN	0x1
#define OMAP3_VP_VSTEPMAX_VSTEPMAX	0x04
#define OMAP3_VP_VLIMITTO_TIMEOUT_US	200

#define OMAP4_VP_CORE_VSTEPMAX_VSTEPMAX	0x04
#define OMAP4_SRI2C_SLAVE_ADDR		0x12
#define OMAP4_VDD_MPU_SR_VOLT_REG	0x55
#define OMAP4_VDD_MPU_SR_CMD_REG	0x56
#define OMAP4_VDD_IVA_SR_VOLT_REG	0x5B
#define OMAP4_VDD_IVA_SR_CMD_REG	0x5C
#define OMAP4_VDD_CORE_SR_VOLT_REG	0x61
#define OMAP4_VDD_CORE_SR_CMD_REG	0x62

static bool is_offset_valid;
static u8 smps_offset;
/*
 * Flag to ensure Smartreflex bit in TWL
 * being cleared in board file is not overwritten.
 */
static bool __initdata twl_sr_enable_autoinit;

#define TWL4030_DCDC_GLOBAL_CFG        0x06
#define REG_SMPS_OFFSET         0xE0
#define SMARTREFLEX_ENABLE     BIT(3)

static unsigned long twl4030_vsel_to_uv(const u8 vsel)
{
	return (((vsel * 125) + 6000)) * 100;
}

static u8 twl4030_uv_to_vsel(unsigned long uv)
{
	return DIV_ROUND_UP(uv - 600000, 12500);
}

static unsigned long twl6030_vsel_to_uv(const u8 vsel)
{
	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET
	 * efuse register the voltage range supported in
	 * standard mode can be either between 0.6V - 1.3V or
	 * 0.7V - 1.4V. In TWL6030 ES1.0 SMPS_OFFSET efuse
	 * is programmed to all 0's where as starting from
	 * TWL6030 ES1.1 the efuse is programmed to 1
	 */
	if (!is_offset_valid) {
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &smps_offset,
				REG_SMPS_OFFSET);
		is_offset_valid = true;
	}

	if (!vsel)
		return 0;
	/*
	 * There is no specific formula for voltage to vsel
	 * conversion above 1.3V. There are special hardcoded
	 * values for voltages above 1.3V. Currently we are
	 * hardcoding only for 1.35 V which is used for 1GH OPP for
	 * OMAP4430.
	 */
	if (vsel == 0x3A)
		return 1350000;

	if (smps_offset & 0x8)
		return ((((vsel - 1) * 1266) + 70900)) * 10;
	else
		return ((((vsel - 1) * 1266) + 60770)) * 10;
}

static u8 twl6030_uv_to_vsel(unsigned long uv)
{
	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET
	 * efuse register the voltage range supported in
	 * standard mode can be either between 0.6V - 1.3V or
	 * 0.7V - 1.4V. In TWL6030 ES1.0 SMPS_OFFSET efuse
	 * is programmed to all 0's where as starting from
	 * TWL6030 ES1.1 the efuse is programmed to 1
	 */
	if (!is_offset_valid) {
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &smps_offset,
				REG_SMPS_OFFSET);
		is_offset_valid = true;
	}

	if (!uv)
		return 0x00;
	/*
	 * There is no specific formula for voltage to vsel
	 * conversion above 1.3V. There are special hardcoded
	 * values for voltages above 1.3V. Currently we are
	 * hardcoding only for 1.35 V which is used for 1GH OPP for
	 * OMAP4430.
	 */
	if (uv > twl6030_vsel_to_uv(0x39)) {
		if (uv == 1350000)
			return 0x3A;
		pr_err("%s:OUT OF RANGE! non mapped vsel for %ld Vs max %ld\n",
			__func__, uv, twl6030_vsel_to_uv(0x39));
		return 0x3A;
	}

	if (smps_offset & 0x8)
		return DIV_ROUND_UP(uv - 709000, 12660) + 1;
	else
		return DIV_ROUND_UP(uv - 607700, 12660) + 1;
}

static struct omap_voltdm_pmic omap3_mpu_pmic = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.vp_erroroffset		= OMAP3_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP3_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP3_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP3430_VP1_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP3430_VP1_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP3_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP3_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP3_VDD_MPU_SR_CONTROL_REG,
	.i2c_high_speed		= true,
	.vsel_to_uv		= twl4030_vsel_to_uv,
	.uv_to_vsel		= twl4030_uv_to_vsel,
};

static struct omap_voltdm_pmic omap3_core_pmic = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.vp_erroroffset		= OMAP3_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP3_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP3_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP3430_VP2_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP3430_VP2_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP3_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP3_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP3_VDD_CORE_SR_CONTROL_REG,
	.i2c_high_speed		= true,
	.vsel_to_uv		= twl4030_vsel_to_uv,
	.uv_to_vsel		= twl4030_uv_to_vsel,
};

static struct omap_voltdm_pmic omap4_mpu_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP4_VP_MPU_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP4_VP_MPU_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP4_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP4_VDD_MPU_SR_VOLT_REG,
	.cmd_reg_addr		= OMAP4_VDD_MPU_SR_CMD_REG,
	.i2c_high_speed		= true,
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic omap4_iva_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP4_VP_IVA_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP4_VP_IVA_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP4_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP4_VDD_IVA_SR_VOLT_REG,
	.cmd_reg_addr		= OMAP4_VDD_IVA_SR_CMD_REG,
	.i2c_high_speed		= true,
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic omap443x_core_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.startup_time		= 500,
	.shutdown_time		= 500,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_CORE_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP4_VP_CORE_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP4_VP_CORE_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP4_SRI2C_SLAVE_ADDR,
	.i2c_high_speed		= true,
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
	.i2c_hscll_high		= 0x00,
	.volt_reg_addr		= OMAP4_VDD_CORE_SR_VOLT_REG,
	.cmd_reg_addr		= OMAP4_VDD_CORE_SR_CMD_REG,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static __initdata struct omap_pmic_map omap_twl_map[] = {
	{
		.name = "mpu",
		.pmic_data = &omap4_mpu_pmic,
	},
	{
		.name = "core",
		.pmic_data = &omap443x_core_pmic,
	},
	{
		.name = "iva",
		.pmic_data = &omap4_iva_pmic,
	},
	/* Terminator */
	{ .name = NULL, .pmic_data = NULL},
};

static __initdata struct omap_pmic_map omap3_twl_map[] = {
	{
		.name = "mpu",
		.pmic_data = &omap3_mpu_pmic,
	},
	{
		.name = "core",
		.pmic_data = &omap3_core_pmic,
	},
	/* Terminator */
	{ .name = NULL, .pmic_data = NULL},
};

int __init omap_twl4030_init(void)
{
	/* Reuse OMAP3430 values */
	if (cpu_is_omap3630()) {
		omap3_mpu_pmic.vp_vddmin = OMAP3630_VP1_VLIMITTO_VDDMIN;
		omap3_mpu_pmic.vp_vddmax = OMAP3630_VP1_VLIMITTO_VDDMAX;
		omap3_core_pmic.vp_vddmin = OMAP3630_VP2_VLIMITTO_VDDMIN;
		omap3_core_pmic.vp_vddmax = OMAP3630_VP2_VLIMITTO_VDDMAX;
	}

	if (cpu_is_omap446x()) {
		/* use SMPS1  for CORE instead of SMPS3 on 4430 */
		omap_twl_map[1].pmic_data->volt_reg_addr = OMAP4_VDD_MPU_SR_VOLT_REG;
		omap_twl_map[1].pmic_data->cmd_reg_addr = OMAP4_VDD_MPU_SR_CMD_REG;
		/* Adjust min / max voltages */
		omap_twl_map[0].pmic_data->vp_vddmin = OMAP4460_VP_MPU_VLIMITTO_VDDMIN;
		omap_twl_map[0].pmic_data->vp_vddmax = OMAP4460_VP_MPU_VLIMITTO_VDDMAX;
		omap_twl_map[1].pmic_data->vp_vddmin = OMAP4460_VP_CORE_VLIMITTO_VDDMIN;
		omap_twl_map[1].pmic_data->vp_vddmax = OMAP4460_VP_CORE_VLIMITTO_VDDMAX;
		omap_twl_map[2].pmic_data->vp_vddmin = OMAP4460_VP_IVA_VLIMITTO_VDDMIN;
		omap_twl_map[2].pmic_data->vp_vddmax = OMAP4460_VP_IVA_VLIMITTO_VDDMAX;
	}

	if (cpu_is_omap34xx())
		return omap_pmic_register_data(omap3_twl_map);
	else if (cpu_is_omap443x())
		return omap_pmic_register_data(&omap_twl_map[0]);
	else if (cpu_is_omap446x()) /* mpu from tps6236x */
		return omap_pmic_register_data(&omap_twl_map[1]);
	else
		return 0;
}

/**
 * omap3_twl_set_sr_bit() - Set/Clear SR bit on TWL
 * @enable: enable SR mode in twl or not
 *
 * If 'enable' is true, enables Smartreflex bit on TWL 4030 to make sure
 * voltage scaling through OMAP SR works. Else, the smartreflex bit
 * on twl4030 is cleared as there are platforms which use OMAP3 and T2 but
 * use Synchronized Scaling Hardware Strategy (ENABLE_VMODE=1) and Direct
 * Strategy Software Scaling Mode (ENABLE_VMODE=0), for setting the voltages,
 * in those scenarios this bit is to be cleared (enable = false).
 *
 * Returns 0 on success, error is returned if I2C read/write fails.
 */
int __init omap3_twl_set_sr_bit(bool enable)
{
	u8 temp;
	int ret;
	if (twl_sr_enable_autoinit)
		pr_warning("%s: unexpected multiple calls\n", __func__);

	ret = twl_i2c_read_u8(TWL4030_MODULE_PM_RECEIVER, &temp,
					TWL4030_DCDC_GLOBAL_CFG);
	if (ret)
		goto err;

	if (enable)
		temp |= SMARTREFLEX_ENABLE;
	else
		temp &= ~SMARTREFLEX_ENABLE;

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, temp,
				TWL4030_DCDC_GLOBAL_CFG);
	if (!ret) {
		twl_sr_enable_autoinit = true;
		return 0;
	}
err:
	pr_err("%s: Error access to TWL4030 (%d)\n", __func__, ret);
	return ret;
}
