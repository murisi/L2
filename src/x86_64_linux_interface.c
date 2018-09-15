//x86-64 Linux syscalls and constants
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_EXIT 60
#define SYS_CLOCK_GETTIME 228
#define	O_RDWR 02
#define	O_CREAT	0100
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
	unsigned long i;
	for(i = 0; src[i]; i++) {
		dest[i] = src[i];
	}
	dest[i] = '\0';
	return dest;
}

int strcmp(const char *s1, const char *s2) {
	unsigned long i;
	for(i = 0; s1[i] && s2[i] && (s1[i] == s2[i]); i++);
	return s1[i] - s2[i];
}

void *memcpy(void *dest, const void *src, unsigned long n) {
	unsigned long i;
	for(i = 0; i < n; i++) {
		((unsigned char *) dest)[i] = ((const unsigned char *) src)[i];
	}
	return dest;
}

int isspace(int c) {
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

int myopen(char *path) {
	return (int) syscall(SYS_OPEN, path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
}

void mywrite(int fd, void *d, int len) {
	syscall(SYS_WRITE, fd, d, len);
}

void mywrite_str(int fd, char *str) {
	mywrite(fd, str, strlen(str));
}

void mywrite_char(int fd, char ch) {
	mywrite(fd, &ch, 1);
}

void mywrite_ul(int fd, unsigned long i) {
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
	mywrite(fd, str+j+1, 19-j);
}

void mywrite_l(int fd, long i) {
	if(i < 0) {
		mywrite_char(fd, '-');
		mywrite_ul(fd, -((unsigned long) i));
	} else {
		mywrite_ul(fd, i);
	}
}

long int myread(int fd, void *buf, int cnt) {
	return syscall(SYS_READ, fd, buf, cnt);
}

void myclose(int fd) {
	syscall(SYS_CLOSE, fd);
}

long int mysize(char *path) {
	unsigned char buf[1024];
	long int file_size = 0, bytes_read;
	int fd = myopen(path);
	while(bytes_read = myread(fd, buf, sizeof(buf))) {
		file_size += bytes_read;
	}
	myclose(fd);
	return file_size;
}

void myexit(int status) {
	syscall(SYS_EXIT, status);
}

void *mymmap(unsigned long len) {
	return (void *) syscall(SYS_MMAP, NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0L);
}

void mymunmap(void *ptr, unsigned long len) {
	syscall(SYS_MUNMAP, ptr, len);
}

unsigned long round_size(unsigned long x, unsigned long nearest) {
	return x + (nearest - (x % nearest));
}

typedef void* region;

region create_region(unsigned long min_capacity) {
	unsigned long len = round_size(min_capacity + 5 * sizeof(void *), PAGE_SIZE);
	region reg = mymmap(len);
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
		mymunmap(reg, ((void **) reg)[3] - reg);
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
} myjmp_buf;

struct timer {
	long seconds;
	long nanoseconds;
};

void gettime(long *sec, long *nsec) {
	struct timer t;
	syscall(SYS_CLOCK_GETTIME, CLOCK_PROCESS_CPUTIME_ID, &t);
	*sec = t.seconds;
	*nsec = t.nanoseconds;
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
	gettime(&timer_i.seconds, &timer_i.nanoseconds); \
	stm; \
	gettime(&timer_f.seconds, &timer_f.nanoseconds); \
	timer_add(dest, timer_f); \
	timer_subtract(dest, timer_i); \
}

#define print_timer(src) {\
	mywrite_str(STDOUT, #src ": "); \
	mywrite_l(STDOUT, src.seconds); \
	mywrite_str(STDOUT, "s "); \
	mywrite_l(STDOUT, src.nanoseconds); \
	mywrite_str(STDOUT, "ns\n"); \
}
