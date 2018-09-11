
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"

/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main() 
{
    struct task* p_task;
    struct proc* p_proc= proc_table;
    char* p_task_stack = task_stack + STACK_SIZE_TOTAL;
    u16   selector_ldt = SELECTOR_LDT_FIRST;
    u8    privilege;
    u8    rpl;
    int   eflags;
    int   i, j;
    int   prio;

    // start the process
    for (i = 0; i < NR_TASKS+NR_PROCS; i++) {
        if (i < NR_TASKS) {
            /* task */
            p_task    = task_table + i;
            privilege = PRIVILEGE_TASK;
            rpl       = RPL_TASK;
            eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1   1 0010 0000 0010(2)*/
            prio      = 15;     //set the priority to 15
        }
        else {
            /* user process */
            p_task    = user_proc_table + (i - NR_TASKS);
            privilege = PRIVILEGE_USER;
            rpl       = RPL_USER;
            eflags    = 0x202; /* IF=1, bit 2 is always 1              0010 0000 0010(2)*/
            prio      = 5;     //set the priority to 5
        }

        strcpy(p_proc->name, p_task->name); /* set prio name */
        p_proc->pid = i;            /* set pid */

        p_proc->run_count = 0;

        p_proc->ldt_sel = selector_ldt;

        memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
               sizeof(struct descriptor));
        p_proc->ldts[0].attr1 = DA_C | privilege << 5;
        memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
               sizeof(struct descriptor));
        p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
        p_proc->regs.cs = (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.ds = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.es = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.fs = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.ss = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.gs = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

        p_proc->regs.eip = (u32)p_task->initial_eip;
        p_proc->regs.esp = (u32)p_task_stack;
        p_proc->regs.eflags = eflags;

        p_proc->p_flags = 0;
        p_proc->p_msg = 0;
        p_proc->p_recvfrom = NO_TASK;
        p_proc->p_sendto = NO_TASK;
        p_proc->has_int_msg = 0;
        p_proc->q_sending = 0;
        p_proc->next_sending = 0;

        for (j = 0; j < NR_FILES; j++) {
            p_proc->filp[j] = 0;
        }

        p_proc->ticks = p_proc->priority = prio;
        p_proc->run_state = 1;
        p_task_stack -= p_task->stacksize;
        p_proc++;
        p_task++;
        selector_ldt += 1 << 3;
    }

    proc_table[4].run_state = 0;
    proc_table[5].run_state = 0;

    // init process
    k_reenter = 0;
    ticks = 0;

    p_proc_ready = proc_table;

    init_clock();
    init_keyboard();

    restart();
}


/*****************************************************************************
 *                                get_ticks
 *****************************************************************************/
PUBLIC int get_ticks() {
    MESSAGE msg;
    reset_msg(&msg);
    msg.type = GET_TICKS;
    send_recv(BOTH, TASK_SYS, &msg);
    return msg.RETVAL;
}

PUBLIC void addTwoString(char *to_str, char *from_str1, char *from_str2) {
    int i = 0, j = 0;
    while (from_str1[i] != 0) {
        to_str[j++]=from_str1[i++];
    }
    i = 0;
    while (from_str2[i] != 0) {
        to_str[j++]=from_str2[i++];
    }
    to_str[j]=0;
}

