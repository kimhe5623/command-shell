#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <utime.h>

#define SZ_STR_BUF 256
#define SZ_FILE_BUF 1024

// get_argv_optv() 함수에서 상세히 설명함
char *cmd;
char *argv[100];
char *optv[10];
int argc, optc;
char cur_work_dir[SZ_STR_BUF]; // 현재 작업 디렉토리 이름을 저장하는 버퍼
// char *PRINT_ERR_RET()을 호출했다면 아래 return은
//	cp()함수에서 리턴한다는 의미임/
//--------------------------------------------------------------------------

// 에러 원인을 화면에 출력하고 이 매크로를 사용하는 해당 함수에서 리턴함
/* cmdrk "ls"라면 "ls: 에러원인" 형태로 출력됨 */

#define PRINT_ERR_RET() \
do{ \
	perror(cmd); \
	return; \
} while (0)

#define EQUAL(_s1, _s2)		(strcmp(_s1, _s2) == 0) // 두개 문자열 같으면 true
#define NOT_EQUAL(_s1, _s2)	(strcmp(_s1, _s2) != 0) // 두개 문자열 다르면 true

// 명령어 사용법을 출력함
// help()와 proc_cmd()에서 호출됨
static void
print_usage(char *msg, char *cmd, char *opt, char *arg)
{
	// proc_cmd()에서 msg로 "사용법:"을,
	// help()	 에서 msg로 "	" 을 넘겨줌
	printf("%s%s", msg, cmd);	// "사용법: ls"
	if (NOT_EQUAL(opt, ""))		// 옵션이 있으면
		printf("  %s", opt);	// "-l "
	printf("  %s\n", arg);		// "[디렉토리 이름]"
	// 최종 출력 예) "사용법: ls -l [디렉토리 이름]\n"
}

//*******************************************************************
// 명령어 옵션 및 인자 개수가 정확한지 체크하는 작업을 수행함
//*******************************************************************

// [명령어 인자의 개수]가 정확한지 체크함
// argc: 사용자가 입력한 명령어 인자의 수
// count: 그 명령어가 필요로 하는 인자의 수
// 리턴값: 인자개수가 정확하면 0, 틀렸으면 -1
static int
check_arg(int count)
{
	if(count < 0) { // AC_ANY(-1), AC_LESS_1(-100)인 경우
		// 인자 개수 가변 : 인자를 주어도 되고 안주어도 되는 경우
		count = -count; // 음수를 양수로 변환
		if (argc <= count) // 입력된 인자 개수(argc)가 최대로 가능한
			return(0);	   // 개수(count)보다 같거나 적으면 OK
	}
	if (argc == count)
		return(0);
	if (argc > count)
		printf("불필요한 명령어 인자가 있습니다.\n");
	else // (argc < count)
		printf("명령어 인자의 수가 부족합니다.\n");
	return(-1);
}

// 명령어에서 정확한 [옵션]이 주어졌는지 체크함
// optc: 사용자가 입력한 옵션 개수
// optv[i]: 사용자가 입력한 옵션 문자열
// opt: 그 명령어가 필요로 하는 옵션 문자열
// 리턴값: 옵션이 없으면 0, 있으면 1, 옵션이 틀렸으면 -1
static int
check_opt(char *opt)
{
	int i, err = 0;

	for(i=0 ; i<optc ; ++i) {
		if(NOT_EQUAL(opt, optv[i])) {
			printf("지원되지 않는 명령어 옵션(%s)입니다.\n", optv[i]);
			err = -1;
			}
	}
	return(err);
}

//********************************************************************************
// get_argv_optv(): 명령어 한줄을 명령어, 옵션, 명령인자와 같이 각 단어별로 분리
//********************************************************************************
//
// 전역변수
// char *cmd;		// 명령어 문자열
// char *argv[100]; // 명령어 인자 문자열들 시작주소 저장
// char *optv[10];	// 명령어 옵션 문자열들 시작주소 저장
// int optc;		// 옵션 토큰 수, 즉, optv[]에 저장된 원소의 개수
//					// 즉, argv[]에 저장된 원소의 개수

// 입력된 명령어 행 전체를 저장하고 있는 cmd_line[]에서
// 각 토큰(단어)을 별개의 문자열로 자른 후, 그 토큰의 시작주소를
// 명령어일 경우, cmd에, 옵션일 경우 optv[]에,
// 명령어 인자인 경우 argv[]에 각각 순서적으로 저장한다.

