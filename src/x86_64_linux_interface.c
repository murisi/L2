//x86-64 Linux syscalls and constants
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define	O_RDWR 02
#define	O_CREAT	0100
#define S_IRWXU 00700
#define S_IRWXG 00070
#define S_IRWXO 00007

#define SEEK_SET 0	/* Seek from beginning of file.  */
#define SEEK_CUR 1	/* Seek from current position.  */
#define SEEK_END 2	/* Seek from end of file.  */
#define STDIN 0
#define STDOUT 1

int mystrlen(char *str) {
	int i;
	for(i = 0; str[i]; i++);
	return i;
}

int myopen(char *path) {
	return csyscall(SYS_OPEN, path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
}

void mywrite(int fd, void *d, int len) {
	csyscall(SYS_WRITE, fd, d, len);
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

int myread(int fd, void *buf, int cnt) {
	return csyscall(SYS_READ, fd, buf, cnt);
}

void myseek(int fd, long int offset, int whence) {
	csyscall(SYS_LSEEK, fd, offset, whence);
}

void myclose(int fd) {
	csyscall(SYS_CLOSE, fd);
}