void shell(char *tty_name) {
	clear();
    int fd;
    //int isLogin = 0;
    char rdbuf[512];
    char cmd[512];
    char arg1[512];
    char arg2[512];
    char buf[1024];
    char temp[512];

    int fd_stdin  = open(tty_name, O_RDWR);
    assert(fd_stdin  == 0);
    int fd_stdout = open(tty_name, O_RDWR);
    assert(fd_stdout == 1);
    // The boot animation
    // animation();
    char current_dirr[512] = "/";
	clear();
    while (1) {
        // clear the array ！
        clearArr(rdbuf, 512);
        clearArr(cmd, 512);
        clearArr(arg1, 512);
        clearArr(arg2, 512);
        clearArr(buf, 1024);
        clearArr(temp, 512);
        
        //help();
        printf("root@owls%s:~$ ", current_dirr);
        int r = read(fd_stdin, rdbuf, 512);
        if (strcmp(rdbuf, "") == 0) {
            continue;
        }
        int i = 0;
        int j = 0;
        while ((rdbuf[i] != ' ') && (rdbuf[i] != 0)) {
            cmd[i] = rdbuf[i];
            i++;
        }
        i++;
        while ((rdbuf[i] != ' ') && (rdbuf[i] != 0)) {
            arg1[j] = rdbuf[i];
            i++;
            j++;
        }
        i++;
        j = 0;
        while ((rdbuf[i] != ' ') && (rdbuf[i] != 0)) {
            arg2[j] = rdbuf[i];
            i++;
            j++;
        }
        // clear
        rdbuf[r] = 0;

        // Command "procmng"
        if (strcmp(cmd, "procmng") == 0) {
            ProcessManage();
        }

        // Command "help"
        else if (strcmp(cmd, "help") == 0) {
            help();
        }      

        // Command "clear"
        else if (strcmp(cmd, "clear") == 0) {
            clear(); 
        }

        // Command "ls"
        else if (strcmp(cmd, "ls") == 0) {
            ls(current_dirr);
        }

        // Command "about"
        else if (strcmp(cmd, "about") == 0) {
            printAbout();
        }

        // Command "pause 4"
        else if (strcmp(rdbuf, "pause 4") == 0) {
            proc_table[4].run_state = 0 ;
            ProcessManage();
        }

        // Commandd "pause5"
        else if (strcmp(rdbuf, "pause 5") == 0) {
            proc_table[5].run_state = 0 ;
            ProcessManage();
        }

        // Command "pause6"
        else if (strcmp(rdbuf, "pause 6") == 0) {
            proc_table[6].run_state = 0 ;
            ProcessManage();
        }

        // Command "kill 4"
        else if (strcmp(rdbuf, "kill 4") == 0) {
        //            proc_table[4].p_flags = 1;
        //            ProcessManage();
            printf("cannott kill this process!");
        }

        // Command "kill 5"
        else if (strcmp(rdbuf, "kill 5") == 0) {
            proc_table[5].p_flags = 1;
            ProcessManage();
        }

        // Command "kill 6"
        else if (strcmp(rdbuf, "kill 6") == 0) {
            proc_table[6].p_flags = 1;
            ProcessManage();
        }

        // Command "resume 4"
        else if (strcmp(rdbuf, "resume 4") == 0 ) {
            proc_table[4].run_state = 1 ;
            ProcessManage();
        }

        // Command "resume 5"
        else if (strcmp(rdbuf, "resume 5") == 0 ) {
            proc_table[5].run_state = 1 ;
            ProcessManage();
        }

        // Command "resume 6"
        else if (strcmp(rdbuf, "resume 6") == 0 ) {
            proc_table[6].run_state = 1 ;
            ProcessManage();
        }

        // Command "up 4"
        else if (strcmp(rdbuf, "up 4") == 0 ) {
            proc_table[4].priority = proc_table[4].priority*2;
            ProcessManage();
        }

        // Command "up 5"
        else if (strcmp(rdbuf, "up 5") == 0 ) {
            proc_table[5].priority = proc_table[5].priority*2;
            ProcessManage();
        }

        // Command "up 6"
        else if (strcmp(rdbuf, "up 6") == 0 ) {
            proc_table[6].priority = proc_table[6].priority*2;
            ProcessManage();
        }

        // Command "mkfile"
        else if (strcmp(cmd, "mkfile") == 0) {
            if (arg1[0] != '/') {
                addTwoString(temp,current_dirr, arg1);
                memcpy(arg1, temp, 512);                
            }

            fd = open(arg1, O_CREAT | O_RDWR);
            if (fd == -1) {
                printf("Failed to create file! Please check the filename!\n");
                continue ;
            }
            write(fd, buf, 1);
            printf("File created: %s \n", arg1);
            close(fd);
        }

        // Command "mkdir"
        else if (strcmp(cmd, "mkdir") == 0) {
            i = j = 0;
            while (current_dirr[i] != 0) {
                arg2[j++] = current_dirr[i++];
            }
            i = 0;
            
            while (arg1[i] != 0) {
                arg2[j++] = arg1[i++];
            }
            arg2[j] = 0;
            printf("%s\n", arg2);
            fd = mkdir(arg2);
        }

        // Command "cd"
        else if (strcmp(cmd, "cd") == 0) {
            if (strcmp(arg1, "..") == 0) {
                memcpy(arg2, current_dirr, 512);
                j = 0;
                int k = 0;
                int count = 0;
                while (arg2[k] != 0) {
                    if (arg2[k++] == '/') {
                        count++;
                    }
                }
                int index = 0;
                for (i = 0; arg2[i] != 0; i++) {
                    if (arg2[i] == '/') {
                        index++;
                    }
                    if (index < count - 1) {
                        arg1[j++] = arg2[i];
                    }
                }
                arg1[j++] = '/';
                arg1[j] = 0;
            }
            else if (arg1[0] == 0) {
                arg1[0] = '/';
                arg1[1] = 0;
            }
            else { // not absolute path from root
                i = j = 0;
                while (current_dirr[i] != 0) {
                    arg2[j++] = current_dirr[i++];
                }
                i = 0;
                while (arg1[i] != 0) {
                    arg2[j++] = arg1[i++];
                }
                arg2[j++] = '/';
                arg2[j] = 0;
                memcpy(arg1, arg2, 512);
            }
            //printf("%s\n", arg1);
            fd = open(arg1, O_RDWR);

            if (fd == -1) {
                printf("The path not exists! Please check the pathname!\n");
            }
            else {
                memcpy(current_dirr, arg1, 512);
                //printf("Change dir %s success!\n", current_dirr);
            }
        }

	else if (strcmp(cmd,"queen")==0){
		queen();}
	else if (strcmp(cmd,"calculator")==0){
		//fd=open(cmd,O_RDWR);
		//if(fd==-1)
		//	printf("Fail to Open Calculation File!");
		calculation();
}
	else if(strcmp(cmd,"minesweeper")==0){
		minesweeper();}
        // Command not supported
        else {
            printf("Command not supported!\n");
        }
    }
}