// 예제: get_argv_optv(cmd_line)를 호출하여 리턴한 후의 각 변수의 값
//
// cmd_line[]에 "ls -l pr4"가 저장되어 있을 경우
// cmd -> "ls"
// optc = 1, optv[0] -> "-l";
// argc = 1, argv[0] -> "pr4";

// cmd_line[]에 "ln -s f1 f2" 가 저장되어 있을 경우
// cmd -> "ln"
// optc = 1, optv[0] -> "-s"
// argc = 2, argv[0] -> "f1, argv[1] -> "f1";

// cmd_line[]에 "ln f1 f2"가 저장되어 있을 경우
// cmd -> "ln"
// optc = 0;
// argc = 2, argc[0] -> "f1", argv[1] -> "f1";

// cmd_line[]에 "pwd"가 저장되어 있을 경우
// cmd -> "pwd"
// optc = 0;
// argc = 0;

// cmd, optv[], argv[]에 저자되는 것은 각 문자열의 시작주소,
// 즉, 각 문자열의 첫 글자의 주소가 저장됨
// 따라서 각 문자여른 여전히 cmd_line[]에 저장되어 있음

static char *
get_argv_optv(char *cmd_line)
{
	// 사용자가 키보드에서 입력한 명령어 한 줄이 cmd_line[]에 저장되어 있음
	// cmd_line[]ㅔ "ln -s file1 file2"가 저장되어 있다고 가정하자.
	//
	// strtok()의 두번째 인자인 " \t\n"는 토큰(단어)을 구분하는 구분자임.
	// 즉, 스페이스 ' ', 탭 '\t', 엔터 '\n' 글자가 나오면 토큰을 자름.
	// strtok()함수는 cmd_line[]에 있는 문자열(전체가 하나의 문자열)에서
	// 첫 단어 "ln"을 찾아 단어의 끝에 null문자('\0')를 삽입해
	// 단어를 하나의 문자열로 만들고(단어를 자른다고 표현함),
	// 그 문자열의 첫 글자 주소를 리턴함
	// 그 문자열의 첫 글자 주소를 리턴함.
	// 첫 단어를 잘라내기 위해 strtok()호출 시 첫 인자로 cmd_line을 주고,
	// 다음에 또 호출할 땐 첫 인자로 NULL을 주는데,
	// 이 경우 앞에서 처리한 단어의 그 다음 단어를 찾아 자른다.
	// 만약 함수 호출 시 더 이상 처리할 단어가 없다면 NULL을 리턴함.
	//
	// 결국 함수 호출 [후]에는 cmd_line[]가 "ln\0-s\0file1\0file2"로 변함
	// '\0'는 null문자(한 글자임)를 표현한 것이고,
	// 메모리에는 ASCII 코드 0(정수값)이 한 byte로 저장되어 있음
	// 모든 문자열 끝에는 이 문자가 있음(함수들이 자동으로 삽입해 줌)
	// 즉, cmd_line[]에는 네개의 문자열이 저장되어 있고,
	// 각 문자열의 시작주소는 optv[]와 argv[]에 분리되어 저장되어 있음

	char *tok; // 잘라낸 토큰(단어) 문자열의 시작주소(첫 글자의 주소)

	argc = optc = 0;

	if((cmd = strtok(cmd_line, " \t\n")) == NULL) // 첫 토큰(단어) 잘라냄
		return(NULL); // 명령어 입력없이 그냥 엔터만 친 경우
	// cmd는 명령 문자열의 시작주소임. 예) "ln"

	for(; (tok = strtok(NULL, " \t\n")) != NULL;)
	{
		// 다음 토큰 잘라냄, tok[0] : 잘라낸 토큰의 첫글자
		if (tok[0] == '-') // 토큰이 옵션임을 의미. 예) optv[0]->"-s"
			optv[optc++] = tok; // optv[optc]에 토큰 주소 저장하고 optc++
		else // 토큰이 명령어 인자임을 의미
			argv[argc++] = tok; // argv[argc]에 토큰 주소 저장하고 argc++
	}
	return(cmd);
	// 이 함수가 리턴해도 각 문자열은 여전히 cmd_line[]에 저장되어 있고
	// 각 문자열의 시작주소만 cmd, optv[], argc[]에 저장되어 있음
}

