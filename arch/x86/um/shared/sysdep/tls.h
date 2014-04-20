#ifndef _SYSDEP_TLS_H
#define _SYSDEP_TLS_H

# ifndef __KERNEL__


typedef struct um_dup_user_desc {
	unsigned int  entry_number;
	unsigned int  base_addr;
	unsigned int  limit;
	unsigned int  seg_32bit:1;
	unsigned int  contents:2;
	unsigned int  read_exec_only:1;
	unsigned int  limit_in_pages:1;
	unsigned int  seg_not_present:1;
	unsigned int  useable:1;
#ifdef __x86_64__
	unsigned int  lm:1;
#endif
} user_desc_t;

# else 

typedef struct user_desc user_desc_t;

# endif 

extern int os_set_thread_area(user_desc_t *info, int pid);
extern int os_get_thread_area(user_desc_t *info, int pid);

#ifdef __i386__
#define GDT_ENTRY_TLS_MIN_I386 6
#define GDT_ENTRY_TLS_MIN_X86_64 12
#endif

#endif 
