//x86-64 Linux syscalls and constants
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_MMAP 9
#define SYS_MUNMAP 11
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

#define STDIN 0
#define STDOUT 1

long int mysyscall(long int syscall_num, ...);

int mystrlen(char *str) {
	int i;
	for(i = 0; str[i]; i++);
	return i;
}

int myopen(char *path) {
	return (int) mysyscall(SYS_OPEN, path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
}

void mywrite(int fd, void *d, int len) {
	mysyscall(SYS_WRITE, fd, d, len);
}

void mywrite_str(int fd, char *str) {
	mywrite(fd, str, mystrlen(str));
}

void mywrite_char(int fd, char ch) {
	mywrite(fd, &ch, 1);
}

void mywrite_uint(int fd, unsigned int i) {
	char str[10]; int j;
	for(j = 9; i; j--, i/=10) {
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
	mywrite(fd, str+j+1, 9-j);
}

void mywrite_int(int fd, int i) {
	if(i < 0) {
		mywrite_char(fd, '-');
		mywrite_uint(fd, -((unsigned int) i));
	} else {
		mywrite_uint(fd, i);
	}
}

long int myread(int fd, void *buf, int cnt) {
	return mysyscall(SYS_READ, fd, buf, cnt);
}

void myclose(int fd) {
	mysyscall(SYS_CLOSE, fd);
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

void *mymmap(unsigned long len) {
	return (void *) mysyscall(SYS_MMAP, NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0L);
}

void mymunmap(void *ptr, unsigned long len) {
	mysyscall(SYS_MUNMAP, ptr, len);
}

int round_size(unsigned long x, unsigned long nearest) {
	return x + (nearest - (x % nearest));
}

typedef void* region;

region create_region(unsigned long min_capacity) {
	unsigned long len = round_size(min_capacity + 3 * sizeof(void *), PAGE_SIZE);
	region reg = mymmap(len);
	((void **) reg)[0] = NULL;
	((void **) reg)[1] = ((void **) reg) + 3;
	((void **) reg)[2] = reg + len;
	return reg;
}

#define ALIGNMENT 8

void *region_malloc(region reg, unsigned long len) {
	len = round_size(len, ALIGNMENT);
	while(((void **) reg)[1] + len > ((void **) reg)[2]) {
		if(!((void **) reg)[0]) {
			((void **) reg)[0] = create_region(len);
		}
		reg = ((void **) reg)[0];
	}
	void *mem = ((void **) reg)[1];
	((void **) reg)[1] += len;
	return mem;
}

void destroy_region(region reg) {
	do {
		region next_reg = ((void **) reg)[0];
		mymunmap(reg, ((void **) reg)[2] - reg);
		reg = next_reg;
	} while(reg);
}
