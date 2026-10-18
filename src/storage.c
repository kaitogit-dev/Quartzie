#include <stdint.h>
#include <stddef.h>

#define STORAGE_SLOTS 16
#define STORAGE_DATA_LBA 64u
#define STORAGE_SECTORS_PER_SLOT 16u
#define STORAGE_MAX_BYTES 4096u
#define STORAGE_TOTAL_SECTORS 32768u

static uint8_t storage_dir[512];
static uint8_t storage_work[512];
static uint8_t storage_sector[512];
static int storage_ready;
static int storage_error;
static uint32_t storage_generation;
static int storage_directory_lba;

static uint32_t storage_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void storage_w32(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
    p[2] = (uint8_t)(n >> 16);
    p[3] = (uint8_t)(n >> 24);
}

static uint32_t storage_crc(const uint8_t *p, uint32_t n) {
    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static int storage_same(const uint8_t *p, const char *text, int count) {
    for (int i = 0; i < count; i++) if (p[i] != (uint8_t)text[i]) return 0;
    return 1;
}

static int storage_name_valid(const char *name) {
    int n = 0;
    while (name[n]) {
        unsigned char c = (unsigned char)name[n];
        if (n >= 19 || c < 32 || c > 126 || c == '/' || c == '\\' || c == ':') return 0;
        n++;
    }
    return n > 0;
}

#ifndef CINMIX_STORAGE_TEST
static uint16_t storage_in16(uint16_t port) {
    uint16_t data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static void storage_out16(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" :: "a"(data), "Nd"(port));
}

static void storage_delay(void) {
    for (int i = 0; i < 4; i++) (void)in8(0x3f6);
}

static int storage_wait(int need_data) {
    for (uint32_t i = 0; i < 100000; i++) {
        uint8_t status = in8(0x1f7);
        if (status == 0 || status == 0xff || (status & 0x21)) return 0;
        if (!(status & 0x80) && (!need_data || (status & 8))) return 1;
    }
    return 0;
}

static int storage_probe(void) {
    out8(0x3f6, 2);
    out8(0x1f6, 0xa0);
    storage_delay();
    out8(0x1f2, 0);
    out8(0x1f3, 0);
    out8(0x1f4, 0);
    out8(0x1f5, 0);
    out8(0x1f7, 0xec);
    if (!storage_wait(1)) return 0;
    uint16_t fields[256];
    for (int i = 0; i < 256; i++) fields[i] = storage_in16(0x1f0);
    uint32_t sectors = (uint32_t)fields[60] | ((uint32_t)fields[61] << 16);
    return (fields[49] & 0x200) && sectors >= STORAGE_TOTAL_SECTORS;
}

static int storage_transfer(uint32_t lba, uint8_t *buffer, int write) {
    if (lba >= STORAGE_TOTAL_SECTORS) return 0;
    if (!storage_wait(0)) return 0;
    out8(0x1f6, (uint8_t)(0xe0 | ((lba >> 24) & 0x0f)));
    storage_delay();
    out8(0x1f1, 0);
    out8(0x1f2, 1);
    out8(0x1f3, (uint8_t)lba);
    out8(0x1f4, (uint8_t)(lba >> 8));
    out8(0x1f5, (uint8_t)(lba >> 16));
    out8(0x1f7, write ? 0x30 : 0x20);
    if (!storage_wait(1)) return 0;
    for (int i = 0; i < 256; i++) {
        if (write) storage_out16(0x1f0, (uint16_t)buffer[i * 2] | ((uint16_t)buffer[i * 2 + 1] << 8));
        else {
            uint16_t value = storage_in16(0x1f0);
            buffer[i * 2] = (uint8_t)value;
            buffer[i * 2 + 1] = (uint8_t)(value >> 8);
        }
    }
    return storage_wait(0);
}

static int storage_flush(void) {
    out8(0x1f7, 0xe7);
    return storage_wait(0);
}
#else
extern int storage_test_read(uint32_t lba, uint8_t *buffer);
extern int storage_test_write(uint32_t lba, const uint8_t *buffer);
static int storage_probe(void) { return 1; }
static int storage_transfer(uint32_t lba, uint8_t *buffer, int write) {
    if (lba >= STORAGE_TOTAL_SECTORS) return 0;
    return write ? storage_test_write(lba, buffer) : storage_test_read(lba, buffer);
}
static int storage_flush(void) { return 1; }
#endif

static int storage_dir_valid(const uint8_t *p) {
    return storage_same(p, "CMXDIR01", 8) && storage_crc(p + 16, 496) == storage_u32(p + 12);
}

static uint8_t *storage_entry(int slot) { return storage_dir + 16 + slot * 28; }

static int storage_slot(const char *name) {
    if (!storage_ready) return -1;
    for (int i = 0; i < STORAGE_SLOTS; i++) {
        uint8_t *e = storage_entry(i);
        if (!(e[23] & 1)) continue;
        int j = 0;
        while (j < 20 && name[j] && e[j] == (uint8_t)name[j]) j++;
        if (j < 20 && name[j] == 0 && e[j] == 0) return i;
    }
    return -1;
}

static int storage_mount(void) {
    storage_ready = 0;
    storage_error = 0;
    if (!storage_probe() || !storage_transfer(0, storage_sector, 0)) return 0;
    if (!storage_same(storage_sector, "CINMIXFS", 8) || storage_u32(storage_sector + 8) != 1 ||
        storage_u32(storage_sector + 12) != STORAGE_TOTAL_SECTORS ||
        storage_u32(storage_sector + 16) != STORAGE_SLOTS ||
        storage_u32(storage_sector + 20) != STORAGE_DATA_LBA ||
        storage_u32(storage_sector + 24) != STORAGE_SECTORS_PER_SLOT ||
        storage_crc(storage_sector, 28) != storage_u32(storage_sector + 28)) return 0;
    int valid_a = storage_transfer(1, storage_dir, 0) && storage_dir_valid(storage_dir);
    int valid_b = storage_transfer(2, storage_work, 0) && storage_dir_valid(storage_work);
    if (!valid_a && !valid_b) return 0;
    uint32_t first = valid_a ? storage_u32(storage_dir + 8) : 0;
    uint32_t second = valid_b ? storage_u32(storage_work + 8) : 0;
    if (valid_b && (!valid_a || (int32_t)(second - first) > 0)) {
        for (int i = 0; i < 512; i++) storage_dir[i] = storage_work[i];
        storage_directory_lba = 2;
        storage_generation = second;
    } else {
        storage_directory_lba = 1;
        storage_generation = first;
    }
    storage_ready = 1;
    return 1;
}

static int storage_load(const char *name, char *buffer, uint32_t cap) {
    int slot = storage_slot(name);
    if (slot < 0 || cap == 0) return -1;
    uint8_t *e = storage_entry(slot);
    uint32_t length = (uint32_t)e[20] | ((uint32_t)e[21] << 8);
    if (length >= cap || length > STORAGE_MAX_BYTES || e[22] > 1) return -1;
    uint32_t lba = STORAGE_DATA_LBA + (uint32_t)slot * STORAGE_SECTORS_PER_SLOT + (uint32_t)e[22] * 8;
    uint32_t checksum = 0xffffffffu;
    for (uint32_t i = 0; i < (length + 511) / 512; i++) {
        if (!storage_transfer(lba + i, storage_sector, 0)) { storage_error = 1; return -1; }
        uint32_t take = length - i * 512;
        if (take > 512) take = 512;
        for (uint32_t j = 0; j < take; j++) {
            uint8_t v = storage_sector[j];
            buffer[i * 512 + j] = (char)v;
            checksum ^= v;
            for (int bit = 0; bit < 8; bit++) checksum = (checksum >> 1) ^ (0xedb88320u & (0u - (checksum & 1u)));
        }
    }
    buffer[length] = 0;
    if ((~checksum) != storage_u32(e + 24)) { storage_error = 2; return -1; }
    return (int)length;
}

static int storage_save(const char *name, const char *buffer, uint32_t length) {
    if (!storage_ready || !storage_name_valid(name) || length > STORAGE_MAX_BYTES || (!buffer && length)) return 0;
    int slot = storage_slot(name);
    if (slot < 0) {
        for (int i = 0; i < STORAGE_SLOTS; i++) if (!(storage_entry(i)[23] & 1)) { slot = i; break; }
    }
    if (slot < 0) { storage_error = 3; return 0; }
    uint8_t *old = storage_entry(slot);
    int bank = (old[23] & 1) ? 1 - (int)old[22] : 0;
    uint32_t lba = STORAGE_DATA_LBA + (uint32_t)slot * STORAGE_SECTORS_PER_SLOT + (uint32_t)bank * 8;
    for (uint32_t i = 0; i < (length + 511) / 512; i++) {
        for (int j = 0; j < 512; j++) {
            uint32_t k = i * 512 + (uint32_t)j;
            storage_sector[j] = k < length ? (uint8_t)buffer[k] : 0;
        }
        if (!storage_transfer(lba + i, storage_sector, 1)) { storage_error = 1; return 0; }
    }
    if (!storage_flush()) { storage_error = 1; return 0; }
    for (int i = 0; i < 512; i++) storage_work[i] = storage_dir[i];
    uint8_t *e = storage_work + 16 + slot * 28;
    for (int i = 0; i < 20; i++) e[i] = 0;
    for (int i = 0; name[i]; i++) e[i] = (uint8_t)name[i];
    e[20] = (uint8_t)length;
    e[21] = (uint8_t)(length >> 8);
    e[22] = (uint8_t)bank;
    e[23] = 1;
    storage_w32(e + 24, storage_crc((const uint8_t *)buffer, length));
    storage_w32(storage_work + 8, storage_generation + 1);
    storage_w32(storage_work + 12, storage_crc(storage_work + 16, 496));
    int next_lba = storage_directory_lba == 1 ? 2 : 1;
    if (!storage_transfer((uint32_t)next_lba, storage_work, 1) || !storage_flush()) { storage_error = 1; return 0; }
    for (int i = 0; i < 512; i++) storage_dir[i] = storage_work[i];
    storage_directory_lba = next_lba;
    storage_generation++;
    storage_error = 0;
    return 1;
}

static int storage_list(int index, char *name) {
    if (!storage_ready || index < 0) return 0;
    for (int i = 0; i < STORAGE_SLOTS; i++) {
        uint8_t *e = storage_entry(i);
        if (!(e[23] & 1)) continue;
        if (index-- == 0) {
            for (int j = 0; j < 20; j++) name[j] = (char)e[j];
            name[19] = 0;
            return 1;
        }
    }
    return 0;
}
