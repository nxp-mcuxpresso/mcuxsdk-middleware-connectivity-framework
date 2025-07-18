/*
 * Copyright 2020-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fwk_config.h"
#include "fsl_os_abstraction.h"
#include "mflash_drv.h"
/* mflash_drv.h must be included first */
#include "fwk_platform_extflash.h"

/*******************************************************************************
 * Macros
 ******************************************************************************/

/* mflash_drv.h defines MFLASH_REMAP_ACTIVE and MFLASH_REMAP_OFFSET if remap is
 * supported by the platform */
#if !defined MFLASH_REMAP_ACTIVE
#define MFLASH_REMAP_ACTIVE() false
#endif

#if !defined MFLASH_REMAP_OFFSET
#define MFLASH_REMAP_OFFSET() 0U
#endif

/* _SECTOR_ADDR returns the address of sector containing address */
#define _SECTOR_ADDR(addr) (((addr) & ~(PLATFORM_EXTFLASH_SECTOR_SIZE - 1U)))
/* _PAGE_ADDR returns the address of page containing address */
#define _PAGE_ADDR(addr) (((addr) & ~(PLATFORM_EXTFLASH_PAGE_SIZE - 1U)))

#if !defined PLATFORM_ACCESS_ALIGNMENT_CONSTRAINT_LOG
#define PLATFORM_ACCESS_ALIGNMENT_CONSTRAINT_LOG 0u
#endif

#define PLATFORM_ACCESS_UNIT_SZ  (1u << PLATFORM_ACCESS_ALIGNMENT_CONSTRAINT_LOG)
#define PLATFORM_ACCESS_UNIT_MSK (PLATFORM_ACCESS_UNIT_SZ - 1u)

#define _SZ_MULTIPLE_OF_ACCESS_UNIT(sz)           (((sz)&PLATFORM_ACCESS_UNIT_MSK) == 0u)
#define _ROUND_UP_MULTIPLE_OF_ACCESS_UNIT(length) ((length + PLATFORM_ACCESS_UNIT_MSK) & ~PLATFORM_ACCESS_UNIT_MSK)

/* -------------------------------------------------------------------------- */
/*                               Private functions                            */
/* -------------------------------------------------------------------------- */

static bool MemCmpToEraseValue(uint8_t *ptr, uint32_t blen)
{
    bool     ret            = true;
    uint32_t remaining_blen = blen;
    do
    {
        uint8_t  *p_8             = ptr;
        uint32_t  unaligned_bytes = (uint32_t)ptr % 4;
        uint32_t *p_32;

#if (PLATFORM_ACCESS_ALIGNMENT_CONSTRAINT_LOG == 2u)
        /* need to respect constraint on direct flash access */
        unaligned_bytes = 4u - unaligned_bytes;
#endif

        /* Compare byte by byte to 0xff till alignment */
        for (uint32_t i = 0u; i < unaligned_bytes; i++)
        {
            if (*p_8++ != 0xff)
            {
                ret = false;
                break; /* for */
            }
            remaining_blen--;
        }
        if (!ret)
        {
            break; /* do .. while */
        }
        /* offset_32b is at the word alignment offset in buffer */
        p_32 = (uint32_t *)&p_8[0];
        while (remaining_blen >= sizeof(uint32_t))
        {
            /* read word by word to compare to 0xffffffff */
            if (*p_32++ != ~0UL)
            {
                /* while */
                ret = false;
                break;
            }
            remaining_blen -= sizeof(uint32_t);
        }
        if (!ret)
        {
            /* do .. while */
            break;
        }
        p_8 = (uint8_t *)p_32;
        for (uint32_t i = 0u; i < remaining_blen; i++)
        {
            /* Unaligned trailing bytes */
            if (*p_8 != 0xffu)
            {
                ret = false;
                break;
            }
        }
    } while (false);
    return ret;
}

/*******************************************************************************
 * Public functions
 ******************************************************************************/
/*
 *  Public API exists because it is used from other modules than current one.
 * Internally use MFLASH_REMAP_ACTIVE because generated code is better optimized.
 */