/*======================================================================*
                               TestA
 *======================================================================*/

//A process
void TestA(){
    animation();
    shell("/dev_tty0");
    
    assert(0);
}

/*======================================================================*
                               TestB
 *======================================================================
*/
//B process
void TestB(){
	shell("/dev_tty1");
	assert(0); /* never arrive here */
}
/*======================================================================*
                               TestC
 *======================================================================*/
//C process
void TestC(){
	// shell("/dev_tty2");
	assert(0);
}

/*****************************************************************************
 *                                panic
 *****************************************************************************/
PUBLIC void panic(const char *fmt, ...) {
    int i;
    char buf[256];
    /* 4 is the size of fmt in the stack */
    va_list arg = (va_list) ((char*) &fmt + 4);
    i = vsprintf(buf, fmt, arg);
    printl("%c !!panic!! %s", MAG_CH_PANIC, buf);
    /* should never arrive here */
    __asm__ __volatile__("ud2");
}

/*****************************************************************************
 *                                Custom Command
 *****************************************************************************/
char* findpass(char *src) {
    char pass[128];
    int flag = 0;
    char *p1, *p2;

    p1 = src;
    p2 = pass;

    while (p1 && (*p1 != ' ')) {
        if (*p1 == ':') {
            flag = 1;
        }

        if (flag && (*p1 != ':')) {
            *p2 = *p1;
            p2++;
        }
        p1++;
    }
    *p2 = '\0';

    return pass;
}

void clearArr(char *arr, int length) {
    int i;
    for (i = 0; i < length; i++) {
        arr[i] = 0;
    }
}

void printAbout() {
    clear(); 
    if (current_console == 0) {
        printf("================================================================================");
	printf("\n");
	printf("\n");        
	printf("                                Yuxin Sun's OS                                       ");
        printf("\n");
	printf("\n");
        printf("================================================================================");
    }
    else {
    	printf("[TTY #%d]\n", current_console);
    }
}

void clear() {	
    clear_screen(0, console_table[current_console].cursor);
    console_table[current_console].crtc_start = console_table[current_console].orig;
    console_table[current_console].cursor = console_table[current_console].orig;
}

void doTest(char *path) {
    struct dir_entry *pde = find_entry(path);
    printl(pde->name);
    printl("\n");
    printl(pde->pass);
    printl("\n");
}

int verifyFilePass(char *path, int fd_stdin) {
    char pass[128];

    struct dir_entry *pde = find_entry(path);

    /*printl(pde->pass);*/

    if (strcmp(pde->pass, "") == 0) {
        return 1;
    }

    printl("Please input the file password: ");
    read(fd_stdin, pass, 128);

    if (strcmp(pde->pass, pass) == 0) {
        return 1;
    }

    return 0;
}

