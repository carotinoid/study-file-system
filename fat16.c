#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h> // TODO
#include <time.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define CLUSTER_FREE 0x0000
#define CLUSTER_BAD  0xFFF7
#define CLUSTER_FINAL 0xFFFF

#pragma pack(push, 1)

// --- 1. Boot Sector (BPB) ---
typedef struct {
    uint8_t  jumpBoot[3];
    char     oemName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numberOfFats;
    uint16_t rootEntryCount;
    uint16_t totalSector16;
    uint8_t  media;
    uint16_t sectorsPerFat16;
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSectors;
    uint32_t totalSector32;
    
    // FAT12/16 Extended BPB
    uint8_t  driveNumber;
    uint8_t  reserved;
    uint8_t  bootSignature;
    uint32_t volumeSerialNumber;
    char     volumeLabel[11];
    char     fileSystemType[8];

    uint8_t  bootCode[448];
    uint16_t bootSectorSignature;
} BootSector;

// --- 2. Directory Entry (32 bytes) ---
typedef struct {
    char     filename[8];
    char     extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  createTimeTenth;
    uint16_t createTime;
    uint16_t createDate;
    uint16_t lastAccessDate;
    uint16_t firstClusterHigh;
    uint16_t writeTime;
    uint16_t writeDate;
    uint16_t firstClusterLow;
    uint32_t fileSize;
} DirectoryEntry;

#pragma pack(pop)

// --- Constants ---
#define SECTOR_SIZE 512
#define FAT_ENTRY_SIZE 2 
#define NUM_FATS 2

void init_boot_sector(BootSector *bs, uint32_t total_sectors);
void create_disk_image(const char *filename, uint32_t total_sectors);

void create_disk_image(const char *filename, uint32_t total_sectors) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("File open failed");
        return;
    }

    // Generate Boot Sector
    BootSector bs;
    memset(&bs, 0, sizeof(bs));
    init_boot_sector(&bs, total_sectors);
    
    fwrite(&bs, sizeof(BootSector), 1, fp);
    printf("[*] Boot Sector Written. Total Sectors: %d\n", total_sectors);

    // initi FAT area 
    uint32_t fat_size_bytes = bs.sectorsPerFat16 * bs.bytesPerSector;
    printf("[&] FAT Size (bytes): %d\n", fat_size_bytes);
    uint8_t *fat_table = (uint8_t *)calloc(1, fat_size_bytes);
    
    ((uint16_t*)fat_table)[0] = 0xFFF8; 
    ((uint16_t*)fat_table)[1] = 0xFFFF;

    fwrite(fat_table, 1, fat_size_bytes, fp); // FAT 1
    fwrite(fat_table, 1, fat_size_bytes, fp); // FAT 2
    free(fat_table);
    printf("[*] FAT Tables Written.\n");

    // Root Directory Area 
    uint32_t root_dir_size = bs.rootEntryCount * sizeof(DirectoryEntry);
    uint8_t *root_dir = (uint8_t *)calloc(1, root_dir_size);

    fwrite(root_dir, 1, root_dir_size, fp);
    free(root_dir);
    printf("[*] Root Directory Written.\n");

    // Data Area 
    long current_pos = ftell(fp);
    long target_size = total_sectors * SECTOR_SIZE;
    
    fseek(fp, target_size - 1, SEEK_SET);
    fputc(0, fp);

    printf("[*] Data Area Allocated. Image '%s' Created Successfully.\n", filename);

    fclose(fp);
}

void init_boot_sector(BootSector *bs, uint32_t total_sectors) {
    bs->jumpBoot[0] = 0xEB; bs->jumpBoot[1] = 0x3C; bs->jumpBoot[2] = 0x90;
    memcpy(bs->oemName, "MSWIN4.1", 8);
    bs->bytesPerSector = SECTOR_SIZE;
    bs->sectorsPerCluster = 4;      // 2KB Cluster
    bs->reservedSectorCount = 1;    // Boot Sector only
    bs->numberOfFats = NUM_FATS;
    bs->rootEntryCount = 512;
    
    if (total_sectors < 65535) {
        bs->totalSector16 = (uint16_t)total_sectors;
        bs->totalSector32 = 0;
    } else {
        bs->totalSector16 = 0;
        bs->totalSector32 = total_sectors;
    }
    
    bs->media = 0xF8;
    uint32_t clusters = total_sectors / bs->sectorsPerCluster;
    bs->sectorsPerFat16 = (clusters * 2 + SECTOR_SIZE - 1) / SECTOR_SIZE; 

    bs->bootSectorSignature = 0x55AA;
    memcpy(bs->fileSystemType, "FAT16   ", 8);
    memcpy(bs->volumeLabel, "NO NAME    ", 11);

    memset(bs->bootCode, -1, sizeof(bs->bootCode));

    bs->sectorsPerTrack = 63; // dummy
    bs->numberOfHeads = 255; // dummy
    bs->hiddenSectors = 0;

    printf("[&] Debug info:\n");
    printf("    jumpBoot: %02X %02X %02X\n", bs->jumpBoot[0], bs->jumpBoot[1], bs->jumpBoot[2]);
    printf("    oemName: %.8s\n", bs->oemName);
    printf("    bytesPerSector: %d\n", bs->bytesPerSector);
    printf("    sectorsPerCluster: %d\n", bs->sectorsPerCluster);
    printf("    reservedSectorCount: %d\n", bs->reservedSectorCount);
    printf("    numberOfFats: %d\n", bs->numberOfFats);
    printf("    rootEntryCount: %d\n", bs->rootEntryCount);
    printf("    totalSector16: %d\n", bs->totalSector16);
    printf("    totalSector32: %d\n", bs->totalSector32);
    printf("    media: %02X\n", bs->media);
    printf("    sectorsPerFat16: %d\n", bs->sectorsPerFat16);
    printf("    sectorsPerTrack: %d\n", bs->sectorsPerTrack);
    printf("    numberOfHeads: %d\n", bs->numberOfHeads);
    printf("    hiddenSectors: %d\n", bs->hiddenSectors);
    printf("    driveNumber: %02X\n", bs->driveNumber);
    printf("    reserved: %02X\n", bs->reserved);
    printf("    bootSignature: %02X\n", bs->bootSignature);
    printf("    volumeSerialNumber: %08X\n", bs->volumeSerialNumber);
    printf("    volumeLabel: %.11s\n", bs->volumeLabel);
    printf("    fileSystemType: %.8s\n", bs->fileSystemType);

    printf("    bootCode(first 6 bytes): ");
    for (int i = 0; i < 6; i++) {
        printf("%02X ", bs->bootCode[i]);
    }
    printf("\n");
    printf("    bootSectorSignature: %04X\n", bs->bootSectorSignature);
}