bool PLATFORM_IsActiveApplicationRemapped(void)
{
    return MFLASH_REMAP_ACTIVE();
}

int PLATFORM_InitExternalFlash(void)
{
    status_t    st        = kStatus_Success;
    static bool init_done = false;
    if (!init_done)
    {
        st = mflash_drv_init();
        if (st == kStatus_Success)
        {
            init_done = true;
        }
    }
    return (int)st;
}

int PLATFORM_EraseExternalFlash(uint32_t address, uint32_t size)
{
    status_t st      = kStatus_Success;
    uint32_t endAddr = address + size;
    uint32_t startBlock, endBlock;
    uint32_t index;
    if ((endAddr & (PLATFORM_EXTFLASH_SECTOR_SIZE - 1U)) != 0U)
    {
        /* If the address is in the middle of a block, round up to the next block
         * This gives the upper block limit, every blocks before this one will be erased */
        endAddr = ((endAddr / PLATFORM_EXTFLASH_SECTOR_SIZE) + 1U) * PLATFORM_EXTFLASH_SECTOR_SIZE;
    }

    startBlock = address / PLATFORM_EXTFLASH_SECTOR_SIZE;
    endBlock   = endAddr / PLATFORM_EXTFLASH_SECTOR_SIZE;
    index      = startBlock;
    for (index = startBlock; (index < endBlock); index++)
    {
        uint32_t addr;

        /* The external flash driver is MFLASH, which has no Block Erase API, so limit to sector erase use only.
         * This is less optimal and operations take around 15% longer when erasing sectors one by one */
        addr = index * PLATFORM_EXTFLASH_SECTOR_SIZE;
        /* No address conversion to physical address */
        st = mflash_drv_sector_erase(addr);
        if (st != kStatus_Success)
        {
            break;
        }
    }
    return (int)st;
}

/* \brief Read from external flash to RAM buffer
 *
 * \dest[in/out] pointer of RAM destination to copy flash contents - arbitrary alignment
 * \param[in] length number of bytes - arbitrary byte length.
 * \param[in] address offset relative to the start of the flash device - not AHB address
 * \param[in] directRead read from flash offset converting offset to AHB address
 *
 * \return int return status: 0 for success, otherwise an error was raised.
 */
