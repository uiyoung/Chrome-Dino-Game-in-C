#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio_ext.h>

// keyboard values
#define UP 65
#define DOWN 66
#define ENTER '\n'

// colours 
#define C_DEF "\033[0m"
#define C_BLCK "\033[30m"
#define C_RED  "\033[31m"
#define C_GREN "\033[32m"
#define C_YLLW "\033[33m"
#define C_BLUE "\033[34m"
#define C_PRPL "\033[35m"
#define C_AQUA "\033[36m"
#define Def printf("%c[0m", 27);
#define Red printf("%c[1;31m", 27);
#define Blue printf("%c[1;34m", 27);
#define GREEN printf("%c[1;32m", 27);
#define YELLOW printf("%c[1;33m", 27);
#define PURPLE printf("%c[1;35m", 27);

// map size
#define ROW 10 
#define COLUMN 50

typedef struct _Item
{
    int invincibleTime; // 무적 남은 시간
    int bullets;        // 총알 수
    int speedUpTime;    // 스피드업 남은 시간
} Item;

typedef struct _Player
{
    char name[30];      // 이름 
    int xPos;   		// 유저 x좌표
    int yPos;   		// 유저 y좌표
    int heart;	    	// 유저 체력
    int timeRecord;     // 기록 시간 
    int score;	    	// 유저 점수
    int map[ROW][COLUMN];
    int isGameOver;
    Item item;
} Player;

typedef struct _Game
{
    Player p[2];        // 1p, 2p 구조체
    int players;        // 플레이어 수
    int elapsedTime;    // 게임 플레이 시간
    int speed;          // 게임 속도
    int key;            // 입력 키
    int highScore;      // 랭킹 최고 점수
    pthread_mutex_t mutx;
} Game;

typedef struct _SaveData    // 랭킹정보를 txt파일로부터 읽어 저장시킬 구조체
{
    char name[30];      // 플레이어 이름
    int score;          // 점수
    int timeRecord;     // 기록 시간
    char date[20];      // 플레이 날짜 
} SaveData;

// functions for menu UI
void printMenu(int menuPos);
int selectMenu(int *pos);
void printMenuDinosaur(int menuPos);

// functions which run on threads
void* t_timer(void*);       // 타이머
void* t_keyInput1(void*);   // 1p 키입력 체크
void* t_keyInput2(void*);   // 2p 키입력 체크
void* t_game(void*);        // 게임진행

// functions for in-game
void InitGame(Game* g, int playerNum);              // 게임 초기화
void Play(Game* game);                              // 게임 시작(스레드 생성 및 종료)
void maps(Game* g, int player, int *already_obs);   // 맵 그리기
void jump(Game* g, int player);                     // 플레이어 점프
void damage(Game* g, int player);                   // 데미지 체크
void get_Item(Game* g, int player);                 // 아이템 습득 체크
void Itm_heart(Player* p);                          // 하트 아이템 습득
void Itm_speed_up(Game* p);                         // 스피드업 아이템 습득
void return_speed(Game* g, int player);             // 기본 속도로 복귀

// functions for in-game UI
void PrintItems(Game* pg, int player);          // 아이템 표시
void PrintHearts(Player p);                     // 현재 하트 표시
void PrintStatusBar(Game* g, int player);       // 상태바 표시
void PrintGameOver(Player p);                   // 게임오버 스크린

// functions for ranking and fileIO 
void SaveScore(Game *g);                                 // 플레이 기록을 저장
int GetHighScore(SaveData* sd);                          // 최고점수 반환
void PrintRanking(Game *g, SaveData *sd, int sortby);    // 랭킹 출력
void PrintRankerASCII();                                 // 랭킹 아스키아트 출력
void PrintLine();

// functions for util
void hideCursor();          // 커서 숨기기
void displayCursor();       // 커서 재표시
void gotoxy(int x, int y);  
int getch();
int kbhit();

