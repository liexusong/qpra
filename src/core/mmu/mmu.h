/*
 * core/mmu/mmu.h -- Emulator MMU functions (header).
 *
 * Defines the structures used to represent the Memory Management Unit, and
 * related things such as symbols for address space segments, as well as
 * declaring the memory pointers for most parts of it.
 * Declares all the MMU functions.
 *
 */

#ifndef QPRA_CORE_MMU_H
#define QPRA_CORE_MMU_H

#include <stdint.h>

/* Segments of the address space which we handle. */
static const uint16_t A_ROM_FIXED = 0x0000;
static const uint16_t A_ROM_SWAP  = 0x4000;
static const uint16_t A_RAM_FIXED = 0x8000;
static const uint16_t A_RAM_SWAP  = 0xa000;
static const uint16_t A_TILE_SWAP = 0xc000;
static const uint16_t A_TILE_SWAP_END = 0xe000;
static const uint16_t A_VPU_START = 0xe000;
static const uint16_t A_VPU_END   = 0xec00;
static const uint16_t A_APU_START = 0xec00;
static const uint16_t A_APU_END   = 0xf000;
static const uint16_t A_DPCM_SWAP = 0xf000;
static const uint16_t A_DPCM_SWAP_END = 0xf800;
static const uint16_t A_CART_FIXED = 0xfe00;
static const uint16_t A_CART_FIXED_END = 0xff00;
static const uint16_t A_ROM_BANK_SELECT = 0xffe0;
static const uint16_t A_RAM_BANK_SELECT = 0xffe1;
static const uint16_t A_HIRES_CTR = 0xffe2;
static const uint16_t A_HIRES_CTR_END = 0xffe6;
static const uint16_t A_PAD1_REG = 0xfff0;
static const uint16_t A_PAD2_REG = 0xfff2;
static const uint16_t A_SERIAL_REG = 0xfff4;
static const uint16_t A_INT_VEC = 0xfff8;

/* Memory bank names, for the core_mmu_bank_select function. */ 
enum core_mmu_bank 
{
    B_ROM_FIXED, B_ROM_SWAP, B_RAM_FIXED, B_RAM_SWAP, B_TILE_SWAP, B_DPCM_SWAP
};

/* Metadata about the memory layout of a particular cartridge. */
struct core_mmu_params
{
    uint8_t rom_banks;
    uint8_t ram_banks;
    uint8_t tile_banks;
    uint8_t dpcm_banks;

    uint8_t (*vpu_readb)(uint16_t);
    void (*vpu_writeb)(uint16_t, uint8_t);

    uint8_t (*apu_readb)(uint16_t);
    void (*apu_writeb)(uint16_t, uint8_t);
};

/* Structure holding pointers to the memory banks, as well as handlers for
 * external parts of the address space.
 */
struct core_mmu
{
    uint8_t *rom_f;
    uint8_t *rom_s;
    uint8_t *ram_f;
    uint8_t *ram_s;
    uint8_t *tile_s;
    uint8_t *dpcm_s;
    uint8_t *cart_f;

    uint8_t rom_s_bank;
    uint8_t rom_s_total;
    uint8_t ram_s_bank;
    uint8_t ram_s_total;
    uint8_t tile_bank;
    uint8_t tile_s_total;
    uint8_t dpcm_bank;
    uint8_t dpcm_s_total;

    uint8_t (*vpu_readb)(uint16_t);
    void (*vpu_writeb)(uint16_t, uint8_t);

    uint8_t (*apu_readb)(uint16_t);
    void (*apu_writeb)(uint16_t, uint8_t);
};

/* Static memory banks. */
static uint8_t *rom_f;
static uint8_t **rom_s;
static uint8_t *ram_f;
static uint8_t **ram_s;
static uint8_t **tile_s;
static uint8_t **dpcm_s;
static uint8_t *cart_f;

/* Function declarations. */
int core_mmu_init(struct core_mmu **, struct core_mmu_params *);
int core_mmu_destroy(struct core_mmu *);

int core_mmu_bank_select(struct core_mmu *, enum core_mmu_bank, uint8_t);

uint8_t core_mmu_readb(struct core_mmu *, uint16_t);
void core_mmu_writeb(struct core_mmu *, uint16_t, uint8_t);

uint16_t core_mmu_readw(struct core_mmu *, uint16_t);
void core_mmu_writew(struct core_mmu *, uint16_t, uint8_t);

#endif

