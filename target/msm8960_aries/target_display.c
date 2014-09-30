/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2011-2014, Xiaomi Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <debug.h>
#include <msm_panel.h>
#include <mipi_dsi.h>
#include <dev/pm8921.h>
#include <board.h>
#include <mdp4.h>
#include <target/display.h>
#include <target/board.h>
#include <gsbi.h>
#include <i2c_qup.h>
#include <platform/iomap.h>

#include "include/panel.h"
#include "include/display_resource.h"

#define PM8921_GPIO_LCD_DCDC_EN         11
#define PM8921_GPIO_PANEL_RESET         25
#define PM8921_GPIO_PANEL_ID            12

#define PM8921_GPIO_BL_LED_EN_MITWOA           22
#define PM8921_GPIO_BL_LED_EN_MITWO           13
#define LM3530_I2C_ADDR                 (0x38)

static void panel_backlight_on_mitwoa(unsigned int on)
{
	int ret = 0;
	static struct qup_i2c_dev *lm3530_dev = NULL;

	if (on) {
		lm3530_dev = qup_i2c_init(GSBI_ID_10, 100000, 24000000);

		pmic8921_gpio_set(PM8921_GPIO_BL_LED_EN_MITWOA, 1);

		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x10, 0x17);
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x20, 0x00);
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x30, 0x00);
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0xA0, 0x1F);
	} else {
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x10, 0x00);
		pmic8921_gpio_set(PM8921_GPIO_BL_LED_EN_MITWOA, 0);
		lm3530_dev = NULL;
	}
	if (ret) {
		dprintf(CRITICAL, "panel_backlight_on: ret %d\n", ret);
	}
}

static void panel_backlight_on_mitwo(unsigned int on)
{
	int ret = 0;
	static struct qup_i2c_dev *lm3530_dev = NULL;

	if (on) {
		lm3530_dev = qup_i2c_init(GSBI_ID_1, 100000, 24000000);

		pmic8921_gpio_set(PM8921_GPIO_BL_LED_EN_MITWO, 1);

		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x10, 0x17);
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x20, 0x00);
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x30, 0x00);
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0xA0, 0x1F);
	} else {
		ret += i2c_write(lm3530_dev, LM3530_I2C_ADDR, 0x10, 0x00);
		pmic8921_gpio_set(PM8921_GPIO_BL_LED_EN_MITWO, 0);
		lm3530_dev = NULL;
	}
	if (ret) {
		dprintf(CRITICAL, "panel_backlight_on: ret %d\n", ret);
	}
}

int target_backlight_ctrl(struct backlight *bl, uint8_t enable)
{
	if (board_target_id() == LINUX_MACHTYPE_8064_MITWO || (board_target_id() == LINUX_MACHTYPE_8064_MTP))
		panel_backlight_on_mitwo(enable);
	else
		panel_backlight_on_mitwoa(enable);
	return 0;
}

static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
	/* regulator */
	.regulator = {0x03, 0x0a, 0x04, 0x00, 0x20},
	/* timing */
	.timing = {0xb3, 0x8c, 0x1d, 0x00, 0x20, 0x94, 0x20, 0x8e,
		0x20, 0x03, 0x04},
	/* phy ctrl */
	.ctrl = {0x5f, 0x00, 0x00, 0x10},
	/* strength */
	.strength = {0xff, 0x00, 0x06, 0x00},
	/* pll control */
	.pll = {0x0,
	0xc1, 0x01, 0x1a,
	0x00, 0x50, 0x48, 0x63,
	0x71, 0x0f, 0x03,
	0x00, 0x14, 0x03, 0x00, 0x02,
	0x00, 0x20, 0x00, 0x01, 0x00},
};

int target_panel_clock(uint8_t enable, struct msm_panel_info *pinfo)
{
	if (enable) {
		mdp_clock_init();
		mmss_clock_init();
	} else if(!target_cont_splash_screen()) {
		mmss_clock_disable();
	}

	pinfo->mipi.dsi_phy_db = &dsi_cmd_mode_phy_db;

	return 0;
}

int target_panel_reset(uint8_t enable, struct panel_reset_sequence *resetseq,
						struct msm_panel_info *pinfo)
{
	return 0;
}

int target_ldo_ctrl(uint8_t enable, struct msm_panel_info *pinfo)
{
	unsigned int lcd_id_det = 2;
	if (enable) {
		/* Set and enabale LDO2 1.2V for  VDDA_MIPI_DSI0/1_PLL */
		pm8921_ldo_set_voltage(LDO_2, LDO_VOLTAGE_1_2V);

		mi_display_gpio_init();

		/* Initial condition */
		pmic8921_gpio_set(PM8921_GPIO_PANEL_RESET, 0);
		pmic8921_gpio_set(PM8921_GPIO_LCD_DCDC_EN, 0);
		pm8921_ldo_clear_voltage(LDO_23);
		mdelay(8);

		/* Enable LVS7 */
		pm8921_low_voltage_switch_enable(lvs_7);
		pm8921_ldo_set_voltage(LDO_23, LDO_VOLTAGE_1_8V);
		mdelay(10);

		/* Enable VSP VSN */
		pmic8921_gpio_set(PM8921_GPIO_LCD_DCDC_EN, 1);
		mdelay(8);

		/* Reset */
		pmic8921_gpio_set(PM8921_GPIO_PANEL_RESET, 1);
		mdelay(3);

		lcd_id_det = pmic8921_gpio_get(PM8921_GPIO_PANEL_ID);
	} else {
		if (!target_cont_splash_screen()) {
			/* Reset down */
			pmic8921_gpio_set(PM8921_GPIO_PANEL_RESET, 0);

			/* Disable VSP VSN */
			pmic8921_gpio_set(PM8921_GPIO_LCD_DCDC_EN, 0);
			mdelay(8);

			/* Disable 1V8 */
			pm8921_ldo_clear_voltage(LDO_23);
		}
	}

	return 0;
}

bool target_display_panel_node(char *panel_name, char *pbuf, uint16_t buf_size)
{
	return gcdb_display_cmdline_arg(panel_name, pbuf, buf_size);
}

void target_display_init(const char *panel_name)
{
	uint32_t panel_loop = 0;
	uint32_t ret = 0;

	if (!strcmp(panel_name, NO_PANEL_CONFIG)) {
		dprintf(INFO, "Skip panel configuration\n");
		return;
	}

	do {
		ret = gcdb_display_init(panel_name, MDP_REV_44, MIPI_FB_ADDR);
		if (ret) {
			/*Panel signature did not match, turn off the display*/
			target_force_cont_splash_disable(true);
			msm_display_off();
			target_force_cont_splash_disable(false);
		} else {
			break;
		}
	} while (++panel_loop <= oem_panel_max_auto_detect_panels());
}

void target_display_shutdown(void)
{
	gcdb_display_shutdown();
}