int main()
{
    int menuPos = 20;
    int isSelected = 0;
    Game game;
    Player p;                       // 랭킹에 쓰이는 플레이어 구조체 변수
    SaveData sd[10];                // 랭킹의 txt파일을 읽어와 저장시키는 구조체 배열 
    int highScore = 0;              // 랭킹의 최고점수 변수

    srand(time(NULL));
    pthread_mutex_init(&game.mutx, NULL);	// mutex 초기화 함수 
    hideCursor();

    while(1)
    {
        printMenu(menuPos);
        isSelected = selectMenu(&menuPos);
        game.highScore = GetHighScore(sd);

        if(isSelected)
        {
            switch(menuPos)
            {
                case 20:    // 1p 게임 시작
                    InitGame(&game, 1);
                    Play(&game);
                    SaveScore(&game);
                    PrintRanking(&game, sd, 0);
                    break;
                case 21:    // 2p 게임 시작
                    InitGame(&game, 2);
                    Play(&game);
                    SaveScore(&game);
                    PrintRanking(&game, sd, 0);
                    break;
                case 22:    // ranking 정보 출력
                    PrintRanking(&game, sd, 0);
                    break;
                case 23:
                    displayCursor();
                    gotoxy(1, 26);
                    pthread_mutex_destroy(&game.mutx);  // mutex 소멸 함수
                    return 0;
                default :
                    break;
            }
        }
    }
    return 0;
}

void Play(Game* game)
{
    pthread_t thread[4];
    void * t_result[4];

    pthread_create(&thread[0], NULL, t_timer, (void*)game);     // thread for timer
    pthread_create(&thread[1], NULL, t_keyInput1, (void*)game);	// thread for 1p keyInput
    pthread_create(&thread[2], NULL, t_keyInput2, (void*)game); // thread for 2p keyInput
    pthread_create(&thread[3], NULL, t_game, (void*)game);      // thread for game play

    pthread_join(thread[0], &t_result[0]);
    pthread_join(thread[1], &t_result[1]);
    pthread_join(thread[2], &t_result[2]);
    pthread_join(thread[3], &t_result[3]);
}

void* t_timer(void* g)
{
    Game *pg = (Game*)g;

    while(1)
    {
        // gameover시 스레드 종료
        if(pg->p[0].isGameOver && pg->p[1].isGameOver)
            break;

        (pg->elapsedTime)++;
        usleep(100000);
    }
}

void* t_game(void* g)
{
    Game *pg = (Game*)g;
    int already_obs = 0;

    while(1)
    {
        // 1p, 2p 모두 gameover시 스레드 종료
        if(pg->p[0].isGameOver && pg->p[1].isGameOver)
        {
            puts("press any key to continue...");
            break;
        }

        system("clear");

        for(int i=0;i<COLUMN*2;i++)
            printf("─");
        putchar('\n');

        // 1p인 경우
        if(pg->p[0].heart>0)
        {
            PrintStatusBar(pg, 0);                  // 상태바
            maps(pg, 0, &already_obs);              // map 출력
            pg->p[0].timeRecord = pg->elapsedTime;  // 시간 기록
        }
        else
        {
            pg->p[0].isGameOver=1;
            PrintGameOver(pg->p[0]);    // 게임오버 스크린
        }


        // 2p인 경우(1p 아래에 2p 플레이화면 출력)
        if(pg->players==2)
        {
            for(int i=0;i<COLUMN*2;i++)
                printf("─");

            putchar('\n');

            if(pg->p[1].heart>0)
            {
                PrintStatusBar(pg, 1);                  // 상태바
                maps(pg, 1, &already_obs);
                pg->p[1].timeRecord = pg->elapsedTime;
            }
            else
            {
                pg->p[1].isGameOver=1;
                PrintGameOver(pg->p[1]);    // 게임오버 스크린
            }
        }

        for(int i=0;i<COLUMN*2;i++)
            printf("─");
        putchar('\n');

        usleep(pg->speed);
    }
}

