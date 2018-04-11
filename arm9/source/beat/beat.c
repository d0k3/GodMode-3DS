// C port of byuu's \nall\beat\patch.hpp and \multi.hpp, which were released under GPLv3
// https://github.com/eai04191/beat/blob/master/nall/beat/patch.hpp
// https://github.com/eai04191/beat/blob/master/nall/beat/multi.hpp
// Ported by Hyarion for use with VirtualFatFS

#include "beat.h"

size_t patchSize;
FIL patchFile;
FIL sourceFile;
FIL targetFile;

unsigned int modifyOffset;
uint32_t modifyChecksum;
uint32_t targetChecksum;

bool bpmIsActive;
uint32_t bpmChecksum;

uint8_t buffer;
unsigned int br;

char progressText[54];
bool progressCancel;

uint8_t BPSread() {
    fvx_read(&patchFile, &buffer, 1, &br);
    modifyChecksum = crc32_adjust(modifyChecksum, buffer);
    if (bpmIsActive) bpmChecksum = crc32_adjust(bpmChecksum, buffer);
    modifyOffset++;
    return buffer;
}

uint64_t BPSdecode() {
    uint64_t data = 0, shift = 1;
    while(true) {
        uint8_t x = BPSread();
        data += (x & 0x7f) * shift;
        if(x & 0x80) break;
        shift <<= 7;
        data += shift;
    }
    return data;
}

void BPSwrite(uint8_t data) {
    fvx_write(&targetFile, &data, 1, &br);
    targetChecksum = crc32_adjust(targetChecksum, data);
}

int ApplyBeatPatch() {
    unsigned int sourceRelativeOffset = 0, targetRelativeOffset = 0;
    modifyOffset = 0;
    modifyChecksum = ~0;
    targetChecksum = ~0;
    if(patchSize < 19) return BEAT_PATCH_TOO_SMALL;
    
    if(BPSread() != 'B') return BEAT_PATCH_INVALID_HEADER;
    if(BPSread() != 'P') return BEAT_PATCH_INVALID_HEADER;
    if(BPSread() != 'S') return BEAT_PATCH_INVALID_HEADER;
    if(BPSread() != '1') return BEAT_PATCH_INVALID_HEADER;
    
    size_t modifySourceSize = BPSdecode();
    size_t modifyTargetSize = BPSdecode();
    size_t modifyMarkupSize = BPSdecode();
    for(unsigned int n = 0; n < modifyMarkupSize; n++) BPSread(); // metadata, not useful to us

    fvx_lseek(&targetFile, modifyTargetSize);
    fvx_lseek(&targetFile, 0);
    size_t sourceSize = fvx_size(&sourceFile);
    size_t targetSize = fvx_size(&targetFile);
    if(modifySourceSize > sourceSize) return BEAT_SOURCE_TOO_SMALL;
    if(modifyTargetSize > targetSize) return BEAT_TARGET_TOO_SMALL;

    while((modifyOffset < patchSize - 12) && !progressCancel) {
        if (!ShowProgress(fvx_tell(&patchFile), fvx_size(&patchFile), progressText)) progressCancel = true;
        unsigned int length = BPSdecode();
        unsigned int mode = length & 3;
        length = (length >> 2) + 1;

        switch(mode) {
            case BEAT_SOURCEREAD:
                fvx_lseek(&sourceFile, fvx_tell(&targetFile));
                while(length--) {
                    fvx_read(&sourceFile, &buffer, 1, &br);
                    BPSwrite(buffer);
                }
                break;
            case BEAT_TARGETREAD:
                while(length--) BPSwrite(BPSread());
                break;
            case BEAT_SOURCECOPY:
            case BEAT_TARGETCOPY:
                ; // intentional null statement
                int offset = BPSdecode();
                bool negative = offset & 1;
                offset >>= 1;
                if(negative) offset = -offset;

                if(mode == BEAT_SOURCECOPY) {
                    sourceRelativeOffset += offset;
                    fvx_lseek(&sourceFile, sourceRelativeOffset);
                    while(length--) {
                        fvx_read(&sourceFile, &buffer, 1, &br);
                        BPSwrite(buffer);
                        sourceRelativeOffset++;
                    }
                    fvx_lseek(&sourceFile, fvx_tell(&targetFile));
                } else {
                    unsigned int targetOffset = fvx_tell(&targetFile);
                    targetRelativeOffset += offset;
                    while(length--) {
                        fvx_lseek(&targetFile, targetRelativeOffset);
                        fvx_read(&targetFile, &buffer, 1, &br);
                        fvx_lseek(&targetFile, targetOffset);
                        BPSwrite(buffer);
                        targetRelativeOffset++;
                        targetOffset++;
                    }
                }
                break;
        }
    }

    uint32_t modifySourceChecksum = 0, modifyTargetChecksum = 0, modifyModifyChecksum = 0;
    for(unsigned int n = 0; n < 32; n += 8) modifySourceChecksum |= BPSread() << n;
    for(unsigned int n = 0; n < 32; n += 8) modifyTargetChecksum |= BPSread() << n;
    uint32_t checksum = ~modifyChecksum;
    for(unsigned int n = 0; n < 32; n += 8) modifyModifyChecksum |= BPSread() << n;

    uint32_t sourceChecksum = crc32_calculate(sourceFile, modifySourceSize);
    targetChecksum = ~targetChecksum;

    fvx_close(&sourceFile);
    fvx_close(&targetFile);
    if (!bpmIsActive) fvx_close(&patchFile);

    if(sourceChecksum != modifySourceChecksum) return BEAT_SOURCE_CHECKSUM_INVALID;
    if(targetChecksum != modifyTargetChecksum) return BEAT_TARGET_CHECKSUM_INVALID;
    if(checksum != modifyModifyChecksum) return BEAT_PATCH_CHECKSUM_INVALID;
    
    return BEAT_SUCCESS;
}