static void
print_attr(char *path, char *fn)
{
	struct passwd *pwp;
	struct group *grp;
	struct stat st_buf;
	char full_path[SZ_STR_BUF], buf[SZ_STR_BUF], c;
	char time_buf[13];
	struct tm *tmp;

	sprintf(full_path, "%s/%s", path, fn);
	if (lstat(full_path, &st_buf) < 0)
		PRINT_ERR_RET();

	if		(S_ISREG(st_buf.st_mode))	c = '-';
	else if	(S_ISDIR(st_buf.st_mode))	c = 'd';
	else if	(S_ISCHR(st_buf.st_mode))	c = 'c';
	else if	(S_ISBLK(st_buf.st_mode))	c = 'b';
	else if	(S_ISFIFO(st_buf.st_mode))	c = 'f';
	else if	(S_ISLNK(st_buf.st_mode))	c = 'l';
	else if	(S_ISSOCK(st_buf.st_mode))	c = 's';
	buf[0] = c;
	buf[1] = (st_buf.st_mode & S_IRUSR)? 'r': '-';
	buf[2] = (st_buf.st_mode & S_IWUSR)? 'w': '-';
	buf[3] = (st_buf.st_mode & S_IXUSR)? 'x': '-';
	buf[4] = (st_buf.st_mode & S_IRGRP)? 'r': '-';
	buf[5] = (st_buf.st_mode & S_IWGRP)? 'w': '-';
	buf[6] = (st_buf.st_mode & S_IXGRP)? 'x': '-';
	buf[7] = (st_buf.st_mode & S_IROTH)? 'r': '-';
	buf[8] = (st_buf.st_mode & S_IWOTH)? 'w': '-';
	buf[9] = (st_buf.st_mode & S_IXOTH)? 'x': '-';
	buf[10] = '\0';
	// printf("%s", buf);
	pwp = getpwuid(st_buf.st_uid);
	grp = getgrgid(st_buf.st_gid);
	tmp = localtime(&st_buf.st_mtime);
	strftime(time_buf, 13, "%b %d %H:%M", tmp);
	sprintf(buf+10, " %3ld %-8s %-8s %8ld %s %s",
		st_buf.st_nlink, pwp->pw_name, grp->gr_name,
		st_buf.st_size, time_buf, fn);
	if (S_ISLNK(st_buf.st_mode)) {
		int len, bytes;
		strcat(buf, "-> ");
		len = strlen(buf);
		bytes = readlink(full_path, buf+len, SZ_STR_BUF-len);
		buf[len+bytes] = '\0';
	}
	printf("%s\n", buf);
}

static void
print_detail(DIR *dp, char *path)
{
	struct dirent *dirp;

	while ((dirp = readdir(dp)) != NULL)
		print_attr(path, dirp->d_name);
	closedir(dp);
}

static void
get_max_name_len(DIR *dp, int *p_max_name_len, int *p_num_per_line)
{
	struct dirent *dirp;
	int max_name_len = 0; // 가장 긴 파일이름 길이

	// 모든 파일 이름을 읽어 내 가장 긴 파일이름 길이

	while ((dirp = readdir(dp)) != NULL) {
		int name_len = strlen(dirp->d_name);
		if (name_len > max_name_len)
			max_name_len = name_len;
	}
	// 디렉토리 읽는 위치 처음으로 되돌리기
	rewinddir(dp);

	// 가장 긴 파일이름 + 이름 뒤의 여유공간
	max_name_len += 4;

	// 한 줄에 출력할 파일이름의 개수 결정
	*p_num_per_line = 80 / max_name_len;
	*p_max_name_len = max_name_len;
}

static void
print_name(DIR *dp)
{
	struct dirent *dirp;
	int max_name_len, num_per_line, cnt = 0;

	// max_name_len: 가장 긴 파일이름 길이 + 4(이름 뒤의 여유공간)
	// num_per_line: 한 줄에 출력 가능한 총 파일이름의 개수
	get_max_name_len(dp, &max_name_len, &num_per_line);

	while ((dirp = readdir(dp)) != NULL) {
		printf("%-*s", max_name_len, dirp->d_name);
		// cmd: 현재까지 한 줄에 출력한 파일이름 개수
		// 한 줄에 출력할 수 있는 파일이름의 개수만큼
		// 이미 출력 했으면 줄 바꾸기
		if ((++cnt % num_per_line) == 0)
			printf("\n");
	}
	// 마지막 줄에 줄 바꾸기 문자 출력
	// 앞에서 이미 한 줄에 출력할 수 있는 파일이름의 개수만큼 이미 출력하여
	// 줄 바꾸기를 했으면 또 다시 줄 바꾸기 하지 않음
	if ((cnt % num_per_line) != 0)
		printf("\n");
}	

