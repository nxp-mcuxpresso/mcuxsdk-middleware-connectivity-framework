/*
 *  Copyright (c) 2021-2023, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   TODO: Add linker instructions for RT1170
 */


OCRAM1_START                 = 0x20240000;
OCRAM1_SIZE                  = 0x00080000; /* 512kB */
OCRAM2_START                 = 0x202C0000;
OCRAM2_SIZE                  = 0x00080000; /* 512kB */
OCRAM_M7_ECC_START           = 0x20360000;
OCRAM2_SIZE                  = 0x00020000; /* 128B */
OCRAM_M7_START               = 0x20380000;
OCRAM_M7_SIZE                = 0x00080000; /* 512kB */
DTCM_START                   = 0x20000000;
DTCM_SIZE                    = 0x00080000; /* 512kB */
CAAM_SEC_RAM_START           = 0x00280000;
CAAM_SEC_RAM_SIZE            = 0x00010000; /* 64kB */

ITCM_START                   = 0x00000000;
ITCM_SIZE                    = 0x00080000; /* 512kB */
__NCACHE_REGION_START        = OCRAM2_START;
__NCACHE_REGION_SIZE         = 0x2000;


ENTRY(Reset_Handler)

/* Entry Point */

m_flash_start                  = 0x30000000;
m_flash_size                   = 0x01000000; /* 16 MB */
m_flash_end                    = m_flash_start + m_flash_size - 1;
m_flash_sector_size            = 0x1000;  /* 4k flash sector size */

m_boot_flash_conf_sector_start      = m_flash_start;
m_boot_flash_conf_offset            = 0x400;
m_boot_flash_conf_start             = m_boot_flash_conf_sector_start + m_boot_flash_conf_offset;
m_boot_flash_conf_sector_end        = m_boot_flash_conf_sector_start + m_flash_sector_size - 1;
m_boot_flash_conf_size              = m_flash_sector_size - m_boot_flash_conf_offset;


m_ivt_start                         = m_boot_flash_conf_sector_end + 1;
m_ivt_size                          = m_flash_sector_size;
m_ivt_end                           = m_ivt_start + m_ivt_size - 1;

/* Note that MCU boot itself embeds the FlashConfig section */
m_mcuboot_size                      = 0x00040000;       /* 256 kB huge, could probably be less */
m_mcuboot_start                     = m_flash_start; /* The flash config section is comprised */
m_mcuboot_end                       = m_mcuboot_start + m_mcuboot_size - 1;
m_mcuboot_img_hdr_sz                = ((0x400 + (m_flash_sector_size -1 )) / m_flash_sector_size) * m_flash_sector_size;

m_app_start                         = m_flash_start + m_mcuboot_size;
m_app_stated_size                   =  (0x00200000 - m_flash_sector_size); /* image reserved space 2MB */
m_padding_must_have                 = m_flash_sector_size; /* at least one sector between the 2 FW slots */
m_fw_update_start                   = m_app_start + m_app_stated_size + m_padding_must_have;

FW_UPDATE_STORAGE_START             = m_fw_update_start;
FW_UPDATE_STORAGE_OFFSET            = m_fw_update_start - m_flash_start;
FW_UPDATE_STORAGE_SIZE              = m_app_stated_size;
FW_ACTIVE_APP_START                 = m_app_start;
FW_ACTIVE_APP_OFFSET                = m_app_start  - m_flash_start;
FW_ACTIVE_APP_SIZE                  = m_app_stated_size;
fw_top                              = m_fw_update_start + m_app_stated_size;
__FACTORY_DATA_START                = fw_top;
__FACTORY_DATA_START_OFFSET         = __FACTORY_DATA_START - m_flash_start;
__FACTORY_DATA_SIZE                 = DEFINED(gUseFactoryData_d) ? m_flash_sector_size : 0;
__FACTORY_DATA_END                  = __FACTORY_DATA_START + __FACTORY_DATA_SIZE - 1;

PROD_DATA_BASE_ADDR                 = __FACTORY_DATA_END + 1;
PROD_DATA_SIZE                      = m_flash_sector_size;
PROD_DATA_END                       = PROD_DATA_BASE_ADDR + m_flash_sector_size - 1;

NV_STORAGE_START_ADDRESS            = PROD_DATA_END + 1;
NV_STORAGE_START_ADDRESS_OFFSET     = NV_STORAGE_START_ADDRESS - m_flash_start;
NV_STORAGE_MAX_SECTORS              = DEFINED(gNVMSectorCountLink_d) ? gNVMSectorCountLink_d : 16;
NV_STORAGE_SECTOR_SIZE              = m_flash_sector_size;
m_nvm_storage_size                  = NV_STORAGE_MAX_SECTORS * NV_STORAGE_SECTOR_SIZE;
NV_STORAGE_END_ADDRESS              = NV_STORAGE_START_ADDRESS + m_nvm_storage_size - 1;