void doEncrypt(char *path, int fd_stdin) {
    //search the file
    /*struct dir_entry *pde = find_entry(path);*/

    char pass[128] = { 0 };

    printl("Please input the new file password: ");
    read(fd_stdin, pass, 128);

    if (strcmp(pass, "") == 0) {
        /*printl("A blank password!\n");*/
        strcpy(pass, "");
    }
    //encrypt the file
    int i, j;

    char filename[MAX_PATH];
    memset(filename, 0, MAX_FILENAME_LEN);
    struct inode * dir_inode;

    if (strip_path(filename, path, &dir_inode) != 0) {
        return 0;
    }

    /* path: "/" */
    if (filename[0] == 0) {
        return dir_inode->i_num;
    }
    /**
     * Search the dir for the file.
     */
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries = dir_inode->i_size / DIR_ENTRY_SIZE;
    int m = 0;
    struct dir_entry * pde;
    for (i = 0; i < nr_dir_blks; i++) {
        RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
        pde = (struct dir_entry *) fsbuf;
        for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
            if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0) {
                // delete the file
                strcpy(pde->pass, pass);
                WR_SECT(dir_inode->i_dev, dir_blk0_nr + i);
                return;
                /*return pde->inode_nr;*/
            }
            if (++m > nr_dir_entries) {
                break;
            }
        }

         /* all entries have been iterated */
        if (m > nr_dir_entries) {
            break;
        }
    }
}

void help() {
    printf("================================================================================");
    printf("                                  SYX'S  OS                                       ");
    printf("==============================================================================");
    printf("    clear                        : clear the screen\n");
    printf("    help                         : show the guide of operation\n");
    printf("    ls                           : list files in current directory\n");
    printf("    mkfile       [filename]      : create a new file\n");
    printf("    cd           [pathname]      : change the directory\n");
    printf("    mkdir        [dirname]       : create a new directory\n");
    printf("    calculator                   : calculate with a calculator\n");
    printf("    queen                        : play the game N queen\n");
    printf("    minesweeper                  : start the minesweeper game                  ");
    // printf("    snake                         : start the snake game                        ");
    // printf("    2048                          : start the 2048 game                         ");
   // printf("    procmng                          : process management                          ");
   // printf("    uname                         : display the system's name                   ");
    printf("================================================================================");
}

void ProcessManage() {
    int i;
    printf("=============================================================================\n");
    printf("         pID      |    name       |      priority    |     running        \n");
    printf("-----------------------------------------------------------------------------\n");
    for (i = 0 ; i < NR_TASKS + NR_PROCS ; ++i) {
        printf("          %2d          %10s              %2d              %d\n",
               proc_table[i].pid, proc_table[i].name, proc_table[i].priority, proc_table[i].run_state);
    }
    printf("=============================================================================\n");
    printf("=          Usage: pause  [pid]  you can pause one process                   =\n");
    printf("=          	      resume [pid]  you can resume one process                  =\n");
    printf("=                 kill   [pid]  kill the process                            =\n");
    printf("=                 up     [pid]  improve the process priority                =\n");
    printf("=============================================================================\n");
}


/*======================================================================*
                            The boot animation
 *======================================================================*/
void p0(int n, int color){
    for(int i = 0; i < n; i++){

        disp_color_str("0",color);
    }
}

void animation(){

    clear();
}



/*======================================================================*
                            Minesweeper Game
 *======================================================================*/


#define row 12
#define col 12
#define COUNT 10//棋盘中雷的总数

#define _CRT_SECURE_NO_WARNINGS 1
char show_mine[row][col] = { 0 };
char real_mine[row][col] = { 0 };


void muen()
{
    printf("*******************************\n");
    printf("*****1.play       0.exit*******\n");
    printf("*******************************\n");
}
void open_mine(int x, int y)//坐标周围展开函数
{
    if (real_mine[x - 1][y - 1]== '0')
    {
        show_mine[x - 1][y - 1] = count_mine(x - 1, y - 1) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x - 1][y] == '0')
    {
        show_mine[x - 1][y] = count_mine(x - 1, y) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x - 1][y + 1] == '0')
    {
        show_mine[x - 1][y + 1] = count_mine(x - 1, y + 1) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x][y - 1] == '0')
    {
        show_mine[x][y - 1] = count_mine(x, y - 1) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x][y + 1] == '0')
    {
        show_mine[x][y + 1] = count_mine(x, y + 1) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x + 1][y - 1] == '0')
    {
        show_mine[x + 1][y - 1] = count_mine(x + 1, y - 1) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x + 1][y] == '0')
    {
        show_mine[x + 1][y] = count_mine(x + 1, y) + '0';//显示该坐标周围雷数
    }
    if (real_mine[x + 1][y + 1] == '0')
    {
        show_mine[x + 1][y + 1] = count_mine(x + 1, y + 1) + '0';//显示该坐标周围雷数
    }
}