//****************************************************************************
//
//	여기서부터 각 명령어 구현 시작
//
//

// 파일 내용을 화면에 보여주는 명령어
// 사용법: cat 파일이름
// argv[0] -> "파일이름"
void
cat(void)
{
	int fd, len;
	char buf[SZ_FILE_BUF];

	if((fd=open(argv[0], O_RDONLY)) < 0)
		PRINT_ERR_RET();
	while((len=read(fd, buf, SZ_FILE_BUF)) > 0) {
		if(len != (write(STDOUT_FILENO, buf, SZ_FILE_BUF))) {
			len = -1;
			break;
		}
	}
	if(len == -1)
		perror(cmd);
	close(fd);
}

// argv[0] -> "원본파일이름"
// argv[1] -> "복사된파일이름"

// 현재 작업 디렉토리를 변경하는 명령어
// 사용법: cd [디렉토리 이름]
// 			[]는 명령어 인자를 주어도 되고 안 주어도 됨을 의미함
// argv[0] -> "디렉토리이름"; [디렉토리 이름]을 준 경우
// argc = [디렉토리 이름]을 준 경우 1, 주지 않았을 경우 0
void
cd(void)
{
	struct passwd *pwp = getpwuid(getuid());
	if(argc == 0)	// 만약 인자가 주어졌다면 argc != 0.
		argv[0] = pwp->pw_dir;	
	if(chdir(argv[0]) < 0)
		PRINT_ERR_RET();	// 에러 발생
	else					// argc != 0
		getcwd(cur_work_dir, SZ_STR_BUF);
}

// 파일의 접근권한을 변경하는 명령어
// 사용법: chmod 8진수모드값 파일이름
// argv[0] -> "8진수모드값"
// argv[1] -> "파일이름"
void
changemod(void)
{
	int mode;	// "%o", 영어 소문자 o임(8잔수 octal의 약어)
	sscanf(argv[0], "%o", &mode);
	if(chmod(argv[1], mode) < 0)
		PRINT_ERR_RET();

}
void
cp(void)
{
	int rfd, wfd, len;
	char buf[SZ_FILE_BUF];
	struct stat st_buf;

	if((stat(argv[0], &st_buf) < 0) ||
		((rfd = open(argv[0], O_RDONLY))< 0))
		PRINT_ERR_RET();

	if((wfd = creat(argv[1], st_buf.st_mode))< 0) {
			perror(cmd);
			close(rfd);
			return;
	}
	while((len = read(rfd, buf, SZ_FILE_BUF)) > 0) {
		if(len != (write(wfd, buf, len))){
			len = -1;
			break;
		}
	}
	if(len < 0)
		perror(cmd);
	close(rfd);
	close(wfd);
}

// 오늘 현재의 "년월일시분초"를 보여주는 명령어
// 사용법: date
void
date(void)
{
	char stm[128];
	time_t ttm = time(NULL); // time함수는 반환형이 time,
			       // Calender time을 커널에서 가져오는 함수.
			       // time_t는 unsigned int 의 새로운 정의 형태

	struct tm *ltm = localtime(&ttm); // localtime함수는 Calender time을
				    // broken-down time(tm구조체)로
				    // 변환시켜주는 함수	

	printf("atm: %s",asctime(ltm));	// asctime함수는 tm구조체를 문자열 형태로
				// 변환시켜주는 함수

	printf("ctm: %s",ctime(&ttm));	// Calender time을 바로 문자열로
				// 변환시켜주는 함수

	strftime(stm, 128, "stm: %a %b %d %H:%M:%S %Y", ltm);
										 // strftime은 시간 값을 print해주는 함수
										 // format한 결과를 배열 인자에 저장.
										 // stm에 포맷 결과 저장.

	puts(stm);	// stm 출력

}
// echo 다음에 입력된 문자열을 화면에 바로 echo해 주는 명령어
// 사용법: echo [echo할 문자열 입력: 길이 제한 없음]
//   예) echo I love you. 
// argv[0] -> "I"
// argv[1] -> "love"
// argv[2] -> "you."
// argc = 3; 에코할 문장의 총 토큰(단어)의 수
void
echo(void)
{
	int i;
	for(i=0;i<argc;i++)
	{
		printf("%s ",argv[i]);
	}
	printf("\n");
}