int PLATFORM_ReadExternalFlash(uint8_t *dest, uint32_t length, uint32_t address, bool requestFastRead)
{
    status_t st = 0;
    (void)requestFastRead;

    if (!MFLASH_REMAP_ACTIVE())
    {
        /* If remapping is not active, can use direct read */
        memcpy(dest, (uint8_t *)EXTFLASH_PHYS_ADDR(address), length);
        st = 0;
    }
    else
#if (PLATFORM_ACCESS_ALIGNMENT_CONSTRAINT_LOG == 2u)
    /* Need to check source address alignment on platforms requiring it */
    {
        /* temporary buffer size has to be multiple of 4! */
        uint32_t tmp_buf[64 / sizeof(uint32_t)];
        uint32_t chunkAlign_cnt = 0;
        uint32_t offset         = address;
        uint8_t *dst_ptr        = dest;

        /* Correction for unaligned start address offset*/
        if (length > 0u && (address % 4u != 0u))
        {
            uint32_t cor         = address % 4u;
            uint32_t restTo4     = 4u - cor;
            uint32_t segment_len = (length >= 4u ? restTo4 : length);

            /* read whole 4B and copy desired segment to destination*/
            st = mflash_drv_read(offset - cor, tmp_buf, 4u);

            if (st != kStatus_Success)
            {
                return (int)st;
            }

            memcpy(dst_ptr, (uint8_t *)tmp_buf + cor, segment_len);

            /* correct offset address, destination pointer and remaining data size */
            offset += restTo4;
            dst_ptr += restTo4;
            if (restTo4 > length)
            {
                length = 0u;
            }
            else
            {
                length -= restTo4;
            }
        }

        while (length > 0u)
        {
            size_t chunk = (length > sizeof(tmp_buf)) ? sizeof(tmp_buf) : length;
            /* mflash demands size to be in multiples of 4 */
            size_t chunkAlign4 = (chunk + 3u) & (~3u);

            /* directly copy into destination if address and data size is aligned */
            if ((uint32_t)dst_ptr % 4u == 0u && chunk % 4u == 0u)
            {
                st = mflash_drv_read(offset, (uint32_t *)(dst_ptr + chunkAlign_cnt * sizeof(tmp_buf)), chunkAlign4);
            }
            else
            {
                st = mflash_drv_read(offset, tmp_buf, chunkAlign4);
                memcpy(dst_ptr + chunkAlign_cnt * sizeof(tmp_buf), (uint8_t *)tmp_buf, chunk);
            }

            if (st != kStatus_Success)
            {
                return (int)st;
            }

            length -= chunk;
            offset += chunk;
            chunkAlign_cnt++;
        }
    }
#else
        do
        {
            uint8_t *p_dst;
            uint32_t read_sz;
            uint8_t  page_buf[PLATFORM_EXTFLASH_PAGE_SIZE + 4u];
            if ((uint32_t)dest % sizeof(uint32_t) == 0u)
            {
                read_sz = length & ~(sizeof(uint32_t) - 1);
                p_dst   = dest;
                st      = mflash_drv_read(address, (uint32_t *)p_dst, read_sz);
                if (kStatus_Success != st)
                {
                    st = -1;
                    break;
                }
                length -= read_sz;
                p_dst += read_sz; /* increased by read_sz that is necessarily a multiple of 4 */
                address += read_sz;
                if (length > 0u)
                {
                    /* we cannot know whether the caller has left space for extra bytes to allow
                     * rounding size to next multiple of 4, so copy to page_buf and then memcpy the
                     * trailing bytes to the destination buffer */
                    read_sz = (length + 3u) >> 2u << 2u;
                    st      = mflash_drv_read(address, (uint32_t *)&page_buf[0], read_sz);
                    if (kStatus_Success != st)
                    {
                        st = -1;
                        break;
                    }
                    memcpy(p_dst, &page_buf[0], length);
                }
                st = 0;
            }
            else
            {
                uint32_t remaining_sz = length;

                while (remaining_sz > PLATFORM_EXTFLASH_PAGE_SIZE)
                {
                    read_sz = PLATFORM_EXTFLASH_PAGE_SIZE;
                    st      = mflash_drv_read(address, (uint32_t *)&page_buf[0], read_sz);
                    if (kStatus_Success != st)
                    {
                        st = -1;
                        break;
                    }
                    remaining_sz -= read_sz;
                    address += read_sz;
                    memcpy(dest, &page_buf[0], read_sz);
                    dest += read_sz;
                }
                if (st < 0)
                {
                    break;
                }
                if (remaining_sz > 0)
                {
                    read_sz = (remaining_sz + 3u) >> 2u << 2u;
                    st      = mflash_drv_read(address, (uint32_t *)&page_buf[0], read_sz);
                    if (kStatus_Success != st)
                    {
                        st = -1;
                        break;
                    }
                    memcpy(dest, &page_buf[0], remaining_sz);
                }
            }
            st = 0;
        } while (false);
#endif

    return (int)st;
}

/*
 * If address is not aligned, previous content may have been written to flash already.
 * Copy contents of partial page in RAM, add the new values at right offset in page buffer (256 bytes), then program
 * page. From that point on the remainder, if any, is page aligned. if any.
 */