void init_mine()//初始化两个棋盘
{
    int i = 0;
    int j = 0;
    for (int i = 0; i < row; i++)
    {
        for (j = 0; j < col; j++)
        {
            show_mine[i][j] = '*';
            real_mine[i][j] = '0';
        }
    }
}


void print_player()//打印玩家棋盘
{
    int i = 0;
    int j = 0;
    printf("0  ");
    for (i = 1; i <row-1; i++)
    {
        printf("%d ", i);//打印横标（0--10）
    }
    printf("\n");
    for (i = 1; i <row-2; i++)//打印竖标（1--10）
    {
        printf("%d  ", i);
        for (j = 1; j < col-1; j++)
        {
            printf("%c ", show_mine[i][j]);//玩家棋盘数组
        }
        printf("\n");
    }
    printf("10 ");//开始打印最后一行
    for (i = 1; i < row-1; i++)
    {
        printf("%c ", show_mine[10][i]);
    }
    printf("\n");
}


void print_mine()//打印设计者棋盘
{
    int i = 0;
    int j = 0;
    printf("0  ");
    for (i = 1; i <row - 1; i++)
    {
        printf("%d ", i);//打印横标（0--10）
    }
    printf("\n");
    for (i = 1; i <row - 2; i++)//打印竖标（1--10）
    {
        printf("%d  ", i);
        for (j = 1; j < col - 1; j++)
        {
            printf("%c ", real_mine[i][j]);
        }
        printf("\n");
    }
    printf("10 ");//开始打印最后一行
    for (i = 1; i < row - 1; i++)
    {
        printf("%c ", real_mine[10][i]);
    }
    printf("\n");
}



void set_mine()//给设计者棋盘布雷
{
    real_mine[1][4]='1';
    real_mine[2][1]='1';
    real_mine[3][5]='1';
    real_mine[4][2]='1';
    real_mine[5][9]='1';
    real_mine[6][8]='1';
    real_mine[7][7]='1';
    real_mine[8][3]='1';
    real_mine[9][6]='1';
    real_mine[10][10]='1';
}


int count_mine(int x, int y)//检测周围8个区域雷的个数
{
    int count = 0;
    if (real_mine[x - 1][y - 1] == '1')
        count++;
    if (real_mine[x - 1][y] == '1')
        count++;
    if (real_mine[x - 1][y + 1] == '1')
        count++;
    if (real_mine[x][y - 1] == '1')
        count++;
    if (real_mine[x][y + 1] == '1')
        count++;
    if (real_mine[x + 1][y - 1] == '1')
        count++;
    if (real_mine[x + 1][y] == '1')
        count++;
    if (real_mine[x + 1][y + 1] == '1')
        count++;
    return count;
}

void safe_mine()//避免第一次炸死
{
    int x = 0;
    int y = 0;
    char ch = 0;
    int count = 0;
    int ret = 1;

    char bufr1[128]={'\0'};
    char bufr2[128]={'\0'};
    printf("Please input the location\n");
    while (1)
    {
        int i=0,j=0;
    printf("Input the location:\n");
    printf("Row:");
    i=read(0,bufr1,128);
    x=getNum(bufr1);
    printf("Column:");
    j=read(0,bufr2,128);
    y=getNum(bufr2);//只能输入1到10，输入错误重新输入
        if ((x >= 1 && x <= 10) && (y >= 1 && y <= 10))//判断输入坐标是否有误
        {
            if (real_mine[x][y] == '1')//第一次踩到雷后补救
            {
                real_mine[x][y] = '0';
                char ch = count_mine(x, y);
                show_mine[x][y] = ch + '0';//数字对应的ASCII值和数字字符对应的ASCII值相差48，即'0'的ASCII值
                open_mine(x, y);
                while (ret)//在其余有空的地方设置一个雷
                {
                    int x = 5;//产生1到10的随机数，在数组下标为1到10的范围内布雷
                    int y = 1;//产生1到10的随机数，在数组下标为1到10的范围内布雷
		    x++,y++;
		    x%=10;
		    y%=10;
                    if (real_mine[x][y] == '0')//找不是雷的地方布雷
                    {
                        real_mine[x][y] = '1';
                        ret--;
                        break;
                    }
                }break;//跳出此函数
            }
            if (real_mine[x][y] == '0')
            {
                char ch = count_mine(x, y);
                show_mine[x][y] = ch + '0';//数字对应的ASCII值和数字字符对应的ASCII值相差48，即'0'的ASCII值
                open_mine(x, y);
                break;
            }
        }
        else//坐标错误
        {
            printf("Wrong input! Please input again!\n");
        }
    }
}