void* t_keyInput1(void* g)
{
    Game *pg = (Game*)g;

    while(1)
    {
        // 1p game over시 스레드 종료
        if(pg->p[0].isGameOver)
            break;

        pg->p[0].map[pg->p[0].yPos][pg->p[0].xPos]=0;	// 잔상을 없애기 위해 초기화
        pthread_mutex_lock(&(pg->mutx));
        pg->key=getch();  

        if(pg->key==27 || pg->key==91)
        {
            if(pg->key == 27) 
                pg->key = getch();

            if(pg->key==91)
            {
                pg->key=getch();
                if(pg->key==65)
                {
                    if(pg->p[0].item.bullets==0)
                    {
                        pg->key=0;
                        pg->p[0].map[pg->p[0].yPos][pg->p[0].xPos]=0;
                        pthread_mutex_unlock(&(pg->mutx));
                        usleep(5000);
                        jump(pg, 0);
                    }
                    else
                    {
                        pg->p[0].map[pg->p[0].yPos][pg->p[0].xPos+1]=77;
                        pg->p[0].item.bullets-=1;
                        pthread_mutex_unlock(&(pg->mutx));
                    }
                }
                else
                {
                    pthread_mutex_unlock(&(pg->mutx));
                    usleep(100);
                }
            }
        }
        else
        {
            pthread_mutex_unlock(&(pg->mutx));
            usleep(100);
        }
        __fpurge(stdin);
    }
}

void* t_keyInput2(void* g)
{
    Game *pg = (Game*)g;

    while(1)
    {
        // 2p game over시 스레드 종료
        if(pg->p[1].isGameOver)
            break;

        pthread_mutex_lock(&(pg->mutx));

        if(pg->key == 'w');
        else pg->key=getch();

        if(pg->key == 'w')
        {
            if(pg->p[1].item.bullets> 0)
            {
                pg->key=0;
                pthread_mutex_unlock(&(pg->mutx));
                usleep(100);
                pg->p[1].map[pg->p[1].yPos][pg->p[1].xPos+1]=77;
                pg->p[1].item.bullets-=1;
                pthread_mutex_unlock(&(pg->mutx));
            }
            else
            {
                pg->key=0;
                pthread_mutex_unlock(&(pg->mutx));
                usleep(100);
                jump(pg, 1);
            }
        }
        else 
        {
            pthread_mutex_unlock(&(pg->mutx));
            usleep(100);
        }
        __fpurge(stdin);
    }
}

void PrintHearts(Player p)
{
    Red
        for(int i=0;i<p.heart;i++)
            printf("♥ ");
    for(int i=3;i>p.heart;i--)
        printf("♡ ");
    Def
}

void PrintItems(Game* pg, int player)
{
    if(pg->p[player].item.speedUpTime>0)
        printf(" |빨라짐 %ds| ", (pg->p[player].item.speedUpTime-pg->elapsedTime)/10);
    if(pg->p[player].item.bullets>0)
        printf(" |총알 %d발| ", pg->p[player].item.bullets);
    if(pg->p[player].item.invincibleTime>0)
        printf(" |무적 %ds| ", (pg->p[player].item.invincibleTime-pg->elapsedTime)/10);
}

void PrintStatusBar(Game* pg, int player)
{
    printf(" hi-score %d score %d time %d.%ds", (pg->highScore > pg->p[player].score ? pg->highScore : pg->p[player].score), pg->p[player].score, (pg->elapsedTime)/10, (pg->elapsedTime)%10);
    putchar('\n');
    putchar(' ');
    PrintHearts(pg->p[player]);
    PrintItems(pg, player);
    putchar('\n');
}

void InitGame(Game* g, int playerNum)
{
    for(int i=0;i<2;i++)
    {
        // init player
        g->p[i].xPos = 2;
        g->p[i].yPos = ROW-2;
        g->p[i].score = 0;
        g->p[i].timeRecord = 0;
        g->p[i].heart = 3;		
        g->p[i].isGameOver = 0;
        g->p[i].item.invincibleTime = 0;
        g->p[i].item.bullets= 3;
        g->p[i].item.speedUpTime= 0;

        // init map array
        for(int j=0;j<ROW;j++)
        {
            for(int k=0;k<COLUMN;k++)
            {
                if(j==ROW-1)
                    g->p[i].map[j][k]=3;
                else
                    g->p[i].map[j][k]=0;
            }
        }
    }

    // init Game
    g->players = playerNum;
    g->speed = 100000;	
    g->elapsedTime = 0;

    // 1p 게임시작시 2p는 게임오버 상태로 만들기
    if(g->players == 1)
        g->p[1].isGameOver = 1;
}

