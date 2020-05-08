#define PCU_UNIT_COUNT          1
struct mdlwp {
        void *md_tf;
        int     md_flags;
};
typedef volatile const void *wchan_t;
typedef struct callout {
        void    *_c_store[10];
} callout_t;
struct lwp {
        /* Scheduling and overall state. */
        struct {
            void *tqe_next;                /* next element */
            void **tqe_prev;  /* address of previous next element */
        } l_runq;       /* s: run queue */
        union {
                void *  info;           /* s: scheduler-specific structure */
                u_int   timeslice;      /* l: time-quantum for SCHED_M2 */
        } l_sched;
        void * volatile l_cpu;/* s: CPU we're on if LSONPROC */
        void * volatile l_mutex;        /* l: ptr to mutex on sched state */
        int             l_ctxswtch;     /* l: performing a context switch */
        void            *l_addr;        /* l: PCB address; use lwp_getpcb() */
        struct mdlwp    l_md;           /* l: machine-dependent fields. */
        int             l_flag;         /* l: misc flag values */
        int             l_stat;         /* l: overall LWP status */
        struct bintime  l_rtime;        /* l: real time */
        struct bintime  l_stime;        /* l: start time (while ONPROC) */
        u_int           l_swtime;       /* l: time swapped in or out */
        u_int           l_rticks;       /* l: Saved start time of run */
        u_int           l_rticksum;     /* l: Sum of ticks spent running */
        u_int           l_slpticks;     /* l: Saved start time of sleep */
        u_int           l_slpticksum;   /* l: Sum of ticks spent sleeping */
        int             l_biglocks;     /* l: biglock count before sleep */
        int             l_class;        /* l: scheduling class */
        int             l_kpriority;    /* !: has kernel priority boost */
        pri_t           l_kpribase;     /* !: kernel priority base level */
        pri_t           l_priority;     /* l: scheduler priority */
        pri_t           l_inheritedprio;/* l: inherited priority */

        struct {
            void *slh_first;    /* first element */
        } l_pi_lenders; /* l: ts lending us priority */
        uint64_t        l_ncsw;         /* l: total context switches */
        uint64_t        l_nivcsw;       /* l: involuntary context switches */
        u_int           l_cpticks;      /* (: Ticks of CPU time */
        fixpt_t         l_pctcpu;       /* p: %cpu during l_swtime */
        fixpt_t         l_estcpu;       /* l: cpu time for SCHED_4BSD */
        psetid_t        l_psid;         /* l: assigned processor-set ID */
        void *l_target_cpu;     /* l: target CPU to migrate */
        void    *l_lwpctl;      /* p: lwpctl block kernel address */
        void    *l_lcpage;      /* p: lwpctl containing page */
        void    *l_affinity;    /* l: CPU set for affinity */

        /* Synchronisation. */
        void *l_ts;             /* l: current turnstile */
        void  *l_syncobj;     /* l: sync object operations set */
        struct {
            void *tqe_next;             /* next element */
            void **tqe_prev;    /* address of previous next element */
        }  l_sleepchain;  /* l: sleep queue */
        wchan_t         l_wchan;        /* l: sleep address */
        const char      *l_wmesg;       /* l: reason for sleep */
        void   *l_sleepq;      /* l: current sleep queue */
        int             l_sleeperr;     /* !: error before unblock */
        u_int           l_slptime;      /* l: time since last blocked */
        callout_t       l_timeout_ch;   /* !: callout for tsleep */
        u_int           l_emap_gen;     /* !: emap generation number */
        kcondvar_t      l_waitcv;

#if PCU_UNIT_COUNT > 0
        void * volatile l_pcu_cpu[PCU_UNIT_COUNT];
        uint32_t        l_pcu_used;
#endif

        /* Process level and global state, misc. */
        struct {
            void *le_next;      /* next element */
            void **le_prev;     /* address of previous next element */
        } l_list;         /* a: entry on list of all LWPs */
        void            *l_ctxlink;     /* p: uc_link {get,set}context */
        void     *l_proc;        /* p: parent process */
};

struct lwplist {
        void *lh_first;  /* first element */
};



typedef struct krwlock krwlock_t;

struct proc {
        struct {
            void *le_next;      /* next element */
            void **le_prev;     /* address of previous next element */
        } p_list;       /* l: List of all processes */

