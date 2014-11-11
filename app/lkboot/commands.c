/*
 * Copyright (c) 2014 Brian Swetland
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include <string.h>
#include <endian.h>
#include <malloc.h>
#include <arch.h>
#include <err.h>
#include <trace.h>

#include <kernel/thread.h>

#include <lib/bio.h>
#include <lib/bootargs.h>
#include <lib/bootimage.h>
#include <lib/ptable.h>
#include <lib/sysparam.h>

#include <app/lkboot.h>

#if PLATFORM_ZYNQ
#include <platform/fpga.h>
#endif

#define bootdevice "spi0"

#define LOCAL_TRACE 0

extern void *lkb_iobuffer;
extern paddr_t lkb_iobuffer_phys;
extern size_t lkb_iobuffer_size;

struct lkb_command {
    struct lkb_command *next;
    const char *name;
    lkb_handler_t handler;
    void *cookie;
};

struct lkb_command *lkb_cmd_list = NULL;

void lkb_register(const char *name, lkb_handler_t handler, void *cookie) {
    struct lkb_command *cmd = malloc(sizeof(struct lkb_command));
    if (cmd != NULL) {
        cmd->next = lkb_cmd_list;
        cmd->name = name;
        cmd->handler = handler;
        cmd->cookie = cookie;
        lkb_cmd_list = cmd;
    }
}

static int do_reboot(void *arg) {
    thread_sleep(250);
    platform_halt(HALT_ACTION_REBOOT, HALT_REASON_SW_RESET);
    return 0;
}

static int do_boot(void *arg) {
    thread_sleep(250);

    /* construct a boot argument list */
    // XXX get memory smarter than this
    const size_t bootargs_size = 64*1024;
    void *args = (void *)((uintptr_t)lkb_iobuffer + lkb_iobuffer_size - bootargs_size);
    paddr_t args_phys = lkb_iobuffer_phys + lkb_iobuffer_size - bootargs_size;
    bootargs_start(args, bootargs_size);
    bootargs_add_command_line(args, bootargs_size, "what what");

    ulong lk_args[4];
    bootargs_generate_lk_arg_values(args_phys, lk_args);

    const void *ptr;

    /* sniff it to see if it's a bootimage or a raw image */
    bootimage_t *bi;
    if (bootimage_open(lkb_iobuffer, lkb_iobuffer_size, &bi) >= 0) {
        size_t len;

        /* it's a bootimage */
        TRACEF("detected bootimage\n");

        /* find the lk image */
        if (bootimage_get_file_section(bi, TYPE_LK, &ptr, &len) >= 0) {
            TRACEF("found lk section at %p\n", ptr);

            /* add the boot image to the argument list */
            size_t bootimage_size;
            bootimage_get_range(bi, NULL, &bootimage_size);

            bootargs_add_bootimage_pointer(args, bootargs_size, "pmem", lkb_iobuffer_phys, bootimage_size);
        }
    } else {
        /* raw image, just chain load it directly */
        TRACEF("raw image, chainloading\n");

        ptr = lkb_iobuffer;
    }

    arch_chain_load((void *)ptr, lk_args[0], lk_args[1], lk_args[2], lk_args[3]);

    return 0;
}

/* try to boot the system from a flash partition */
status_t do_flash_boot(void)
{
    status_t err;

    /* construct a boot argument list */
    // XXX get memory smarter than this
    const size_t bootargs_size = 64*1024;
    void *args = (void *)((uintptr_t)lkb_iobuffer + lkb_iobuffer_size - bootargs_size);
    paddr_t args_phys = lkb_iobuffer_phys + lkb_iobuffer_size - bootargs_size;
    bootargs_start(args, bootargs_size);
    bootargs_add_command_line(args, bootargs_size, "what what");

    ulong lk_args[4];
    bootargs_generate_lk_arg_values(args_phys, lk_args);

    const void *ptr;

    if (!ptable_found_valid()) {
        TRACEF("ptable not found\n");
        return ERR_NOT_FOUND;
    }

    /* find the system partition */
    struct ptable_entry entry;
    err = ptable_find("system", &entry);
    if (err < 0) {
        TRACEF("cannot find system partition\n");
        return ERR_NOT_FOUND;
    }

    /* get a direct pointer to the device */
    bdev_t *bdev = ptable_get_device();
    if (!bdev) {
        TRACEF("error opening boot device\n");
        return ERR_NOT_FOUND;
    }

    /* convert the bdev to a memory pointer */
    err = bio_ioctl(bdev, BIO_IOCTL_GET_MEM_MAP, (void *)&ptr);
    TRACEF("err %d, ptr %p\n", err, ptr);
    if (err < 0) {
        TRACEF("error getting direct pointer to block device\n");
        return ERR_NOT_FOUND;
    }

    /* sniff it to see if it's a bootimage or a raw image */
    bootimage_t *bi;
    if (bootimage_open((char *)ptr + entry.offset, entry.length, &bi) >= 0) {
        size_t len;

        /* it's a bootimage */
        TRACEF("detected bootimage\n");

        /* find the lk image */
        if (bootimage_get_file_section(bi, TYPE_LK, &ptr, &len) >= 0) {
            TRACEF("found lk section at %p\n", ptr);

            /* add the boot image to the argument list */
            size_t bootimage_size;
            bootimage_get_range(bi, NULL, &bootimage_size);

            bootargs_add_bootimage_pointer(args, bootargs_size, bdev->name, entry.offset, bootimage_size);
        }
    } else {
        /* did not find a bootimage, abort */
        bio_ioctl(bdev, BIO_IOCTL_PUT_MEM_MAP, NULL);
        return ERR_NOT_FOUND;
    }

    TRACEF("chain loading binary at %p\n", ptr);
    arch_chain_load((void *)ptr, lk_args[0], lk_args[1], lk_args[2], lk_args[3]);

    /* put the block device back into block mode (though we never get here) */
    bio_ioctl(bdev, BIO_IOCTL_PUT_MEM_MAP, NULL);

    return NO_ERROR;
}