void maps(Game* g, int player, int* already_obs)
{
    int appear=COLUMN-1;            // 변할 장애물

    if(*already_obs<=0 && rand()%10==1)             // 장애물 등장 확률
    {
        g->p[player].map[ROW-2][appear] = 2;
        *already_obs = 3;                          // 장애물 최소간격 설정
    }
    else
        *already_obs -= 1;

    // 아이템 출현 확률
    if(rand()%200==1)
        g->p[player].map[ROW-4][appear]=11; // 하트
    else if(rand()%400==1)
        g->p[player].map[ROW-4][appear]=22; // 별 : 무적
    else if(rand()%300==1)
        g->p[player].map[ROW-4][appear]=33; // 화살표 : 총
    else if(rand()%100==1)
        g->p[player].map[ROW-4][appear]=44; // 음표 : 스피드업

    get_Item(g, player);

    // 데미지 체크
    damage(g, player);

    // 속도복귀
    return_speed(g,player);

    // 장애물 뛰어 넘었을 시 점수+2
    if(g->p[player].yPos!=ROW-2 && g->p[player].map[ROW-2][g->p[player].xPos]==2)
        g->p[player].score+=2;

    // 플레이어의 위치에 1삽입 
    if(g->p[player].yPos==ROW-2)
    {
        g->p[player].map[ROW-3][g->p[player].xPos]=0;
        g->p[player].map[g->p[player].yPos][g->p[player].xPos]=1;
    }
    else if((g->p[player].yPos==ROW-3) || (g->p[player].yPos==ROW-4))
        g->p[player].map[g->p[player].yPos][g->p[player].xPos]=4;   // 떠있을때 모양

    // draw objects
    for(int y=0;y<ROW;y++)
    {
        for(int x=0;x<COLUMN;x++)
        {
            if(g->p[player].map[y][x]==0)
                printf("  ");
            else if(g->p[player].map[y][x]==1)		// @ 플레이어
                printf("%s@ %s", C_GREN, C_DEF);
            else if(g->p[player].map[y][x]==2)
                printf("▲ ");
            else if(g->p[player].map[y][x]==3)
                printf("■ ");
            else if(g->p[player].map[y][x]==4)      // @ 플레이어 점프 시
                printf("%s@ %s", C_GREN, C_DEF);
            else if(g->p[player].map[y][x]==11)
                printf("%s♥ %s", C_RED, C_DEF);
            else if(g->p[player].map[y][x]==22)
                printf("%s★ %s", C_YLLW, C_DEF);
            else if(g->p[player].map[y][x]==33)
                printf("%s▣ %s", C_GREN, C_DEF);
            else if(g->p[player].map[y][x]==44)
                printf("%s♬ %s", C_AQUA, C_DEF);
            else if(g->p[player].map[y][x]==77)
                printf("%s┏√守━──%s", C_YLLW, C_DEF);
        }
        putchar('\n');
    }

    // 오른쪽으로 한칸 씩 이동(쏘면 벽 바로 없어짐)
    for(int i=0;i<ROW;i++)
    {
        for(int j=0;j<COLUMN;j++)
        {
            if(g->p[player].map[i][j]==77)
            {
                if(j==COLUMN-1)
                    g->p[player].map[i][j]=0;
                else if(g->p[player].map[i][j+1]==2)    // 가야할 칸에 장애물이 있다.
                {
                    g->p[player].map[i][j]=0;
                    g->p[player].map[i][j+1]=0;
                    g->p[player].score+=1;
                }
                else
                {
                    g->p[player].map[i][j+1]=g->p[player].map[i][j];
                    g->p[player].map[i][j]=0;
                }
            }
        }
    }

    // 맵을 왼쪽으로 한칸 씩 이동
    for(int i=0;i<ROW;i++)
    {
        if(i!=ROW-1)
            g->p[player].map[i][0]=0;

        for(int j=1;j<COLUMN;j++)
        {
            if(g->p[player].map[i][j]!=1 && g->p[player].map[i][j]!=4 && i>1 && i<ROW-1)
            {
                g->p[player].map[i][j-1] = g->p[player].map[i][j];
                g->p[player].map[i][j]=0;
            }
        }
    }
}

