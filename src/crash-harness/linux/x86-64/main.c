/**
 * main.c
 *
 * Copyright (C) 2018 zznop, zznop0x90@gmail.com
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include "utils.h"
#include "hooks.h"

#define ELF_MAGIC 0x464c457f

extern char **environ;
static bool _continue = 1;
struct syshooks _hookops[] = {
    {
        .sysnum = __NR_mmap,
        .callback = &mmap_hook
    }
};


static void dump_elf_base(pid_t pid, uint64_t addr)
{
    uint64_t qword;
    addr = addr & ~(PAGE_SIZE - 1);
    while (_continue) {
        qword = ptrace(PTRACE_PEEKDATA, pid, addr, 0);
        if (errno)
            return;

        if ((qword & 0x00000000ffffffff) == ELF_MAGIC) {
            printf("ELF base address: %016lx\n\n", addr);
            break;
        }

        addr -= PAGE_SIZE;
    }
}

static void dump_reg_memory(pid_t pid, uint64_t addr)
{
    uint8_t buf[16];
    size_t i;
    uint64_t *ptr;

    memset(buf, '\0', sizeof(buf));
    ptr = (uint64_t *)buf;
    *ptr = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
    if (errno) {
        printf("\n");
        return;
    }

    ptr += 1;
    *ptr = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);

    printf(" -> ");
    for (i = 0; i < sizeof(buf); i++) {
        printf("%02x ", buf[i]);
    }

    printf("    ");
    for (i = 0; i < sizeof(buf); i++) {
        if (isprint(buf[i]))
            printf("%c", buf[i]);
        else
            printf(".");
    }

    printf("\n");
}

static void display_crash_dump(pid_t pid)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    dump_elf_base(pid, regs.rip);
    printf("rax    : %016llx", regs.rax);
    dump_reg_memory(pid, regs.rax);
    printf("rbx    : %016llx", regs.rbx);
    dump_reg_memory(pid, regs.rbx);
    printf("rcx    : %016llx", regs.rcx);
    dump_reg_memory(pid, regs.rcx);
    printf("rdx    : %016llx", regs.rdx);
    dump_reg_memory(pid, regs.rdx);
    printf("rsp    : %016llx", regs.rsp);
    dump_reg_memory(pid, regs.rsp);
    printf("rbp    : %016llx", regs.rbp);
    dump_reg_memory(pid, regs.rbp);
    printf("rsi    : %016llx", regs.rsi);
    dump_reg_memory(pid, regs.rsi);
    printf("rdi    : %016llx", regs.rdi);
    dump_reg_memory(pid, regs.rdi);
    printf("rip    : %016llx", regs.rip);
    dump_reg_memory(pid, regs.rip);
    printf("r8     : %016llx", regs.r8);
    dump_reg_memory(pid, regs.r8);
    printf("r9     : %016llx", regs.r9);
    dump_reg_memory(pid, regs.r9);
    printf("r10    : %016llx", regs.r10);
    dump_reg_memory(pid, regs.r10);
    printf("r11    : %016llx", regs.r11);
    dump_reg_memory(pid, regs.r11);
    printf("r12    : %016llx", regs.r12);
    dump_reg_memory(pid, regs.r12);
    printf("r13    : %016llx", regs.r13);
    dump_reg_memory(pid, regs.r13);
    printf("r14    : %016llx", regs.r14);
    dump_reg_memory(pid, regs.r14);
    printf("r15    : %016llx", regs.r15);
    dump_reg_memory(pid, regs.r15);
    printf("\neflags : %016llx\n", regs.eflags);
    printf("ss: %04x cs: %04x ds: %04x gs: %04x es: %04x fs: %04x\n",
        (uint32_t)regs.ss, (uint32_t)regs.cs, (uint32_t)regs.ds,
        (uint32_t)regs.gs, (uint32_t)regs.es, (uint32_t)regs.fs
    );
}

static void spawn_process(char **argv)
{
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);

	/* Stop before doing anything, giving parent a chance to catch the exec: */
    kill(getpid(), SIGSTOP);

    execve(argv[0], argv, environ);

    // execve only returns on failure
    err("Failed to execute binary");
    exit(1);
}

static estat_t status_type(int status)
{
    if (WIFSTOPPED(status)) {
        // Process crashed!
        if (WSTOPSIG(status) == SIGSEGV)
            return CRASHED;

        // Syscall trace
        if (WSTOPSIG(status) & 0x80)
            return SYSCALL;
    }

    // Process exited
    if (WIFEXITED(status))
        return EXITED;

    return UNKNOWN;
}

static int handle_syscall(pid_t pid, int entry)
{
    struct syshooks *pop;
    struct user_regs_struct regs;
    size_t i;

    memset(&regs, '\0', sizeof(regs));
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	printf("rax; %llx\n", regs.rax);
    pop = _hookops;
    for (i = 0; i < sizeof(_hookops) / sizeof(*pop); i++) {
        if (entry && regs.rax == pop->sysnum) {
            printf("derp\n");
            regs.rip = (uint64_t)pop->callback;
            ptrace(PTRACE_SETREGS, pid, NULL, &regs);
        }
    };

    return 0;
}

static int monitor_execution(pid_t pid)
{
    int isentry = 1;
    int ret = 1;
    int status, st;

    waitpid(pid, &status, 0);
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
    while (_continue) {
        ptrace(PTRACE_SYSCALL, pid, 0, 0);
        waitpid(pid, &status, 0);

        st = status_type(status);
        if (st == CRASHED) {
            info("Debuggee crashed!");
            return 0;
        }

        if (st == EXITED) {
            info("Debuggee exited");
            return 1;
        }

        if (st == SYSCALL) {
            display_crash_dump(pid);
            handle_syscall(pid, isentry);
        }

        if (isentry)
            isentry = 0;
        else
            isentry = 1;
    }

    return ret;
}

int main(int argc, char **argv)
{
    int ret = FAILURE;
    int pid;

    if (argc < 2) {
        printf("./flyr-crash [cmd]\n");
        goto done;
    }

    pid = fork();
    if (!pid) {
        // This won't return
        spawn_process(&argv[1]);
    } else {
        ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
        if (!monitor_execution(pid)) {
            info("Fuzzed process has crashed with SIGSEGV");
            display_crash_dump(pid);
        }

        ptrace(PTRACE_DETACH, pid, 0, 0);
    }
done:
    return ret;
}