int sweep_mine()//扫雷函数，踩到雷返回1，没有踩到雷返回0
{
    int x = 0;
    int y = 0;
    int count = 0;
    int i=0,j=0;
    char bufr1[128]={'\0'};
    char bufr2[128]={'\0'};
    printf("Input the location:\n");
    printf("Row:");
    i=read(0,bufr1,128);
    x=getNum(bufr1);
    printf("Column:");
    j=read(0,bufr2,128);
    y=getNum(bufr2);
    if ((x >= 1 && x <= 10) && (y >= 1 && y <= 10))//判断输入坐标是否有误，输入错误重新输入
    {
        if (real_mine[x][y] == '0')//没踩到雷
        {
            char ch = count_mine(x,y);
            show_mine[x][y] = ch+'0';//数字对应的ASCII值和数字字符对应的ASCII值相差48，即'0'的ASCII值
            open_mine(x, y);
            if (count_show_mine() == COUNT)//判断剩余未知区域的个数，个数为雷数时玩家赢
            {
                print_mine();
                printf("Win！\n\n");
                return 0;
            }
        }
        else if (real_mine[x][y]=='1')//踩到雷
        {
            return 1;
        }
        
    }
    else
    {
        printf("Wrong input! Please input again!\n");
    }
    return 0;//没踩到雷
}



int count_show_mine()//判断剩余未知区域的个数，个数为雷数时玩家赢
{
    int count = 0;
    int i = 0;
    int j = 0;
    for (i = 1; i <= row - 2; i++)
    {
        for (j = 1; j <= col - 2; j++)
        {
            if (show_mine[i][j] == '*')
            {
                count++;
            }
        }
        
    }
    return count;
}

void game()
{
    int ret = 0;
    init_mine();//初始化玩家棋盘和设计者棋盘
    set_mine();//给设计者棋盘布雷
   // print_mine();//打印设计者棋盘（可不打印）
    printf("\n");
    print_player();//打印玩家棋盘
    safe_mine();//避免第一次被炸死
    
    if (count_show_mine() == COUNT)//一步就赢的情况
    {
        print_mine();
        printf("Win！\n\n");
        return ;
    }print_player();////打印玩家棋盘
    
    while (1)//循环扫雷
    {
        int ret=sweep_mine();//扫雷,踩到雷返回1，没有踩到雷返回0
        if (count_show_mine() == COUNT)//若玩家棋盘的'*'个数为雷数时，扫雷完成，游戏胜利
        {
            print_mine();//打印设计者棋盘
            printf("Win！\n\n");
            break;
        }
        if (ret)//判断是否踩到雷
        {
            printf("Beng!\t");
            print_mine();//打印设计者雷阵查看雷的分布
            break;
        }print_player();//打印玩家棋盘
    }
}


int minesweeper()
{
    //srand((unsigned int)time(NULL));//产生随机数生成器
    int input = 0,i=0;
    char bufr[128]={'\0'};
    muen();//菜单
    do
    {
        i=read(0,bufr,128);
	input=getNum(bufr);
        switch (input)
        {
            case 1:
		printf("Input:%d",input);
		game();
                break;
            case 0://退出游戏
                break;
            default:
                printf("Wrong input! Please input again!\n");
                break;
        }
	if(input==1)break;
        muen();
        printf("contiue?\n");
    } while (1);//循环玩游戏
   // system("pause");
    return 0;
}
/*********************** Ending of the snake game ************************/


/**************************queen********************************/

