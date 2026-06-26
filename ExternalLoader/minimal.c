/**
 * Minimal loader — test if CubeProgrammer can load GCC-built ELF
 */
#include <stdint.h>

__attribute__((section(".Dev_Inf"), used))
const struct { char name[32]; uint16_t t; uint32_t a,s,p,e; } StorageInfo = {
    "W25Q128_TEST", 4, 0, 0x1000000, 256, 0xFF
};

int Init(void)    { return 1; }
int Write(void)   { return 1; }
int SectorErase(void) { return 1; }
int MassErase(void)   { return 1; }
uint64_t Verify(void) { return 0; }
int DeInit(void)  { return 1; }
