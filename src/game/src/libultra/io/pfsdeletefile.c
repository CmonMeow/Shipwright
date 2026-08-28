#include "ultra64/pfs.h"
#include "global.h"

int32_t osPfsDeleteFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName) {
    int32_t file_no;
    int32_t ret;
    __OSInode inode;
    __OSDir dir;
    __OSInodeUnit last_page;
    uint8_t startpage = { 0 };
    uint8_t bank;

    if ((companyCode == 0) || (gameCode == 0)) {
        return PFS_ERR_INVALID;
    }
    if ((ret = osPfsFindFile(pfs, companyCode, gameCode, gameName, extName, &file_no)) != 0) {
        return ret;
    }
    if ((pfs->activebank != 0) && (ret = __osPfsSelectBank(pfs, 0)) != 0) {
        return ret;
    }

    if ((ret = __osContRamRead(pfs->queue, pfs->channel, pfs->dir_table + file_no, (uint8_t*)&dir)) != 0) {
        return ret;
    }

    startpage = dir.start_page.inode_t.page;
    for (bank = dir.start_page.inode_t.bank; bank < pfs->banks;) {
        if ((ret = __osPfsRWInode(pfs, &inode, PFS_READ, bank)) != 0) {
            return ret;
        }
        if ((ret = __osPfsReleasePages(pfs, &inode, startpage, bank, &last_page)) != 0) {
            return ret;
        }
        if ((ret = __osPfsRWInode(pfs, &inode, PFS_WRITE, bank)) != 0) {
            return ret;
        }
        if (last_page.ipage == PFS_EOF) {
            break;
        }
        bank = last_page.inode_t.bank;
        startpage = last_page.inode_t.page;
    }

    if (bank >= pfs->banks) {
        return PFS_ERR_INCONSISTENT;
    }
    bzero(&dir, sizeof(__OSDir));

    ret = __osContRamWrite(pfs->queue, pfs->channel, pfs->dir_table + file_no, (uint8_t*)&dir, 0);

    return ret;
}

int32_t __osPfsReleasePages(OSPfs* pfs, __OSInode* inode, uint8_t initialPage, uint8_t bank, __OSInodeUnit* finalPage) {
    __OSInodeUnit next;
    int32_t ret = 0;

    next.ipage = (uint16_t)((bank << 8) + initialPage);

    do {
        __OSInodeUnit prev = next;
        next = inode->inodePage[next.inode_t.page];
        inode->inodePage[prev.inode_t.page].ipage = PFS_PAGE_NOT_USED;
    } while (next.ipage >= pfs->inodeStartPage && next.inode_t.bank == bank);

    *finalPage = next;

    return ret;
}