void jump(Game *g, int player)
{
    g->p[player].yPos=ROW-3;
    usleep(g->speed);
    g->p[player].yPos=ROW-4;
    usleep(g->speed);
    g->p[player].yPos=ROW-3;
    usleep(g->speed);
    g->p[player].yPos=ROW-2;
    usleep(g->speed);
}

void PrintGameOver(Player p)
{
    int sec = p.timeRecord/10;
    int sec2 = p.timeRecord%10;

    printf("score : %d time : %d.%d\n", p.score, sec, sec2);
    PrintHearts(p);

    for(int i=0;i<ROW;i++)
    {
        if(i == ROW-1)
            puts("    @...");
        if(i == 4)
            puts("\t\t\t\t\t    Game Over");
        else if(i == 5)
            printf("\t\t\t\t\t    · %d.%d sec\n", sec, sec2);
        else if(i == 6)
            printf("\t\t\t\t\t    · %d point\n", p.score);
        else
            puts("");
    }
}

void Itm_heart(Player* p)
{
    if((p->heart) < 3)
        (p->heart) += 1;
}

void Itm_speed_up(Game* g)
{
    g->speed/=2;  
}

void return_speed(Game* g, int player)
{
    if(g->p[player].item.speedUpTime<=g->elapsedTime && g->p[player].item.speedUpTime!=0)
    {
        g->speed*=2;
        g->p[player].item.speedUpTime=0;
    }
}

void get_Item(Game* g, int player)
{
    if(g->p[player].map[g->p[player].yPos][g->p[player].xPos] == 11)
    {
        Itm_heart(&g->p[player]);
        g->p[player].score+=1;
    }

    if(g->p[player].map[g->p[player].yPos][g->p[player].xPos] == 22)
    {
        int temp_time = g->elapsedTime;
        g->p[player].item.invincibleTime = temp_time + 100;
        g->p[player].score+=1;
    }

    if(g->p[player].map[g->p[player].yPos][g->p[player].xPos] == 33)
    {
        g->p[player].item.bullets = 10;
        g->p[player].score+=1;
    }

    if(g->p[player].map[g->p[player].yPos][g->p[player].xPos] == 44)
    {
        int temp_time = g->elapsedTime;
        Itm_speed_up(g);
        g->p[player].item.speedUpTime = temp_time + 100;
        g->p[player].score+=1;
    }
}

void damage(Game* g, int player)
{
    if(g->p[player].item.invincibleTime <= g->elapsedTime)
        g->p[player].item.invincibleTime = 0;

    if(g->p[player].item.invincibleTime == 0)
    {
        if(g->p[player].map[g->p[player].yPos][g->p[player].xPos]==2&&g->p[player].heart>0)
            g->p[player].heart-=1;
    }
}

void printMenu(int menuPos)
{
    char instructions[][100] = {
        "  1인 게임을 시작합니다(점프:↑키).",
        "  2인 게임을 시작합니다(1p 점프:↑키, 2p 점프:w키).",
        "  랭킹기록을 확인합니다.",
        "  게임을 종료합니다."
    };

    system("clear");
    printMenuDinosaur(menuPos);
    gotoxy(1, 20);
    puts("  1p");
    puts("  2p");
    puts("  ranking");
    puts("  exit");
    printf("\n%s\n", instructions[menuPos-20]);
    gotoxy(1, menuPos);
    puts(">");
}

int selectMenu(int *pos)
{
    int isSelected = 0;
    int ch = getch();

    if(ch == UP || ch == 'w' || ch == 'W')
    {
        (*pos)--;
        if(*pos < 20)
            *pos = 23;
    }
    else if(ch == DOWN || ch == 's' || ch == 'S')
    {
        (*pos)++;
        if(*pos > 23)
            *pos = 20;
    }
    else if(ch == ENTER)
        isSelected = 1;

    return isSelected;
}

