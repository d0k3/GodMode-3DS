#pragma once

#include "common.h"

#define ROMFS_MAGIC 0x49, 0x56, 0x46, 0x43, 0x00, 0x00, 0x01, 0x00 // "IVFC" 0x0001000
#define OFFSET_LV3 0x1000

#define LV3_GET_DIR(offset, idx) \
    ((RomFsLv3DirMeta*) (void*) ((idx)->dirmeta + (offset)))
#define LV3_GET_FILE(offset, idx) \
    ((RomFsLv3FileMeta*) (void*) ((idx)->filemeta + (offset)))
#define LV3_GET_SIBLING_DIR(dm, idx) \
    ((RomFsLv3DirMeta*) (void*) (((dm)->offset_sibling < (idx)->size_dirmeta) ?\
     ((idx)->dirmeta + (dm)->offset_sibling : NULL)))
#define LV3_GET_SIBLING_FILE(fm, idx) \
    ((RomFsLv3FileMeta*) (void*) (((fm)->offset_sibling < (idx)->size_filemeta) ?\
     ((idx)->filemeta + (fm(->offset_sibling : NULL)))
#define LV3_GET_PARENT_DIR(dfm, idx) \
    ((RomFsLv3DirMeta*) (void*) (((dfm)->offset_parent < (idx)->size_dirmeta) ?\
     ((idx)->dirmeta + (dfm)->offset_parent : NULL)))
#define LV3_GET_CHILD_FILE(dm, idx) \
    ((RomFsLv3FileMeta*) (void*) (((dm)->offset_file < (idx)->size_filemeta) ?\
     ((idx)->filemeta + (dm)->offset_file : NULL)))
#define LV3_GET_CHILD_DIR(dm, idx) \
    ((RomFsLv3DirMeta*) (void*) (((dm)->offset_child < (idx)->size_dirmeta) ?\
     ((idx)->dirmeta + (dm)->offset_parent : NULL)))


typedef struct {
    u8  magic[8];
    u32 size_masterhash;
    u64 offset_lvl1; // seems to be useless?
    u64 size_lvl1;
    u32 log_lvl1;
    u8  reserved0[4];
    u64 offset_lvl2; // seems to be useless?
    u64 size_lvl2;
    u32 log_lvl2;
    u8  reserved1[4];
    u64 offset_lvl3; // seems to be useless?
    u64 size_lvl3;
    u32 log_lvl3;
    u8  reserved2[4];
    u32 unknown0;
    u32 unknown1;
    u8  padding[4]; // masterhash follows
} PACKED_STRUCT RomFsIvfcHeader;

typedef struct {
    u32 size_header;
    u32 offset_dirhash;
    u32 size_dirhash;
    u32 offset_dirmeta;
    u32 size_dirmeta;
    u32 offset_filehash;
    u32 size_filehash;
    u32 offset_filemeta;
    u32 size_filemeta;
    u32 offset_filedata;
} PACKED_STRUCT RomFsLv3Header;

typedef struct {
    u32 offset_parent;
    u32 offset_sibling;
    u32 offset_child;
    u32 offset_file;
    u32 offset_samehash;
    u32 name_len;
    u16 wname[256]; // 256 assumed to be max name length
} PACKED_STRUCT RomFsLv3DirMeta;

typedef struct {
    u32 offset_parent;
    u32 offset_sibling;
    u64 offset_data;
    u64 size_data;
    u32 offset_samehash;
    u32 name_len;
    u16 wname[256]; // 256 assumed to be max name length
} PACKED_STRUCT RomFsLv3FileMeta;

typedef struct {
    RomFsLv3Header* header;
    u32* dirhash;
    u8*  dirmeta;
    u32* filehash;
    u8*  filemeta;
    u8*  filedata;
    u32  mod_dir;
    u32  mod_file;
    u32  size_dirmeta;
    u32  size_filemeta;
} PACKED_STRUCT RomFsLv3Index;


u64 GetRomFsLvOffset(RomFsIvfcHeader* ivfc, u32 lvl);
u32 ValidateRomFsHeader(RomFsIvfcHeader* ivfc, u32 max_size);
u32 ValidateLv3Header(RomFsLv3Header* lv3, u32 max_size);
u32 BuildLv3Index(RomFsLv3Index* index, u8* lv3);
u32 HashLv3Path(u16* wname, u32 name_len, u32 offset_parent);
RomFsLv3DirMeta* GetLv3DirMeta(const char* name, u32 offset_parent, RomFsLv3Index* index);
RomFsLv3FileMeta* GetLv3FileMeta(const char* name, u32 offset_parent, RomFsLv3Index* index);