// 현 컴퓨터의 이름을 출력해 주는 명령어
// 사용법: hostname
void
hostname(void)
{
	char hostname[SZ_STR_BUF];
	gethostname(hostname, SZ_STR_BUF);
	printf("%s \n",hostname);
}
// 디렉토리 내의 파일 이름을 보여 주는 명령어
// 사용법: ls [-l] [디렉토리 이름]
//		"-l" 옵션이 없으면 파일 이름만, 있으면 파일의 상세정보를 보여 줌
//		[디렉토리 이름]이 없으면 현재 디렉토리를 의미함
// argv[0] -> "디렉토리이름"; [디렉토리 이름]을 준 경우
// argc = [디렉토리 이름]을 준 경우 1, 안 준 경우 0
// optc = "-l" 옵션을 준 경우 1, 주지 않았을 경우 0

// 사용자ID(계정이름)와 groupID(그룹명)을 보여주는 명령어
// 사용법: id [계정이름]
//		다른 사람 계정 이름을 지정해도 됨.
//		[]는 주지 않아도 되며, 이 경우 본인 정보 출력
// argv[0] -> "계정이름";	[계정이름]을 준 경우
// argc = [계정이름]을 준 경우 1, 안 준 경우 0
void
id(void)
{
	struct passwd *pwp;
	struct group *grp;

	pwp = (argc > 0)? getpwnam(argv[0]):getpwuid(getuid()); 

	if(pwp == NULL || (grp = getgrgid(pwp->pw_gid)) == NULL ){
		printf("id: 잘못된 사용자 이름: \"%s\"", argv[0]);
		return;
	}
	else
		printf("uid=%ld(%s) gid=%ld(%s)\n",pwp->pw_uid, pwp->pw_name,
									   	grp->gr_gid, grp->gr_name);
}

void 
ls(void)
{
	char *path;
	DIR 	*dp;

	// 디렉토리 이름을 주지 않았다면 현재 디렉토리 설정
	path = (argc == 0)? ".": argv[0];

	if((dp = opendir(path)) == NULL)	// 디렉토리 열기
		PRINT_ERR_RET();
	if(optc == 0)
		print_name(dp);
	else
		print_detail(dp, path);
	closedir(dp);				// 디렉토리 닫기
	printf("\n");
}

// 하나의 파일을 다른 이름으로 링크를 만들어 주는 명령어
// 사용법: ln [-s] 원본파일 링크파일
// "-s"옵션이 없으면 hard link,
//			  있으면 symbolic link(바로가기 파일)
// hard link: 디스크에 파일 데이터는 하나만 존재하는데
// 			  이 데이터를 지칭하는 파일 이름이 여러 개인 경우
// argv[0] -> "원본파일 이름"
// argv[1] -> "링크파일 이름"
// optc = "-s"옵션을 준 경우 1, 주지 않았을 경우 0
void
ln(void)
{
	if((optc != 0)?
		symlink(argv[0], argv[1]) < 0 :link(argv[0], argv[1]) < 0)
		PRINT_ERR_RET();
}
// 새로운 디렉토리를 생성하는 명령어
// 사용법: mkdir 디렉토리이름
// argv[0] -> "디렉토리이름"
void
makedir(void)
{
	if(mkdir(argv[0],0755) < 0)
		PRINT_ERR_RET();
		// 0755: rwxr-xr-x
		// 이는 이 디렉토리를 만든 사람은 이 디렉토리에
		// 읽고(r: ls실행 가능), 쓰고(w: 파일 생성 및 삭제 가능),
		// 옮겨 갈 수 있음(x: cd로 갈 수 있음)
		// 그러나 그룹멤버(r-x)나 다른 제 3의 사용자(r-x)는
		// cd 명령어로 옮겨(x)가서 ls를 할 수만 있고(r),
		// 그 디렉토리에 파일을 생성하거나 삭제(w)는 할 수 없다.
}

// 파일 이름을 다른 이름으로 변경하는 명령어
// 사용법: mv 원본파일 바뀐이름
// argv[0] -> "원본파일 이름"
// argv[1] -> "변경된 파일 이름"
void
mv(void)
{
	if((link(argv[0], argv[1])) < 0 || (unlink(argv[0]) < 0))
		PRINT_ERR_RET();
}