void printMenuDinosaur(int menuPos)
{
    int max = (menuPos-18)*2;
    for(int i=1;i<max+1;i++)
    {
        GREEN
            gotoxy(i,1);
        puts("");
        gotoxy(i,2);
        puts("                                      ████████");
        gotoxy(i,3);
        puts("                                     ███▄███████");
        gotoxy(i,4);
        puts("                                     ███████████");
        gotoxy(i,5);
        puts("                                     ███████████");
        gotoxy(i,6);
        puts("                                     ██████");
        gotoxy(i,7);
        puts("                                     █████████");
        gotoxy(i,8);
        puts("                           █       ███████");
        gotoxy(i,9);
        puts("                           ██    ████████████");
        gotoxy(i,10);
        puts("                           ███  ██████████  █");
        gotoxy(i,11);
        puts("                  ██████   ███████████████");
        gotoxy(i,12);
        puts("                 ███▄█████ ███████████████");
        gotoxy(i,13);
        puts("                 █████      █████████████");
        gotoxy(i,14);
        puts("                █████████    ███████████");
        gotoxy(i,15);
        puts("    ░▄▄▄▄░ █    ███████         ████████");
        gotoxy(i,16);
        puts("    ◄██▄▀▀ ██  ██████████        ███  ██");
        gotoxy(i,17);
        puts("    ◄███▀▀  ██████████  █        ██    █");
        gotoxy(i,18);
        puts(" ◄█░◄███▀░     ██    █           █     █");
        gotoxy(i,19);
        puts("  ▀▀████▄▒     ███   ██          ██    ██");
        Def
    }
}

void gotoxy(int x, int y)
{
    printf("\e[%d;%df",y,x);
    fflush(stdout);
}

int getch()
{
    int c;
    struct termios oldattr, newattr;

    tcgetattr(STDIN_FILENO, &oldattr);           // 현재 터미널 설정 읽음
    newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);         // CANONICAL과 ECHO 끔
    newattr.c_cc[VMIN] = 1;                      // 최소 입력 문자 수를 1로 설정
    newattr.c_cc[VTIME] = 0;                     // 최소 읽기 대기 시간을 0으로 설정
    tcsetattr(STDIN_FILENO, TCSANOW, &newattr);  // 터미널에 설정 입력
    c = getchar();                               // 키보드 입력 읽음
    tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);  // 원래의 설정으로 복구

    return c;
}

void hideCursor()
{
    printf("\e[?25l");
    fflush(stdout);
}

void displayCursor()
{
    printf("\e[?25h");
    fflush(stdout);
}

int kbhit()	
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

void SaveScore(Game *g)
{
    FILE *fp = fopen("Ranking.txt", "a");
    const time_t timer = time(NULL);        // 현재 시각을 초단위로 얻는다
    struct tm *t= localtime(&timer);    // 초 단위의 시간을 분리하여 구조체에 넣는다

    char tempName[2][30];   // 1p, 2p 이름 임시 저장

    int year = t->tm_year + 1900;
    int month = t->tm_mon+1;
    int day = t->tm_mday;
    int dayOfWeek = t->tm_wday;  // 일:0, 월:1, 화:2, 수:3, 목:4, 금:5, 토:6
    char dayNames[][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};         

    char date[20];
    // 년,월,일,요일 한 문자열로 합치기
    sprintf(date, "%d-%d-%d(%s)", year, month, day, dayNames[dayOfWeek]);

    // 플레이어 수 만큼 이름을 입력받고 정보 저장
    for(int i=0;i<g->players;i++)
    {
        printf("%dp name>>", i+1);
        __fpurge(stdin);
        scanf("%s", tempName[i]);
        strcpy(g->p[i].name, tempName[i]);

        fprintf(fp, "%s %d %d %s\n", g->p[i].name, g->p[i].timeRecord, g->p[i].score, date);
    }
    fclose(fp);
}

