#ifndef __MSM8960_H
#define __MSM8960_H

#include <platform/mmc.h>

/* 8960 */
#define LINUX_MACHTYPE_8960_SIM     3230
#define LINUX_MACHTYPE_8960_RUMI3   3231
#define LINUX_MACHTYPE_8960_CDP     3396
#define LINUX_MACHTYPE_8960_MTP     3397
#define LINUX_MACHTYPE_8960_FLUID   3398
#define LINUX_MACHTYPE_8960_APQ     3399
#define LINUX_MACHTYPE_8960_LIQUID  3535
#define LINUX_MACHTYPE_8960_MITWOA  4459

/* 8627 */
#define LINUX_MACHTYPE_8627_CDP     3861
#define LINUX_MACHTYPE_8627_MTP     3862

/* 8930 */
#define LINUX_MACHTYPE_8930_CDP     3727
#define LINUX_MACHTYPE_8930_MTP     3728
#define LINUX_MACHTYPE_8930_FLUID   3729
#define LINUX_MACHTYPE_8930_EVT     4558

/* 8064 */
#define LINUX_MACHTYPE_8064_SIM     3572
#define LINUX_MACHTYPE_8064_RUMI3   3679
#define LINUX_MACHTYPE_8064_CDP     3948
#define LINUX_MACHTYPE_8064_MTP     3949
#define LINUX_MACHTYPE_8064_LIQUID  3951
#define LINUX_MACHTYPE_8064_MPQ_CDP 3993
#define LINUX_MACHTYPE_8064_MPQ_HRD 3994
#define LINUX_MACHTYPE_8064_MPQ_DTV 3995
#define LINUX_MACHTYPE_8064_EP      3996
#define LINUX_MACHTYPE_8064_MITWO   4180
#define LINUX_MACHTYPE_8064_MPQ_DMA 4511

void target_mmc_caps(struct mmc_host *host);
uint8_t platform_pmic_type(uint32_t pmic_type);
unsigned board_machtype(void);

#ifdef QCOM_ENABLE_UART
void platform_uart_init_auto(void);
int target_uart_gpio_config(uint8_t id);
#endif

void apq8064_keypad_gpio_init(void);
void msm8960_keypad_gpio_init(void);
void msm8930_keypad_gpio_init(void);

#endif
