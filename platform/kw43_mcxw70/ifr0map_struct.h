
/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IFR0MAP_STRUCT_H_
#define IFR0MAP_STRUCT_H_

#include <stdint.h>

#define CMPA_CUSTOMER_DEFINED_AREA_SZ 512U /* Size in bytes : could be up to 0x1400 */
/*******************************************************************************
 * UPDATE Zone Structure (0x00000000 - 0x0000000F)
 ******************************************************************************/
#define DEVMGR_UPD_OFFSET 0x00000000UL

typedef struct _update_zone
{
    uint32_t upd_type;     /* 0x00: Device configuration update request type */
    uint32_t upd_param[3]; /* 0x04 - 0x0f: Update parameters */
} update_zone_t;

/*******************************************************************************
 * CFPA Zone Structure (0x00000010 - 0x000001FF)
 ******************************************************************************/
#define DEVMGR_CFPA_OFFSET 0x00000010U
typedef struct _cfpa_zone
{
    /* Header and Version (0x10 - 0x17) */
    uint32_t header;       /* 0x10: LC_STATE[7:0], INV_LC_STATE[15:8], HEADER[31:16] */
    uint32_t page_version; /* 0x14: CFPA page version */

    /* Key Revocation (0x18 - 0x1F) */
    uint32_t image_key_revoke; /* 0x18: Image signing key revocation counter */
    uint32_t dbg_revoke_vu;    /* 0x1C: Debug certificate revocation counter */

    /* Firmware Versions (0x20 - 0x3F) */
    /* eex_fw_version are monotonic rollback counters */
    uint32_t ee0_fw_version;       /* 0x20: Secure Firmware version EE0 - Rollback counter for Execution Environment 0
                                      firmware - CPU0 primary image*/
    uint32_t ee1_fw_version;       /* 0x24: Secure Firmware version EE1 - Rollback counter for Execution Environment 1
                                      firmware - CPU0 NS image */
    uint32_t ee2_fw_version;       /* 0x28: Secure Firmware version EE2 - Rollback counter for Execution Environment 2
                                      firmware - CPU1  secure image */
    uint32_t ee3_fw_version;       /* 0x2C: Secure Firmware version EE3 - Rollback counter for Execution Environment 3
                                      firmware - CPU1 NS image low-power wake image*/
    uint32_t fmc_sbl_fw_version;   /* 0x30: FMC_SBL firmware version */
    uint32_t recovery_sb3_version; /* 0x34: Rollback counter for SB3 file used for recovery */
    uint32_t update_sb3_version;   /* 0x38: Rollback counter for SB3 file used for FW update */
    uint32_t lp_fw_version;        /* 0x3C: Rollback counter for low-power wake image version (Monotonic counter)  */

    /* RoT Key Revocation (0x40 - 0x4F) */
    uint32_t rotk_revoke;    /* 0x40: RoT key enable/revoke status */
    uint32_t reserved_44[3]; /* 0x44 - 0x4F: Reserved */

    /* Error Counters (0x50 - 0x7F) */
    uint32_t err_auth_fail_count; /* 0x50: Authentication failure counter */
    uint32_t err_itrc_count;      /* 0x54: Tamper event counter */
    uint32_t reserved_58[10];     /* 0x58 - 0x7F: Reserved */

    /* Monotonic Counters (0x80 - 0x9F) */
    uint32_t mctr_cust_ctr[8]; /* 0x80 - 0x9f: Customer monotonic counter 0 .. 7 */

    /* Monotonic Flags (0xA0 - 0xBF) */
    uint32_t mflag_cust[8]; /* 0xA0 - 0xBF : Customer monotonic flags 0..7 : (once a bit is set the newer version of the
                               page should have the flag set) */

    /* Flash Access Control Lists (0xC0 - 0xDF) */
    uint32_t flash_acl_0_7;   /* 0xC0: Flash ACL sectors 0-7 */
    uint32_t flash_acl_8_15;  /* 0xC4: Flash ACL sectors 8-15 */
    uint32_t flash_acl_16_23; /* 0xC8: Flash ACL sectors 16-23 */
    uint32_t flash_acl_24_31; /* 0xCC: Flash ACL sectors 24-31 */
    uint32_t flash_acl_32_39; /* 0xD0: Flash ACL sectors 32-39 */
    uint32_t flash_acl_40_47; /* 0xD4: Flash ACL sectors 40-47 */
    uint32_t flash_acl_48_55; /* 0xD8: Flash ACL sectors 48-55 */
    uint32_t flash_acl_56_63; /* 0xDC: Flash ACL sectors 56-63 */

    /* Reserved (0xE0 - 0xFF) */
    uint32_t reserved_e0[8]; /* 0xE0 - 0xFF: Reserved */

    /* Image CMAC Cache (0x100 - 0x13F) */
    uint32_t img_cmac_cache[4];        /* 0x100 - 0x10F: Image CMAC cache (128 bits) */
    uint32_t nbu_img_cmac_cache[4];    /* 0x110 - 0x11F: NBU Image CMAC cache (128 bits) */
    uint32_t lp_img_crc_cache[4];      /* 0x120 - 0x12F: LP Image CRC cache (128 bits) */
    uint32_t lp_nbu_img_crc_cache[4];  /* 0x130 - 0x13F: LP NBU Image CRC cache (128 bits) */
    uint32_t reserved_140[4];          /* 0x140 - 0x14F: Reserved */

    uint8_t cust_ble_mac_addr[6];      /* 0x150 - 0x155: Customer BLE MAC address */
    uint8_t reserved_156[2];           /* 0x156 - 0x157: Reserved */
    uint8_t cust_802_15_4_mac_addr[8]; /* 0x158 - 0x15F: Customer IEEE.802.15.4 address */

    uint8_t xtal_32kHz_cfg;            /* 0x160: XTAL 32kHz trim */
    uint8_t reserved_161[3];           /* 0x161 - 0x163: Reserved */
    uint8_t xtal_32MHz_cfg;            /* 0x164: XTAL 32MHz Capacitance */
    uint8_t reserved_165[11];          /* 0x165 - 0x16F: Reserved */

    /*
     * FW update image addresses (0x170 - 0x17F).
     *
     * Silicon revision compatibility:
     *   - KW43 A.0.1 and later: these addresses live in CFPA at offset 0x170.
     *   - KW43 A.0.0 (legacy, KW43_A0_0 != 0): they live in CMPA at offset
     *     0x320, so the CFPA slots are reserved here and the fields are placed
     *     in cmpa_zone_t instead.
     */
#if defined(KW43_A0_0) && (KW43_A0_0 != 0)
    uint32_t reserved_170[4]; /* 0x170 - 0x17F: Reserved (addresses live in CMPA on A.0.0) */
#else
    uint32_t nbu_recovery_img_addr;  /* 0x170: NBU recovery image address */
    uint32_t nbu_update_img_addr;    /* 0x174: NBU update image address */
    uint32_t cpu0_recovery_img_addr; /* 0x178: CPU0 recovery image address */
    uint32_t cpu0_update_img_addr;   /* 0x17c: CPU0 update image address */
#endif

} cfpa_zone_t;