int GetHighScore(SaveData* sd)
{
    FILE *fp = fopen("Ranking.txt", "r");    // 읽기형식으로 파일오픈
    int highScore = 0;
    int num = 0;                            // 파일의 크기를 카운트하기 위한 변수

    if(fp==NULL)
    {
        return highScore;
    }

    // 파일의 끝 줄 까지 읽기
    while(!feof(fp))
    {
        // SaveData 구조체에 저장
        fscanf(fp, "%s %d %d %s", sd[num].name, &sd[num].timeRecord, &sd[num].score, sd[num].date);
        num++;
    }

    // 파일의 크기만큼 반복문을 돌려서 최고점수 반환
    for(int i=0;i<num-1;i++)          
    {
        if (sd[i].score > highScore) 
            highScore = sd[i].score;
    }

    fclose(fp);

    return highScore;
}

void PrintRanking(Game *g, SaveData *sd, int sortby)
{
    int num=0;                                              // 파일의 크기를 카운트 하기 위한 변수
    int t_score, t_timeRecord, t_year, t_month, t_day;      // int형 버블정렬을 위한 각각의 temp 변수
    char t_name[30], t_dayName[10];
    char rank[10][15] = {"[1st]","[2nd]","[3rd]","[4th]","[5th]","[6th]","[7th]","[8th]","[9th]","[10th]"};                               
    int turn1, turn2 = 0;    // 1p, 2p 이름 혹은 점수가 같을시의 오류를 방지하기 위한 변수
    FILE *fp = fopen("Ranking.txt", "r");

    __fpurge(stdin);
    system("clear");

    if(fp==NULL)
    {
        PrintRankerASCII();
        printf("\t\t\t\t\t[EMPTY RANKING]\n");
        Red
            printf("\t\t\t\t[couldn't find the Ranking.txt File]\n");
        Def
        printf("\n\n");
        printf(" < 이전 메뉴로(Enter[<┘])\n");

        if(getch() == ENTER)
            return;
        else 
            PrintRankerASCII();
        return;
    }

    while(!feof(fp))
    {
        fscanf(fp, "%s %d %d %s", sd[num].name, &sd[num].timeRecord, &sd[num].score, sd[num].date);
        num++;
    }

    // 파일의 크기만큼 반복하여 버블정렬(내림차순)
    if(sortby == 0)
    {
        // score 기준 정렬
        for(int i=0;i<num;i++)
        {
            for(int j=0;j<num-1;j++)
            {
                if(sd[j].score < sd[j+1].score)
                {
                    SaveData temp = sd[j];
                    sd[j] = sd[j+1];
                    sd[j+1] = temp;
                }
            }
        }
    }
    else if(sortby == 1)
    {
        // timeRecord 기준 정렬
        for(int i=0;i<num;i++)
        {
            for(int j=0;j<num-1;j++)
            {
                if(sd[j].timeRecord < sd[j+1].timeRecord)
                {
                    SaveData temp = sd[j];
                    sd[j] = sd[j+1];
                    sd[j+1] = temp;
                }
            }
        }
    }

    PrintRankerASCII();
    Red
        printf("Rank\t\t\t");
    Blue
        printf("NAME\t\t\t");
    PURPLE
        printf("SCORE\t\t");
    YELLOW
        printf("TIME\t\t");
    GREEN
        printf("    DATE\n");
    Def
        PrintLine();

    int maxLine = num-1>10 ? 10 : num-1;    // 출력할 랭킹 라인 수

    // print ranks 
    for(int i=0;i<maxLine;i++)
    {
        putchar('\n');

        // 1p인 경우(본인 점수 빨간색으로 출력)
        if(g->players==1)
        {
            //이전의 정보들과 플레이 한 판의 이름과, 점수가 같을 경우 (본인의 등수 확인을 위함)
            if(strcmp(g->p[0].name, sd[i].name)==0 && g->p[0].score == sd[i].score)
            {
                if(turn1==0) 
                    Red
                else
                {
                    turn1 = 1;
                    Def
                }
            }
            else
                Def
        }
        // 2p인 경우(1p 빨간색, 2p 보라색 출력)
        else if(g->players==2)
        {
            if(strcmp(g->p[0].name, sd[i].name)==0 && g->p[0].score == sd[i].score)
            {
                if(turn1==0) 
                    Red
                else
                {
                    turn1 = 1;
                    Def
                }
            }
            else if(strcmp(g->p[1].name, sd[i].name)==0 && g->p[1].score == sd[i].score)
            {
                if(turn2==0) 
                    Blue
                else
                {
                    turn2 = 1;
                    Def
                }
            }
            else
                Def
        }

        printf("%s\t\t\t%s\t\t\t%d\t\t%d.%ds\t\t    %s\n", rank[i], sd[i].name, sd[i].score, sd[i].timeRecord/10, sd[i].timeRecord%10, sd[i].date);
        Def
            PrintLine();
    }
    fclose(fp);

    putchar('\n');
    putchar('\n');
    puts(" [1]점수 기준 정렬  [2]플레이타임 기준 정렬");
    putchar('\n');
    printf(" < 이전 메뉴로(Enter[<┘])\n");

    int key = getch();
    if(key == ENTER) 
        return;
    else if(key == '1')
        PrintRanking(g, sd, 0);
    else if(key == '2')
        PrintRanking(g, sd, 1);
    else 
        PrintRanking(g, sd, 0);
}