int ApplyBPSPatch(const char* modifyName, const char* sourceName, const char* targetName) {
    bpmIsActive = false;
    
    if ((!CheckWritePermissions(targetName)) ||
        (fvx_open(&patchFile, modifyName, FA_READ) != FR_OK) ||
        (fvx_open(&sourceFile, sourceName, FA_READ) != FR_OK) ||
        (fvx_open(&targetFile, targetName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK))
        return BEAT_INVALID_FILE_PATH;
    
    patchSize = fvx_size(&patchFile);
    snprintf(progressText, 54, "%s", targetName);
    if (!ShowProgress(0, patchSize, progressText)) progressCancel = true;
    return ApplyBeatPatch();
}

uint8_t BPMread() {
    fvx_read(&patchFile, &buffer, 1, &br);
    bpmChecksum = crc32_adjust(bpmChecksum, buffer);
    return buffer;
}

uint64_t BPMreadNumber() {
    uint64_t data = 0, shift = 1;
    while(true) {
        uint8_t x = BPMread();
        data += (x & 0x7f) * shift;
        if(x & 0x80) break;
        shift <<= 7;
        data += shift;
    }
    return data;
}

char* BPMreadString(char text[], unsigned int length) {
    for(unsigned n = 0; n < length; n++) text[n] = BPMread();
    text[length] = '\0';
    return text;
}

uint32_t BPMreadChecksum() {
    uint32_t checksum = 0;
    checksum |= BPMread() <<  0;
    checksum |= BPMread() <<  8;
    checksum |= BPMread() << 16;
    checksum |= BPMread() << 24;
    return checksum;
}

int ApplyBPMPatch(const char* patchName, const char* sourcePath, const char* targetPath) {
    bpmIsActive = true;
    bpmChecksum = ~0;
    
    if ((!CheckWritePermissions(targetPath)) ||
        ((fvx_stat(targetPath, NULL) == FR_OK) && (fvx_runlink(targetPath) != FR_OK)) ||
        (fvx_mkdir(targetPath) != FR_OK) ||
        (fvx_open(&patchFile, patchName, FA_READ) != FR_OK))
        return BEAT_INVALID_FILE_PATH;
        
    if (!ShowProgress(0, fvx_size(&patchFile), progressText)) progressCancel = true;

    if(BPMread() != 'B') return BEAT_PATCH_INVALID_HEADER;
    if(BPMread() != 'P') return BEAT_PATCH_INVALID_HEADER;
    if(BPMread() != 'M') return BEAT_PATCH_INVALID_HEADER;
    if(BPMread() != '1') return BEAT_PATCH_INVALID_HEADER;
    uint64_t metadataLength = BPMreadNumber();
    while(metadataLength--) BPMread();

    while((fvx_tell(&patchFile) < fvx_size(&patchFile) - 4) && !progressCancel) {
        uint64_t encoding = BPMreadNumber();
        unsigned int action = encoding & 3;
        unsigned int targetLength = (encoding >> 2) + 1;
        char targetName[256];
        BPMreadString(targetName, targetLength);
        snprintf(progressText, 54, "%s", targetName);
        if (!ShowProgress(fvx_tell(&patchFile), fvx_size(&patchFile), progressText)) progressCancel = true;

        if(action == BEAT_CREATEPATH) {
            char newPath[256];
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            if (fvx_mkdir(newPath) != FR_OK) return BEAT_INVALID_FILE_PATH;
        } else if(action == BEAT_CREATEFILE) {
            char newPath[256];
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            FIL newFile;
            if (fvx_open(&newFile, newPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return BEAT_INVALID_FILE_PATH;
            uint64_t fileSize = BPMreadNumber();
            fvx_lseek(&newFile, fileSize);
            fvx_lseek(&newFile, 0);
            while(fileSize--) {
                buffer = BPMread();
                fvx_write(&newFile, &buffer, 1, &br);
            }
            BPMreadChecksum();
            fvx_close(&newFile);
        } else if(action == BEAT_MODIFYFILE) {
            uint64_t encoding = BPMreadNumber();
            char originPath[256], sourceName[256], oldPath[256], newPath[256];
            if (encoding & 1) snprintf(originPath, 256, "%s", targetPath);
            else snprintf(originPath, 256, "%s", sourcePath);
            if ((encoding >> 1) == 0) snprintf(sourceName, 256, "%s", targetName);
            else BPMreadString(sourceName, encoding >> 1);
            snprintf(oldPath, 256, "%s/%s", originPath, sourceName);
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            patchSize = BPMreadNumber();
            if ((fvx_open(&sourceFile, oldPath, FA_READ) != FR_OK) ||
                (fvx_open(&targetFile, newPath, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK))
                return BEAT_INVALID_FILE_PATH;
            int result = ApplyBeatPatch();
            if (result != BEAT_SUCCESS) return result;
        } else if(action == BEAT_MIRRORFILE) {
            uint64_t encoding = BPMreadNumber();
            char originPath[256], sourceName[256], oldPath[256], newPath[256];
            if (encoding & 1) snprintf(originPath, 256, "%s", targetPath);
            else snprintf(originPath, 256, "%s", sourcePath);
            if ((encoding >> 1) == 0) snprintf(sourceName, 256, "%s", targetName);
            else BPMreadString(sourceName, encoding >> 1);
            snprintf(oldPath, 256, "%s/%s", originPath, sourceName);
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            FIL oldFile, newFile;
            if ((fvx_open(&oldFile, oldPath, FA_READ) != FR_OK) ||
                (fvx_open(&newFile, newPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK))
                return BEAT_INVALID_FILE_PATH;
            uint64_t fileSize = fvx_size(&oldFile);
            fvx_lseek(&newFile, fileSize);
            fvx_lseek(&newFile, 0);
            while(fileSize--) {
                fvx_read(&patchFile, &buffer, 1, &br);
                fvx_write(&newFile, &buffer, 1, &br);
            }
            BPMreadChecksum();
            fvx_close(&oldFile);
            fvx_close(&newFile);
        }
    }

    uint32_t cksum = ~bpmChecksum;
    if(BPMread() != (uint8_t)(cksum >>  0)) return BEAT_BPM_CHECKSUM_INVALID;
    if(BPMread() != (uint8_t)(cksum >>  8)) return BEAT_BPM_CHECKSUM_INVALID;
    if(BPMread() != (uint8_t)(cksum >> 16)) return BEAT_BPM_CHECKSUM_INVALID;
    if(BPMread() != (uint8_t)(cksum >> 24)) return BEAT_BPM_CHECKSUM_INVALID;

    fvx_close(&patchFile);
    return BEAT_SUCCESS;
}