int PLATFORM_WriteExternalFlash(uint8_t *data, uint32_t length, uint32_t address)
{
    status_t status;

    do
    {
        uint8_t  page_buf[PLATFORM_EXTFLASH_PAGE_SIZE];
        uint32_t page_addr;
        uint8_t *src;
        uint32_t write_len;
        uint32_t remaining_sz;

        src = (uint8_t *)data;

        remaining_sz = length;

        page_addr = address & ~(PLATFORM_EXTFLASH_PAGE_SIZE - 1U);
        if (page_addr != address)
        {
            uint32_t write_offset;
            /* copy partial contents of page - if any to RAM */
            /* The buffer receiving the contents of flash in RAM has alignment constraints dur to mflash / SPI
             programming. page_buf matches this condition */
            status = mflash_drv_read(page_addr, (uint32_t *)page_buf, PLATFORM_EXTFLASH_PAGE_SIZE);
            if (kStatus_Success != status)
            {
                break;
            }
            write_offset = (address - page_addr);
            write_len    = PLATFORM_EXTFLASH_PAGE_SIZE - write_offset;
            if (write_len > length)
            {
                write_len = length;
            }
            memcpy(&page_buf[write_offset], src, write_len);
            status = mflash_drv_page_program(page_addr, (uint32_t *)&page_buf[0]);
            if (kStatus_Success != status)
            {
                break;
            }
            src += write_len;
            page_addr += PLATFORM_EXTFLASH_PAGE_SIZE;
            remaining_sz -= write_len;
        }
        /* From now the address in flash is page aligned */

        while (remaining_sz >= PLATFORM_EXTFLASH_PAGE_SIZE)
        {
            memcpy(page_buf, src, PLATFORM_EXTFLASH_PAGE_SIZE);
            status = mflash_drv_page_program(page_addr, (uint32_t *)&page_buf[0]);
            if (status != kStatus_Success)
            {
                break;
            }
            remaining_sz -= PLATFORM_EXTFLASH_PAGE_SIZE;
            src += PLATFORM_EXTFLASH_PAGE_SIZE;
            page_addr += PLATFORM_EXTFLASH_PAGE_SIZE;
        }
        /* If a partial page has to be written, pad remainder with 0xff */
        if (remaining_sz > 0u)
        {
            /* partial page left*/
            memcpy(page_buf, src, remaining_sz);
            memset(&page_buf[remaining_sz], 0xffu, PLATFORM_EXTFLASH_PAGE_SIZE - remaining_sz);
            status = mflash_drv_page_program(page_addr, (uint32_t *)page_buf);
            if (status != kStatus_Success)
            {
                break;
            }
        }
        status = kStatus_Success;
    } while (0 != 0);

    return (int)status;
}

int PLATFORM_IsExternalFlashBusy(bool *isBusy)
{
    /* return false as program and erase flash operations are blocking, so it cannot be busy */
    /* no need to attempt any data read */
    *isBusy = 0U;

    return 0;
}

bool PLATFORM_ExternalFlashAreaIsBlank(uint32_t address, uint32_t len)
{
    bool ret = false;

    if (!MFLASH_REMAP_ACTIVE())
    {
        /*
         * Direct access to flash contents requires conversion from offset in flash to AHB address.
         */
        ret = MemCmpToEraseValue((uint8_t *)EXTFLASH_PHYS_ADDR(address), len);
    }
    else
    {
        /* must read block by block, we chose block to be the size of a page, but could be less */
        uint8_t  page_buf[PLATFORM_EXTFLASH_PAGE_SIZE];
        uint32_t remaining_len = len;
        while (remaining_len > 0u)
        {
            uint32_t read_sz = MIN(PLATFORM_EXTFLASH_PAGE_SIZE, remaining_len);
            if (0 == PLATFORM_ReadExternalFlash(&page_buf[0], read_sz, address, false))
            {
                if (!MemCmpToEraseValue(&page_buf[0], read_sz))
                {
                    break;
                }
            }
            else
            {
                assert(0);
                break;
            }
            remaining_len -= read_sz;
        }
        ret = (remaining_len == 0u);
    }
    return ret;
}

bool PLATFORM_IsExternalFlashPageBlank(uint32_t address)
{
    /* Start from address of page containing argument address */
    uint32_t page_addr = _PAGE_ADDR(address);
    return PLATFORM_ExternalFlashAreaIsBlank(page_addr, PLATFORM_EXTFLASH_PAGE_SIZE);
}

bool PLATFORM_IsExternalFlashSectorBlank(uint32_t address)
{
    /* Start from address of sector containing argument address */
    uint32_t sect_addr = _SECTOR_ADDR(address);
    return PLATFORM_ExternalFlashAreaIsBlank(sect_addr, PLATFORM_EXTFLASH_SECTOR_SIZE);
}