void PrintLine()
{
    for (int i=0;i<100;i++) 
        printf("=");
}

void PrintRankerASCII(void)
{
    puts("");
    Red
        puts("       .@@@@!@@@-          @$        -@@@      ,#@@-   -@@@!    :@@*   .#@@@##@@@@@    *@@@*$@@*  ");
    puts("       .@@    @@!        .@@          @@        @      @@@     @@       @@       @     @@    @@@  ");
    YELLOW
        puts("       @@~    -@@       :~@@         !.@@      =:      @@    @!        @@=       .    @@@     @@. ");
    puts("      ~@@     @@;       $  @@-       @  @@     @      @@; $@           @@             @@     @@@  ");
    Blue
        puts("      @@$    @@;       @  $@#       ,*  @@,   ,$      @@,@!           *@@    @       :@@    #@@   ");
    puts("      @@@@@@@*        @   ,@@       @   ,@@   #.     .@@@@*           @@@@@@@#       @@@@@@@#     ");
    GREEN
        puts("      @@  @@~        ,=    @@       @    @@   @      @@@.@@           @@    =,       @@  #@$      ");
    puts("     !@@  !@@        @.....@@       @    *@$  @      @@, @@@         ,@@    ~        @@  .@@      ");
    PURPLE
        puts("     @@    @@!      @      @@~     @      @@~@      :@@   =@@        @@.            @@~   $@@     ");
    puts("    $@@     @@    .@       !@@    .@       @@$      @@,    ~@@      ;@@        @   -@@     @@!    ");
    Def
        puts("    @@;     #@#   @!       ,@@    @;       ;@.     .@@      @@*     @@$      -@!   #@@     -@@    ");
    puts("   *@@@      @@, @@@       @@@*  -@#        @      @@@,     @@@,   :@@@@$#@@@@@   .@@@      @@=   ");
    printf("\n\n");
    puts("                              @@@*          #@@;     @:@@!@   *@@=*!@@!!*@@=  ");
    PURPLE
        puts("                              =@@           !@@    @@     =@  @.    @@    @.  ");
    puts("                              @@=           #@$    @@      *  !    =@@    :   ");
    GREEN
        puts("                              @@            @@     @@=             @@         ");                    
    puts("                             @@~           @@!      @@@-          $@@         ");
    Blue
        puts("                             @@            @@        @@@,         @@:         ");                    
    puts("                            ,@@            @@         @@@         @@          ");                    
    puts("                            @@#           #@@          @@@       -@@          ");
    YELLOW
        puts("                            @@            @@            @@       @@.          ");                    
    puts("                           @@!       :!  @@=    *#      @@      ;@@           ");
    Red
        puts("                           @@-      $@   @@~    #@.    *@       @@$           ");                    
    puts("                         ,@@@@@@@@@@@@ .@@@@,    @@$.,@@,     -@@@@~          ");
    printf("\n\n");
    Def
}