//x86-64 Linux syscalls and constants
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_FSTAT 5
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_EXIT 60
#define SYS_CLOCK_GETTIME 228
#define O_RDWR 02
#define O_CREAT	0100
#define S_IRWXU 00700
#define S_IRWXG 00070
#define S_IRWXO 00007
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define PROT_NONE 0x0
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define PAGE_SIZE 4096
#define CLOCK_PROCESS_CPUTIME_ID 2

#define STDIN 0
#define STDOUT 1

long int syscall(long int syscall_num, ...);

unsigned long strlen(const char *str) {
	unsigned long i;
	for(i = 0; str[i]; i++);
	return i;
}

char *strrchr(const char *s, int c) {
	const char *t = s + strlen(s);
	for(; t >= s; t--) {
		if(*t == c) {
			return (char *) t;
		}
	}
	return NULL;
}

char *strcpy(char *dest, const char *src) {
	char *odest = dest;
	for(; *src; src++, dest++) {
		*dest = *src;
	}
	*dest = '\0';
	return odest;
}

int strcmp(const char *s1, const char *s2) {
	unsigned long i;
	for(; *s1 && *s2 && (*s1 == *s2); s1++, s2++);
	return *s1 - *s2;
}

void *memcpy(void *dest, const void *src, unsigned long n) {
	char *odest = dest;
	for(; n; dest++, src++, n--) {
		*((unsigned char *) dest) = *((const unsigned char *) src);
	}
	return odest;
}

void *memset(void *s, int c, unsigned long n) {
	for(; n; s++, n--) {
		*((unsigned char *) s) = c;
	}
}

int isspace(int c) {
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

int open(char *path) {
	return (int) syscall(SYS_OPEN, path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
}

void write(int fd, void *d, int len) {
	syscall(SYS_WRITE, fd, d, len);
}

void write_str(int fd, char *str) {
	write(fd, str, strlen(str));
}

void write_char(int fd, char ch) {
	write(fd, &ch, 1);
}

void write_ulong(int fd, unsigned long i) {
	char str[20]; int j;
	for(j = 19; i; j--, i/=10) {
		switch(i % 10) {
			case 0: str[j] = '0'; break;
			case 1: str[j] = '1'; break;
			case 2: str[j] = '2'; break;
			case 3: str[j] = '3'; break;
			case 4: str[j] = '4'; break;
			case 5: str[j] = '5'; break;
			case 6: str[j] = '6'; break;
			case 7: str[j] = '7'; break;
			case 8: str[j] = '8'; break;
			case 9: str[j] = '9'; break;
		}
	}
	write(fd, str+j+1, 19-j);
}

void write_long(int fd, long i) {
	if(i < 0) {
		write_char(fd, '-');
		write_ulong(fd, -((unsigned long) i));
	} else {
		write_ulong(fd, i);
	}
}

long int read(int fd, void *buf, int cnt) {
	return syscall(SYS_READ, fd, buf, cnt);
}

void close(int fd) {
	syscall(SYS_CLOSE, fd);
}

long int size(int fd) {
	long int statbuf[18];
	syscall(SYS_FSTAT, fd, statbuf);
	return statbuf[6];
}

void *mmap(unsigned long len) {
	return (void *) syscall(SYS_MMAP, NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0L);
}

void munmap(void *ptr, unsigned long len) {
	syscall(SYS_MUNMAP, ptr, len);
}

unsigned long round_size(unsigned long x, unsigned long nearest) {
	return x + (nearest - (x % nearest));
}

typedef void* region;

region create_region(unsigned long min_capacity) {
	unsigned long len = round_size(min_capacity + 5 * sizeof(void *), PAGE_SIZE);
	region reg = mmap(len);
	((void **) reg)[0] = NULL;
	((void **) reg)[1] = reg;
	((void **) reg)[2] = ((void **) reg) + 5;
	((void **) reg)[3] = reg + len;
	((void **) reg)[4] = (void *) 0xDEADBEEFDEADBEEFUL;
	return reg;
}

#define ALIGNMENT 8

void check_region_integrity(region reg) {
	do {
		if(((void **) reg)[4] != (void *) 0xDEADBEEFDEADBEEFUL) {
			*((void **) NULL) = NULL;
			return;
		}
		reg = ((void **) reg)[0];
	} while(reg);
}

void *region_alloc(region reg, unsigned long len) {
	//check_region_integrity(reg);
	
	len = round_size(len, ALIGNMENT);
	if(((void ***) reg)[1][2] + len > ((void ***) reg)[1][3]) {
		((void **) reg)[1] = ((void ***) reg)[1][0] = create_region(len + (2*(((void ***) reg)[1][3] - ((void **) reg)[1])));
	}
	void *mem = ((void ***) reg)[1][2];
	((void ***) reg)[1][2] += len;
	return mem;
}

void destroy_region(region reg) {
	//check_region_integrity(reg);
	
	do {
		region next_reg = ((void **) reg)[0];
		munmap(reg, ((void **) reg)[3] - reg);
		reg = next_reg;
	} while(reg);
}

char *rstrcpy(const char *src, region reg) {
	char *dest = region_alloc(reg, strlen(src) + 1);
	unsigned long i;
	for(i = 0; src[i]; i++) {
		dest[i] = src[i];
	}
	dest[i] = '\0';
	return dest;
}

typedef struct {
	void *rbp;
	void *cir;
	void *rsi;
	void *r14;
	void *r13;
	void *rbx;
	void *r12;
	void *r15;
	void *rsp;
	void *ctx; //For data that you want to transfer through jumps
} jumpbuf;

struct timer {
	long seconds;
	long nanoseconds;
};

void gettime(struct timer *t) {
	syscall(SYS_CLOCK_GETTIME, CLOCK_PROCESS_CPUTIME_ID, t);
}

#define timer_reset(tmr) {\
	tmr.seconds = 0; \
	tmr.nanoseconds = 0; \
}

#define mod(a, b) (((a) % (b)) < 0 ? (((a) % (b)) + (b)) : ((a) % (b)))

#define timer_add(dest, src) { \
	dest.seconds = dest.seconds + src.seconds + (dest.nanoseconds + src.nanoseconds >= 1000000000L ? 1 : 0); \
	dest.nanoseconds = mod(dest.nanoseconds + src.nanoseconds, 1000000000L); \
}

#define timer_subtract(dest, src) { \
	dest.seconds = (dest.seconds - src.seconds) + (dest.nanoseconds - src.nanoseconds < 0 ? -1 : 0); \
	dest.nanoseconds = mod(dest.nanoseconds - src.nanoseconds, 1000000000L); \
}

#define timer_time(dest, stm) {\
	struct timer timer_i, timer_f; \
	gettime(&timer_i); \
	stm; \
	gettime(&timer_f); \
	timer_add(dest, timer_f); \
	timer_subtract(dest, timer_i); \
}

#define print_timer(src) {\
	write_str(STDOUT, #src ": "); \
	write_long(STDOUT, src.seconds); \
	write_str(STDOUT, "s "); \
	write_long(STDOUT, src.nanoseconds); \
	write_str(STDOUT, "ns\n"); \
}