// run_cmd:
// 외부 명령어를 실행해주는 함수
// cmd 자신을 복제하여 부모 프로세스는 자식이 종료할 때까지 대기하고,
// 자식 프로세스는 외부 명령어 프로그램으로 대체하여 실행함
void
run_cmd(void)
{
	pid_t pid;

	// fork()를 호출하여 기존에 실행중인 cmd를
	// 복제하여 새로운 프로세스를 생성한다.
	// 에러가 발생했으면 에러 원인 출력하고 리턴함

	if((pid = fork()) < 0)
		PRINT_ERR_RET();
	
	// 이 순간부터 동일한 프로그램(cmd) 2개가
	// 동시에 실행되고 있으며,
	// 프로그램 내에서 실행되는 위치도 바로 이
	// 위치에서 계속 실행된다.

	// fork()의 리턴 값(pid)이 0이면 자식
	// 프로세스이고, 아니면 부모 프로세스임

	// 자식 프로세스는 외부프로그램으로 자기자신을 대체한다. 이 과정에서 에러가
	// 발생했으면 에러 원인 출력하고 스스로 종료한다.
	else if(pid == 0){
		int i, cnt = 0;
		char *nargv[100];
		// 기존의 명령어 cmd, 옵션 optv[], 인자 argv[]에 저장된 포인터 값을
		// 순서대로 nargv[]에 연속적으로 저장함; nargv[]의 마지막은 NULL이어야 함
		nargv[cnt++] = cmd;		     // 기존의 cmd를 nargv[]에 저장
		for(i=0; i<optc; ++i)
			nargv[cnt++] = optv[i];          // 기존의 optv[]를 nargv[]에 저장
		for(i=0; i<argc; ++i)
			nargv[cnt++] = argv[i];	// 기존의 argv[]를 nargv[]에 저장
		nargv[cnt++] = NULL;	            // nargv[]의 마지막은 NULL이어야 함

		if(execvp(cmd, nargv) < 0){
		        // 해당 프로그램이나 접근권한이 없거나 또는 실행 프로그램이 아닌 경우 에러
			perror(cmd);	// 여기서 PRINT_ERR_RET()를 호출하면 안됨
			exit(1);		// 에러 발생 시 자식 프로세스는 종료해야 함
		}
	}	// 자식 프로세스는 외부 프로그램으로 대체되었기 때문에 이 이후의 코드는 
			실행되지 않는다.
		
	// fork()의 리턴 값(pid)이 양수(pid = 자식 프로세스의
	// ID)이면 부모 프로세스임
	// 부모 프로세스(cmd vmfhrmfoa)는 자식 프로세스(외부
	// 프로그램)가 종료될 때까지 더 이상 실행되지 않고
	// 여기서 대기해야 함. 이 과정에서 에러가 발생했으면
	// 에러 원인 출력하고 스스로 종료한다.

	else{	// pid > 0 경우, 즉 부모 프로세스
		// 자식(pid)이 종료될 때까지 대기
		if(waitpid(pid, NULL, 0) < 0)
			PRINT_ERR_RET();
		// 자식 프로세스가 종료되면 waitpid()는
		// 자식 프로세스의 pid 값을 리턴함
	}
	// 만약 여기서 부모가 자식 종료를 기다리지 않으면\
	// 자식은 어떻게 될까? Zombie 프로세스가 됨
	// 즉, 죽어서 무덤까지 만들어 주었지만 (시스템 내에
	// 종료된 프로그램 제거), 부모가 동사무소에 사망신고를
	// 하지 않은 경우임(종료된 프로그램의 정보는 남아있음)
}

// 프로그램을 원하는 시간 동안(초 단위) 일시적으로 중지하는 명령어
// 사용법: sleep 초단위 시간
void
Sleep(void)
{
	int sec;
	sscanf(argv[0], "%d", &sec);
	sleep(sec);
}

// 현재 작업 디렉토리 이름을 출력하는 명령어
// 사용법: pwd
void
pwd(void)
{
	printf("%s \n", cur_work_dir);
}
// 빈 디렉토리($ls -l 때 ..과 .만 존재)를 삭제하는 명령어
// 하나의 파일을 삭제하는 명령어
// 사용법: rm  파일이름
// argv[0] -> "파일이름"
void
removedir(void)
{
	if(rmdir(argv[0]) < 0)
		PRINT_ERR_RET();
}

// rm 명령어
void
rm(void)
{
	struct stat buf;
	int ret;
	if(lstat(argv[0], &buf) < 0 ||
		S_ISDIR(buf.st_mode) ? rmdir(argv[0]) : unlink(argv[0]) < 0)
		PRINT_ERR_RET();
}

