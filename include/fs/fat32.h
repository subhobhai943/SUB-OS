#ifndef _FS_FAT32_H
#define _FS_FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <fs/vfs.h>

/* FAT32 Extended BIOS Parameter Block */
typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_short;
    uint8_t  media_type;
    uint16_t table_size_16;
    uint16_t sectors_per_track;
    uint16_t head_side_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 Extended
    uint32_t table_size_32;
    uint16_t extended_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fat_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fat_type_label[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    char     name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dirent_t;

void fat32_init(void);
vfs_node_t* fat32_mount(const char* block_device_name);

#endif // _FS_FAT32_H