void format_filename(const char *src, char *dest_name, char *dest_ext) {
    memset(dest_name, ' ', 8);
    memset(dest_ext, ' ', 3);
    
    char buffer[12];
    strcpy(buffer, src);
    for(int i=0; buffer[i]; i++) {
        if(buffer[i] >= 'a' && buffer[i] <= 'z') buffer[i] -= 32;
    }

    char *dot = strchr(buffer, '.');
    int name_len = dot ? (dot - buffer) : strlen(buffer);
    int ext_len = dot ? strlen(dot + 1) : 0;

    if (name_len > 8) name_len = 8;
    if (ext_len > 3) ext_len = 3;

    memcpy(dest_name, buffer, name_len);
    if (dot) memcpy(dest_ext, dot + 1, ext_len);
}

void add_simple_file(const char *img_name, const char *filename, const char *content) {
    FILE *fp = fopen(img_name, "r+b");
    if (!fp) { perror("Image open failed"); return; }

    BootSector bs;
    fread(&bs, sizeof(BootSector), 1, fp);

    uint32_t fat_start = bs.reservedSectorCount * bs.bytesPerSector;
    uint32_t fat_size = bs.sectorsPerFat16 * bs.bytesPerSector;
    uint32_t root_start = fat_start + (bs.numberOfFats * fat_size);
    uint32_t root_size = bs.rootEntryCount * sizeof(DirectoryEntry);
    uint32_t data_start = root_start + root_size;

    // find free cluster
    uint16_t target_cluster = 0;
    uint16_t fat_entry;
    fseek(fp, fat_start, SEEK_SET);
    fseek(fp, 2 * 2, SEEK_CUR); // 2 entries * 2 bytes

    for (int i = 2; i < 65536; i++) {
        fread(&fat_entry, 2, 1, fp);
        if (fat_entry == 0x0000) {
            target_cluster = i;
            break;
        }
    }

    if (target_cluster == 0) {
        printf("[!] Error: No free clusters found.\n");
        fclose(fp);
        return;
    }

    printf("[*] Found free cluster: %d\n", target_cluster);

    uint32_t fat_offset = target_cluster * 2;
    uint16_t eoc = 0xFFFF; // End of File

    fseek(fp, fat_start + fat_offset, SEEK_SET);
    fwrite(&eoc, 2, 1, fp);
    fseek(fp, fat_start + fat_size + fat_offset, SEEK_SET);
    fwrite(&eoc, 2, 1, fp);

    DirectoryEntry entry;
    memset(&entry, 0, sizeof(entry));
    format_filename(filename, entry.filename, entry.extension);
    entry.attributes = 0x20; // Archive
    entry.firstClusterLow = target_cluster;
    entry.fileSize = strlen(content);

    fseek(fp, root_start, SEEK_SET);
    DirectoryEntry temp;
    int found_slot = 0;
    for (int i = 0; i < bs.rootEntryCount; i++) {
        long pos = ftell(fp);
        fread(&temp, sizeof(DirectoryEntry), 1, fp);
        if (temp.filename[0] == 0x00 || (uint8_t)temp.filename[0] == 0xE5) {
            fseek(fp, pos, SEEK_SET);
            fwrite(&entry, sizeof(DirectoryEntry), 1, fp);
            found_slot = 1;
            break;
        }
    }

    if (!found_slot) {
        printf("[!] Error: Root directory full.\n");
        fclose(fp);
        return;
    }

    uint32_t cluster_size = bs.sectorsPerCluster * bs.bytesPerSector;
    uint32_t data_offset = data_start + (target_cluster - 2) * cluster_size;

    fseek(fp, data_offset, SEEK_SET);
    fwrite(content, 1, strlen(content), fp);

    printf("[*] File '%s' added successfully (Size: %zu bytes, Cluster: %d).\n", 
           filename, strlen(content), target_cluster);

    fclose(fp);
}

#pragma pack(pop)

int main() {
    const char *img_file = "disk.img";
    
    printf("--- Creating Disk Image ---\n");
    create_disk_image(img_file, 40960); // 20MB
    
    printf("\n--- Injecting File ---\n");
    add_simple_file(img_file, "hello.txt", "Hello, FAT16 World! This is a raw write test.");
    add_simple_file(img_file, "test.log", "Another file log entry.\nSecond Line.");

    return 0;
}