// 존재하지 않는 파일 이름일 경우 크기가 0인 새로운 파일 생성
// 존재하는 파일인 경우 파일의 수정시간을 현재시간으로 변경함
// 사용법: touch 파일이름
// argv[0] -> "파일이름"
void
touch(void)
{
	int fd;

	if(utime(argv[0], NULL) == 0)
		return;
	if((errno != ENOENT) || (fd = creat(argv[0], 0644)) < 0)
		PRINT_ERR_RET();
	
	close(fd);
}

// 운영체제 이름 및 버전 등 시스템 정보를 출력하는 명령어
// "-a"옵션을 주면 상세정보를, 안 주면 시스템 이름만 출력함
// 사용법: uname[-a]
// optc = "-a"옵션 주면 1, 안 주면 0;
void
unixname(void)
{
	struct utsname un;
	if(uname(&un) < 0)
		PRINT_ERR_RET();
	printf("%s ", un.sysname);

	if(optc)
		printf("%s %s %s %s",un.nodename, un.release, un.version, un.machine);
	printf("\n");
}

// 사용자의 계정 이름을 출력하는 명령어
// 사용법: whoami 
void
whoami(void)
{
//	char *username;
//	username = getlogin();
//	if(username != NULL)
//		printf("%s \n", username);
//	else
//		printf("터미널 장치가 아니라서 사용자 계정정보를 구할 수 없습니다. \n");
	struct passwd *pwp;
	pwp = getpwuid(getuid());
	printf("%s \n",pwp->pw_name);
}


// 프로그램을 종료하는 명령어
// 사용법: exit
void
quit(void)
{
	exit(0);
}

//
// 각 명령어 구현 끝
//
//***************************************************************************

// 모든 함수는 그 함수를 호출하거나 함수 이름이 사용되기 전에 먼저 
// 선언되거나 정의되어야 한다. 따라서 아래 cmd_tbl[]의 각 배열 원소에서 
// 명령어 처리함수 이름을 사용하기 때문에 이 함수들은 아래 cmd_tbl[] 보다 
// 먼저 함수정의가 되어야 하고 실제 앞에 이미 정의 되었다.
// 그러나 help() 함수의 정의는 cmd_tbl[] 뒤에 있기 때문에, cmd_tbl[] 앞에 
// 이 함수를 아래처럼 미리 선언해야 한다. help() 함수정의는 cmd_tbl[]을 
// 직접 사용하기 때문에 어쩔 수 없이 cmd_tbl[] 뒤에 있어야 한다.

void help(void);

//***************************************************************************
//  각 명령어별 상세 정보를 저장한 구조체 배열 cmd_tbl[]
//***************************************************************************

#define AC_LESS_1		-1		// 명령어 인자 개수가 0 또는 1인 경우
#define AC_ANY			-100	// 명령어 인자 개수가 제한 없는 경우(echo)

// 하나의 명령어에 대한 정보 구조체
typedef struct {
	char *cmd;			// 명령어 문자열
	void (*func)(void);	// 명령어를 처리하는 함수 시작 주소(함수 이름)
	int  argc;			// 명령어 인자의 개수(명령어와 옵션은 제외)
	char *opt;			// 옵션 문자열: 예) "-a", 옵션 없으면 ""
	char *arg;			// 명령어 사용법 출력할 때 사용할 명령어 인자: 
} cmd_tbl_t;			//		예) cp인 경우 "원본파일 복사파일""

cmd_tbl_t cmd_tbl[] = {
// 명령어		명령어		명령어		명령어 	사용법 출력 시
// 이름				처리		인자	명령어  사용할 명령어
// 문자열 		함수이름		개수		옵션	인자
  { "cp",		cp,		2,		"",	"원본파일  복사된파일" },
  { "echo",	echo,		AC_ANY,		"",	"[에코할 문장]" },
  { "help",		help,		0,		"",	"" },
  { "ls",		ls,		AC_LESS_1,	"-l",	"디렉토리" },
  { "exit",		quit,		0,		"",	"" },
  { "rmdir",	removedir,	1,		"",	"디렉토리" },
  { "rm",		rm,		1,		"",	"디렉토리" },
  { "pwd",		pwd,		0,		"",	""},
  { "hostname",	hostname,	0,		"",	""},
  { "whoami",	whoami,		0,		"",	""},
  { "cd",		cd,		AC_LESS_1,	"",	"[디렉토리이름]"},
  { "uname",	unixname,	0,		"-a",	""},
  { "mkdir",	makedir,		1,		"",	"디렉토리"},
  { "mv",		mv,		2,		"",	"원본파일  바뀐이름"},
  { "ln",		ln,		2,		"-s",	"원본파일  링크파일"},
  { "chmod",	changemod,	2,		"",	"8진수mode  파일이름"},			
  { "id",		id,		AC_LESS_1,	"",	"[사용자계정]"},	
  { "date",		date,		0,		"",	""},
  { "cat",		cat,		1,		"",	"파일이름"},
  { "touch",	touch,		1,		"",	"파일이름"},
  { "sleep",	Sleep,		1,		"",	"초단위시간"},
};
// (총 명령어 개수) = (cmd_tbl[] 배열 전체 크기) / (배열의 한 원소 크기)
int num_cmd = sizeof(cmd_tbl) / sizeof(cmd_tbl[0]);