LITTLEFS_STORAGE_START_ADDRESS       = NV_STORAGE_END_ADDRESS + 1;
LITTLEFS_STORAGE_START_ADDRESS_OFFSET = LITTLEFS_STORAGE_START_ADDRESS - m_flash_start;
LITTLEFS_STORAGE_END_ADDRESS         = m_flash_end;
LITTLEFS_STORAGE_SECTOR_SIZE         = m_flash_sector_size;
LITTLEFS_STORAGE_MAX_SECTORS         = (LITTLEFS_STORAGE_END_ADDRESS - LITTLEFS_STORAGE_START_ADDRESS) / LITTLEFS_STORAGE_SECTOR_SIZE;

m_interrupts_start                  = m_app_start + m_mcuboot_img_hdr_sz;
m_interrupts_size                   = 0x00000400;
m_interrupts_end                    = m_interrupts_start + m_interrupts_size -1;
m_text_start                        = m_interrupts_end + 1;
m_text_size                         = m_app_stated_size - m_interrupts_size;
m_text_end                          = m_text_start + m_text_size -1;



HEAP_SIZE  = DEFINED(__heap_size__)  ? __heap_size__  : 0x0800;
STACK_SIZE = DEFINED(__stack_size__) ? __stack_size__ : 0x0800;
RPMSG_SHMEM_SIZE = DEFINED(__use_shmem__) ? 0x2000 : 0;
VECTOR_RAM_SIZE = DEFINED(__ram_vector_table__) ? 0x00000400 : 0;

/* Specify the memory areas */
MEMORY
{
  m_flash_config        (RX)  : ORIGIN = m_boot_flash_conf_start, LENGTH = m_boot_flash_conf_size
  m_ivt                 (RX)  : ORIGIN = m_ivt_start, LENGTH = m_ivt_size
  m_interrupts          (RX)  : ORIGIN = m_interrupts_start, LENGTH = m_interrupts_size
  m_text                (RX)  : ORIGIN = m_text_start, LENGTH = m_text_size
  m_factory_data        (RX)  : ORIGIN = __FACTORY_DATA_START, LENGTH = __FACTORY_DATA_SIZE
  m_data                (RW)  : ORIGIN = OCRAM1_START, LENGTH = 0x00080000
  NVM_region            (RW)  : ORIGIN = NV_STORAGE_START_ADDRESS, LENGTH = m_nvm_storage_size
  m_qacode              (RX)  : ORIGIN = ITCM_START, LENGTH = ITCM_SIZE /* Quick Access code linked in Instruction Tighly Couple Memory */
  rpmsg_sh_mem          (RW)  : ORIGIN = 0x20300000, LENGTH = RPMSG_SHMEM_SIZE
  m_data_noncache       (RW)  : ORIGIN = __NCACHE_REGION_START, LENGTH = __NCACHE_REGION_SIZE
}