        kmutex_t        p_auxlock;      /* :: secondary, longer term lock */
        void            *p_lock;        /* :: general mutex */
        kmutex_t        p_stmutex;      /* :: mutex on profiling state */
        krwlock_t       p_reflock;      /* p: lock for debugger, procfs */
        kcondvar_t      p_waitcv;       /* p: wait, stop CV on children */
        kcondvar_t      p_lwpcv;        /* p: wait, stop CV on LWPs */

        /* Substructures: */
        void *p_cred;   /* p: Master copy of credentials */
        void    *p_fd;          /* :: Ptr to open files structure */
        void    *p_cwdi;        /* :: cdir/rdir/cmask info */
        void    *p_stats;       /* :: Accounting/stats (PROC ONLY) */
        void    *p_limit;       /* :: Process limits */
        void    *p_vmspace;     /* :: Address space */
        void    *p_sigacts;     /* :: Process sigactions */
        void    *p_aio;         /* p: Asynchronous I/O data */
        u_int           p_mqueue_cnt;   /* (: Count of open message queues */
        specificdata_reference
                        p_specdataref;  /*    subsystem proc-specific data */

        int             p_exitsig;      /* l: signal to send to parent on exit */
        int             p_flag;         /* p: PK_* flags */
        int             p_sflag;        /* p: PS_* flags */
        int             p_slflag;       /* s, l: PSL_* flags */
        int             p_lflag;        /* l: PL_* flags */
        int             p_stflag;       /* t: PST_* flags */
        char            p_stat;         /* p: S* process status. */
        char            p_trace_enabled;/* p: cached by syscall_intern() */
        char            p_pad1[2];      /*  unused */

        pid_t           p_pid;          /* :: Process identifier. */
};
typedef struct rb_node {
        void *rb_nodes[2];
        /*
         * rb_info contains the two flags and the parent back pointer.
         * We put the two flags in the low two bits since we know that
         * rb_node will have an alignment of 4 or 8 bytes.
         */
        uintptr_t rb_info;
} rb_node_t;
typedef struct rb_tree {
        void *rbt_root;
        void *rbt_ops;
        void *rbt_minmax[2];
} rb_tree_t;
typedef off_t voff_t;
typedef int vm_inherit_t;
typedef int             vm_prot_t;
typedef unsigned long   vaddr_t;
typedef unsigned long   vsize_t;
struct vm_aref {
        int ar_pageoff;                 /* page offset into amap we start */
        struct vm_amap *ar_amap;        /* pointer to amap */
};


struct vm_map_entry {
        struct rb_node          rb_node;        /* tree information */
        vsize_t                 gap;            /* free space after */
        vsize_t                 maxgap;         /* space in subtree */
        void    *prev;          /* previous entry */
        void    *next;          /* next entry */
        vaddr_t                 start;          /* start address */
        vaddr_t                 end;            /* end address */
        union {
                void *uvm_obj;  /* uvm object */
                void    *sub_map;       /* belongs to another map */
        } object;                               /* object I point to */
        voff_t                  offset;         /* offset into object */
        int                     etype;          /* entry type */
        vm_prot_t               protection;     /* protection code */
        vm_prot_t               max_protection; /* maximum protection */
        vm_inherit_t            inheritance;    /* inheritance */
        int                     wired_count;    /* can be paged if == 0 */
        struct vm_aref          aref;           /* anonymous overlay */
};

struct vm_map {
        void *          pmap;           /* Physical map */
        krwlock_t               lock;           /* Non-intrsafe lock */
        void *          busy;           /* LWP holding map busy */
        kmutex_t                misc_lock;      /* Lock for ref_count, cv */
        kcondvar_t              cv;             /* For signalling */
        int                     flags;          /* flags */
        struct rb_tree          rb_tree;        /* Tree for entries */
        struct vm_map_entry     header;         /* List of entries */
};

struct vmspace {
        struct  vm_map vm_map;  /* VM address map */
};
struct vm_amap {
        void *am_lock;  /* lock [locks all vm_amap fields] */
        int am_ref;             /* reference count */
        int am_flags;           /* flags */
        int am_maxslot;         /* max # of slots allocated */
        int am_nslot;           /* # of slots currently in map ( <= maxslot) */
        int am_nused;           /* # of slots currently in use */
        int *am_slots;          /* contig array of active slots */
        int *am_bckptr;         /* back pointer array to am_slots */
        void **am_anon; /* array of anonymous pages */
};

struct vm_anon {
        void            *an_lock;       /* Lock for an_ref */
        union {
                uintptr_t       au_ref;         /* Reference count [an_lock] */
                void    *au_link;       /* Link for deferred free */
        } an_u;
        void            *an_page;       /* If in RAM [an_lock] */
        int                     an_swslot;
};