int position_check(int,int*);
void print_board(int count,int* y);
int queen()
{
    
    printf("****************************************************\n");
    printf("*                     queen                        *\n");
    printf("****************************************************\n\n");
    printf("Please input the number of Queen:");
    int N=0,i=0;
    char bufr[128]={'\0'};
    i=read(0,bufr,128);
    N=getNum(bufr);
    int y[10]= {0}; //记录每列上的皇后放的位置
    int count = 0; //解的个数
	//printf("%s\n",bufr);
  //  printf("%d\n",N);
    int x = 1;
    while(x>0)
    {
        y[x]++;      //为当前x位置找一个皇后的位置
        while((y[x]<=N) && !position_check(x,y))
            y[x]++; //找到合适的皇后
        //
        if(y[x]<=N)//找到一个可以放置第x个皇后的位置，到该步为止，所求部分解都满足要求
        {
            if(x==N)//找到一个完整的放置方案
            {
                count++;
                printf("%d\n",count);
                for( int i=1; i<=N; i ++ )
                {
                    for( int j=1; j<=N; j++ )
                        if( j==y[i] ) printf("x ");//如果该位置放了皇后则显示x
                        else printf("o ");
                    printf("\n");
                }
            }
            else
                x++; //继续寻找下一个皇后的位置，还没找到完整解决方案
        }
        else//未找到可以放置第x个皇后的位置，到该步为止，已经知道不满足要求
        {
            y[x] = 0;//因为要回溯，下一次是寻找第x-1个皇后的位置，
            //在下一次确定x-1的位置之后，第x个皇后的开始搜索的位置要重置
            x--; //回溯
        }
    }
}
int position_check(int k,int* y) //测试合法性
{
    for(int j = 1; j < k; j++)
        {
		int kj=0;
		if (k>=j)kj=k-j;
		else kj=j-k;
		int yy=0;
		if(y[j]>=y[k])yy=y[j]-y[k];
		else yy=y[k]-y[j];
		if(kj==yy||y[j]==y[k])return 0;
}
    return 1;
}
//int getNumOfQueen(char * bufr)
//{
//	int res=0;
//	res=bufr[0]-'0';
//	return res;
//}

int calculation()
{
	int i, num1 = 0, num2 = 0, flag = 1, res = 0;
    char bufr[128]={'\0'};
	char bufr1[128]={'\0'};
    char bufr2[128]={'\0'};

	printf("***************************************************\n");
	printf("*                  Calculator                     *\n");
	printf("***************************************************\n");
	printf("*  Please enter two integers                      *\n");
	printf("*  Enter e to quit                                *\n");
	printf("***************************************************\n\n");

	while(flag == 1){	
		printf("Please input num1:");
		i = read(0, bufr1, 128);
		if (bufr1[0] == 'e')
			break;
		num1 = getNum(bufr1);
		printf("num1: %d\n", num1);

		printf("Please input num2:");
		i = read(0, bufr2, 128);	
		if (bufr2[0] == 'e')
			break;
		num2 = getNum(bufr2);
		printf("num2: %d\n", num2);

		printf("Please input op( + - * / ):");
		i = read(0, bufr, 1);
		switch(bufr[0])
		{
			case '+':
				res = num1 + num2;
				printf("%d + %d = %d\n", num1, num2, res);
				break;
			case '-':
				res = num1 - num2;
				printf("%d - %d = %d\n", num1, num2, res);
				break;
			case '*':
				res = num1 * num2;
                printf("%d * %d = %d\n", num1, num2, res);
				break;
			case '/':
				if(num2 <= 0)
				{
					printf("Num2 = Zero!\n");
					break;
				}
				res = num1 / num2;
				printf("%d / %d = %d\n", num1, num2, res);
			case 'e':
				flag = 0;
				break;
			default:
				printf("No such command!\n");
		}
        memset(bufr,0,100);
        memset(bufr1,0,100);
        memset(bufr2,0,100);
	}
    
	return 0;
}


int getNum(char * bufr)
{
	printf("%s\n",bufr);
	
	int ten = 1, i = 0, res = 0;
	for (i = 0; i < strlen(bufr) - 1; i++)
	{
		ten *= 10;
	}
	for (i = 0; i < strlen(bufr); i++)
	{
		{res += (bufr[i] - '0') * ten;
		ten /= 10;}
	}
	return res;
}