/*******************************************************************************
 * CMPA Zone Structure (0x00000200 - 0x00000FFF)
 ******************************************************************************/
#define DEVMGR_CMPA_OFFSET 0x200U
typedef struct _cmpa_zone
{
    /* Boot Configuration (0x200 - 0x20F) */
    uint32_t boot_cfg;        /* 0x200: Boot configuration and CMPA header */
    uint32_t flash_cfg;       /* 0x204: Flash and ISP configuration */
    uint32_t boot_led_status; /* 0x208: Boot LED status indicators */
    uint32_t boot_timers;     /* 0x20C: Boot timers (powerdown, watchdog) */

    /* QSPI Flash Configuration (0x210 - 0x217) */
    uint32_t lspi_qflash_cfg0; /* 0x210: QSPI flash configuration 0 */
    uint32_t lspi_qflash_cfg1; /* 0x214: QSPI flash configuration 1 */

    /* LSPI Flash Configuration (0x218 - 0x21F) */
    uint32_t lspi_flash_cfg0; /* 0x218: LSPI flash configuration 0 */
    uint32_t lspi_flash_cfg1; /* 0x21C: LSPI flash configuration 1 */

    /* ISP Interface Configurations (0x220 - 0x23F) */
    uint32_t isp_uart_cfg; /* 0x220: ISP UART configuration */
    uint32_t isp_i2c_cfg;  /* 0x224: ISP I2C configuration */
    uint32_t isp_can_cfg;  /* 0x228: ISP CAN configuration */
    uint32_t isp_spi_cfg0; /* 0x22C: ISP SPI configuration 0 */
    uint32_t isp_spi_cfg1; /* 0x230: ISP SPI configuration 1 */
    uint32_t isp_usb_id;   /* 0x234: ISP USB ID */
    uint32_t isp_usb_cfg;  /* 0x238: ISP USB configuration */
    uint32_t isp_misc_cfg; /* 0x23C: ISP miscellaneous configuration */

    /* Security Configuration (0x240 - 0x25F) */
    uint32_t cc_socu_pin;     /* 0x240: CC SOCU pin configuration */
    uint32_t cc_socu_dflt;    /* 0x244: CC SOCU default configuration */
    uint32_t vendor_usage;    /* 0x248: Vendor usage field */
    uint32_t reserved_24c;    /* 0x24C: Reserved */
    uint32_t secure_boot_cfg; /* 0x250: Secure boot configuration */
    uint32_t rotk_usage;      /* 0x254: RoT key usage properties */
    uint32_t reserved_258;    /* 0x258: Reserved */
    uint32_t err_log_addr;    /* 0x25C: Error log address */

    /* RoT Key Hashes (0x260 - 0x28F) */
    uint32_t rotkh[12]; /* 0x260 - 0x28F: RoT key hash (384 bits) */

    /* Customer Master/Secure Key Blob (0x290 - 0x2BF) */
    uint32_t cust_mk_sk_key_blob[12]; /* 0x290 - 0x2BF: Customer MK/SK key blob (384 bits) */

    /* PQC RoT Key Hash (0x2C0 - 0x2EF) */
    uint32_t pqc_rotkh[12]; /* 0x2C0 - 0x2EF: PQC RoT key hash (384 bits) */

    /* Quick GPIO Configuration (0x2F0 - 0x31F) */
    uint32_t quick_set_gpio_0; /* 0x2F0: Quick set GPIO port 0 */
    uint32_t quick_clr_gpio_0; /* 0x2F4: Quick clear GPIO port 0 */
    uint32_t quick_set_gpio_1; /* 0x2F8: Quick set GPIO port 1 */
    uint32_t quick_clr_gpio_1; /* 0x2FC: Quick clear GPIO port 1 */
    uint32_t quick_set_gpio_2; /* 0x300: Quick set GPIO port 2 */
    uint32_t quick_clr_gpio_2; /* 0x304: Quick clear GPIO port 2 */
    uint32_t quick_set_gpio_3; /* 0x308: Quick set GPIO port 3 */
    uint32_t quick_clr_gpio_3; /* 0x30C: Quick clear GPIO port 3 */
    uint32_t quick_set_gpio_4; /* 0x310: Quick set GPIO port 4 */
    uint32_t quick_clr_gpio_4; /* 0x314: Quick clear GPIO port 4 */
    uint32_t quick_set_gpio_5; /* 0x318: Quick set GPIO port 5 */
    uint32_t quick_clr_gpio_5; /* 0x31C: Quick clear GPIO port 5 */

    /*
     * FW update image addresses (legacy KW43 A.0.0 layout, 0x320 - 0x32F).
     *
     * On A.0.0 the update image addresses live in CMPA at offset 0x320. From
     * A.0.1 onward they were moved to CFPA at offset 0x170 (see cfpa_zone_t),
     * so these CMPA fields exist only for the legacy build (KW43_A0_0 != 0).
     */
#if defined(KW43_A0_0) && (KW43_A0_0 != 0)
    uint32_t nbu_recovery_img_addr;  /* 0x320: NBU recovery image address */
    uint32_t nbu_update_img_addr;    /* 0x324: NBU update image address */
    uint32_t cpu0_recovery_img_addr; /* 0x328: CPU0 recovery image address */
    uint32_t cpu0_update_img_addr;   /* 0x32C: CPU0 update image address */
#endif

} cmpa_zone_t;

#define DEVMGR_CMPA_CUST_DEFINED_OFFSET 0x400U
typedef struct _cmpa_cust_defined_zone
{
    /* Customer Defined Area (0x400 - 0x17FF) */
    uint32_t customer_defined[CMPA_CUSTOMER_DEFINED_AREA_SZ / 4]; /* 0x400 - 0x5FF: Customer defined area */
} cmpa_cust_defined_zone_t;

#endif /* IFR0MAP_STRUCT_H_ */