// return NULL for success, error string for failure
int lkb_handle_command(lkb_t *lkb, const char *cmd, const char *arg, unsigned len, const char **result)
{
    *result = NULL;

    struct lkb_command *lcmd;
    for (lcmd = lkb_cmd_list; lcmd; lcmd = lcmd->next) {
        if (!strcmp(lcmd->name, cmd)) {
            *result = lcmd->handler(lkb, arg, len, lcmd->cookie);
            return 0;
        }
    }

    if (len > lkb_iobuffer_size) {
        *result = "buffer too small";
        return -1;
    }
    if (!strcmp(cmd, "flash") || !strcmp(cmd, "erase")) {
        struct ptable_entry entry;
        bdev_t *bdev;

        if (ptable_find(arg, &entry) < 0) {
            size_t plen = len;
            /* doesn't exist, make one */
#if PLATFORM_ZYNQ
            /* XXX not really the right place, should be in the ptable/bio layer */
            plen = ROUNDUP(plen, 256*1024);
#endif
            off_t off = ptable_allocate(plen, 0);
            if (off < 0) {
                *result = "no space to allocate partition";
                return -1;
            }

            if (ptable_add(arg, off, plen, 0) < 0) {
                *result = "error creating partition";
                return -1;
            }

            if (ptable_find(arg, &entry) < 0) {
                *result = "couldn't find partition after creating it";
                return -1;
            }
        }
        if (len > entry.length) {
            *result = "partition too small";
            return -1;
        }
        if (lkb_read(lkb, lkb_iobuffer, len)) {
            *result = "io error";
            return -1;
        }
        if (!(bdev = ptable_get_device())) {
            *result = "ptable_get_device failed";
            return -1;
        }
        printf("lkboot: erasing partition of size %llu\n", entry.length);
        if (bio_erase(bdev, entry.offset, entry.length) != (ssize_t)entry.length) {
            *result = "bio_erase failed";
            return -1;
        }
        if (!strcmp(cmd, "flash")) {
            printf("lkboot: writing to partition\n");
            if (bio_write(bdev, lkb_iobuffer, entry.offset, len) != (ssize_t)len) {
                *result = "bio_write failed";
                return -1;
            }
        }
    } else if (!strcmp(cmd, "remove")) {
        if (ptable_remove(arg) < 0) {
            *result = "remove failed";
            return -1;
        }
    } else if (!strcmp(cmd, "fpga")) {
#if PLATFORM_ZYNQ
        if (lkb_read(lkb, lkb_iobuffer, len)) {
            *result = "io error";
            return -1;
        }

        zynq_reset_fpga();
        zynq_program_fpga(lkb_iobuffer_phys, len);
        *result = NULL;
#else
        *result = "no fpga";
        return -1;
#endif
    } else if (!strcmp(cmd, "boot")) {
        if (lkb_read(lkb, lkb_iobuffer, len)) {
            *result = "io error";
            return -1;
        }
        thread_resume(thread_create("boot", &do_boot, NULL,
            DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));
    } else if (!strcmp(cmd, "getsysparam")) {
        const void *ptr;
        size_t len;
        if (sysparam_get_ptr(arg, &ptr, &len) == 0) {
            lkb_write(lkb, ptr, len);
        }
    } else if (!strcmp(cmd, "reboot")) {
        thread_resume(thread_create("reboot", &do_reboot, NULL,
            DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));
    } else {
        *result = "unknown command";
        return -1;
    }

    return 0;
}