// 이 프로그램이 지원하는 명령어 리스트를 보여 주는 명령어
// 사용법: help
// 이 함수가 일반 명령어를 처리하는 함수들과는 달리 여기에 배치된 것은
// 바로 위에 있는 cmd_tbl[]을 이 함수가 직접 사용하기 때문이다.
void
help(void)
{
	int k;

	printf("현재 지원되는 명령어 종류입니다.\n");
	for (k = 0; k < num_cmd; ++k)
		print_usage("  ", cmd_tbl[k].cmd, cmd_tbl[k].opt, cmd_tbl[k].arg);
	printf("\n");
}

// proc_cmd():
// 입력된 명령어와 동일한 이름의 명령어 구조체를 cmd_tbl[] 배열 찾아 낸 후
// 명령어 인자와 옵션을 제대로 입력했는지 체크하고 잘못 되었으면 사용법을
// 출력하고 정상이면 해당 명령어를 처리하는 함수를 호출하여 실행함
void
proc_cmd(void)
{
	int  k;

	for (k = 0; k < num_cmd; ++k) {	// 입력한 명령어 정보를 cmd_tbl[]에서 찾음
		// 전역변수char*cmd: 사용자가 입력한 명령어 문자열 주소, 예)cmd->"ls"
		if (EQUAL(cmd, cmd_tbl[k].cmd)) { // 입력 명령어를 cmd_tbl에서 찾았음

			if ((check_arg(cmd_tbl[k].argc) < 0) || 	// 인자 개수 체크
				(check_opt(cmd_tbl[k].opt)  < 0))	// 옵션 체크
				// 인자 개수 또는 옵션이 잘못 되었음: 사용법 출력
				print_usage("사용법: ", cmd_tbl[k].cmd,
						 cmd_tbl[k].opt, cmd_tbl[k].arg);
				// 예) 사용법: ls  -l  [디렉토리이름] 
			else
				cmd_tbl[k].func();	// 명령어를 처리하는 함수를 호출

			return;
		}
	}
	// 내부명령어 찾지 못한 경우
	run_cmd();	// 새로 추가
}

//***************************************************************************
// 	main() 함수
//***************************************************************************
int 
main(int argc, char *argv[])
{
	int cmd_count = 1;	// 현재 명령어 번호
	char cmd_line[SZ_STR_BUF]; 	// 입력된 명령어 라인 전체를 저장 공간

	setbuf(stdout, NULL);	// 표준 출력 버퍼 제거: printf() 즉시 화면 출력
	setbuf(stderr, NULL);	// 표준 에러 출력 버퍼 제거

	help();						// 명령어 전체 리스트 출력

	// 현재작업디렉토리의 이름을 구해와 cur_work_dir[]에 저장함
	// SZ_STR_BUF: cur_work_dir[SZ_STR_BUF]의 크기
	getcwd(cur_work_dir, SZ_STR_BUF);

	for (;;) {
		// 명령 프롬프트 출력: "<현재작업디렉토리> 명령어번호: "
		printf("<%s> %d: ", cur_work_dir, cmd_count);
		gets(cmd_line);	// 키보드에서 한 행을 입력 받아 cmd_line[]에 저장

		// 입력받은 한 행(cmd_line[])의 명령어, 옵션, 인자를 개별 문자열로 분리
		if (get_argv_optv(cmd_line) != NULL) {
			proc_cmd();	// 한 명령어 처리 
			cmd_count++;
		}
		// else 명령어는 주지 않고 엔터만 친 경우 명령어 번호는 증가하지 않음
	}
}