/* Define output sections */
SECTIONS
{
  .flash_config :
  {
    . = ALIGN(4);
    __FLASH_BASE = .;

    KEEP(* (.boot_hdr.conf))     /* flash config section */
    . = ALIGN(4);
  } > m_flash_config

  ivt_begin= ORIGIN(m_flash_config) + LENGTH(m_flash_config);

  .ivt : AT(ivt_begin)
  {
    . = ALIGN(4);
    KEEP(* (.boot_hdr.ivt))           /* ivt section */
    KEEP(* (.boot_hdr.boot_data))     /* boot section */
    KEEP(* (.boot_hdr.dcd_data))      /* dcd section */
    . = ALIGN(4);
  } > m_ivt

 
  /* The startup code goes first into internal RAM */
  .interrupts :
  {
    __VECTOR_TABLE = .;
    __Vectors = .;
    . = ALIGN(4);
    KEEP(*(.isr_vector))     /* Startup code */
    . = ALIGN(4);
  } > m_interrupts

  /* The program code and other data goes into internal RAM */
  .text :
  {
    . = ALIGN(4);
    *(EXCLUDE_FILE(
        /* Exclude flash and frequently executed functions from XIP */
        *fsl_adapter_flexspi_nor_flash.c.obj
        *mflash_drv.c.obj
        *fsl_flexspi.c.obj
    ) .text)                 /* .text sections (code) */
    *(.rodata)               /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)              /* .rodata* sections (constants, strings, etc.) */
    *(.glue_7)               /* glue arm to thumb code */
    *(.glue_7t)              /* glue thumb to arm code */
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
  } > m_text

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > m_text

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > m_text

 .ctors :
  {
    __CTOR_LIST__ = .;
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    KEEP (*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    __CTOR_END__ = .;
  } > m_text

  .dtors :
  {
    __DTOR_LIST__ = .;
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    __DTOR_END__ = .;
  } > m_text

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } > m_text

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > m_text

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > m_text

  __etext = .;    /* define a global symbol at end of code */
  __DATA_ROM = .; /* Symbol is used by startup for data initialization */

  .interrupts_ram :
  {
    . = ALIGN(4);
    __VECTOR_RAM__ = .;
    __interrupts_ram_start__ = .; /* Create a global symbol at data start */
    *(.m_interrupts_ram)     /* This is a user defined section */
    . += VECTOR_RAM_SIZE;
    . = ALIGN(4);
    __interrupts_ram_end__ = .; /* Define a global symbol at data end */
  } > m_data

  __VECTOR_RAM = DEFINED(__ram_vector_table__) ? __VECTOR_RAM__ : ORIGIN(m_interrupts);
  __RAM_VECTOR_TABLE_SIZE_BYTES = DEFINED(__ram_vector_table__) ? (__interrupts_ram_end__ - __interrupts_ram_start__) : 0x0;

  .data : AT(__DATA_ROM)
  {
    . = ALIGN(4);
    __DATA_RAM = .;
    __data_start__ = .;      /* create a global symbol at data start */
    *(m_usb_dma_init_data)
    *(.data)                 /* .data sections */
    *(.data*)                /* .data* sections */
    KEEP(*(.jcr*))
    . = ALIGN(4);
    PROVIDE(__start_NVM_TABLE = .);
    KEEP(*(.NVM_TABLE));
    PROVIDE(__stop_NVM_TABLE = .);
    . = ALIGN(4);
    __data_end__ = .;        /* define a global symbol at data end */
  } > m_data
  
  __ram_function_flash_start = __DATA_ROM + (__data_end__ - __data_start__); /* Symbol is used by startup for TCM data initialization */

  .ram_function : AT(__ram_function_flash_start)
  {
    . = ALIGN(32);
    __ram_function_start__ = .;
    *(CodeQuickAccess)
    . = ALIGN(128);
    /* Explicit placement of flash and frequently executed functions in RAM */
    *fsl_adapter_flexspi_nor_flash.c.obj(.text*)
    *mflash_drv.c.obj(.text*)
    *fsl_flexspi.c.obj(.text*)
  
    __ram_function_end__ = .;
  } > m_qacode

  __DATA_END = __ram_function_flash_start + (__ram_function_end__ - __ram_function_start__);

  text_end = ORIGIN(m_text) + LENGTH(m_text);
  flash_usage = __DATA_END - m_flash_start;
  ASSERT(__DATA_END <= text_end, "region m_text overflowed with text and data")

  /* Uninitialized data section */
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    . = ALIGN(4);
    __START_BSS = .;
    __bss_start__ = .;
    *(m_usb_dma_noninit_data)
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
    __END_BSS = .;
  } > m_data

  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE(end = .);
    __HeapBase = .;
    . += HEAP_SIZE;
    __HeapLimit = .;
    __heap_limit = .; /* Add for _sbrk */
  } > m_data

  .stack :
  {
    . = ALIGN(8);
    __StackLimit = .;
    . += STACK_SIZE;
    __StackTop = .;
  } > m_data

  PROVIDE(__stack = __StackTop);

  /* NOINIT section for rpmsg_sh_mem */
  .noinit_rpmsg_sh_mem (NOLOAD) : ALIGN(4)
  {
     __RPMSG_SH_MEM_START__ = .;
     *(.noinit.$rpmsg_sh_mem*)
     . = ALIGN(4) ;
     __RPMSG_SH_MEM_END__ = .;
  } > rpmsg_sh_mem

  .ncache.init : AT(ORIGIN(m_data_noncache))
  {
    __noncachedata_start__ = .;   /* create a global symbol at ncache data start */
    *(NonCacheable.init)
    . = ALIGN(4);
    __noncachedata_init_end__ = .;   /* create a global symbol at initialized ncache data end */
  } > m_data_noncache
  . = __noncachedata_init_end__;
  .ncache :
  {
    *(NonCacheable)
    . = ALIGN(4);
    __noncachedata_end__ = .;     /* define a global symbol at ncache data end */
  } > m_data_noncache

  /* Remove this section to keep the nvm section on writting the device */
  .NVM :
  {
    FILL(0xFFFFFFFF);
    . = ORIGIN(NVM_region) + LENGTH(NVM_region) - 1;
    BYTE(0xFF)
  } > NVM_region
  .ARM.attributes 0 : { *(.ARM.attributes) }

  ASSERT(__StackLimit >= __HeapLimit, "region m_data2 overflowed with stack and heap")
}