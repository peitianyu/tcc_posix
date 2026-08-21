/* 测试: R10b System V IPC (msg/sem/shm) syscall 补全.
 * musl ipc 库层 (src/ipc/*.c) 走 SYS_msgget(68)..SYS_semtimedop(220),
 * PSX 接口层静态槽表实现 (src/ipc/_ipc.c). 覆盖:
 *  - msg: 创建/重复获取/EXCL/ENOENT, msgsnd/msgrcv 收发 + mtype 选择,
 *         MSG_NOERROR 截断, MSG_EXCEPT, 空队列 NOWAIT ENOMSG,
 *         满队列 NOWAIT EAGAIN, msgctl STAT/RMID
 *  - sem: 创建/重复/EXCL/ENOENT/EINVAL, semop P/V, 多操作原子,
 *         NOWAIT EAGAIN, semtimedop 超时 EAGAIN,
 *         semctl SETVAL/GETVAL/GETPID/GETALL/SETALL/STAT/RMID
 *  - shm: 创建/重复/EXCL/ENOENT/EINVAL/ENOMEM, shmat 映射+读写,
 *         nattch 计数, shmdt 解除, shmctl STAT/RMID
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (errno=%d)\n", msg, errno); fail++; } \
    else printf("  ok: %s\n", msg); \
} while (0)

/* sys/sem.h 未定义 union semun, 测试侧自定 (与 musl semctl.c 一致) */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct test_msg { long mtype; char mtext[64]; };

int main(void)
{
    int r, id, id2, i;
    struct test_msg m;
    struct msqid_ds msqds;
    struct semid_ds semds;
    struct shmid_ds shmds;
    union semun semarg;
    struct sembuf sop[2];
    unsigned short sarray[4];
    struct timespec ts;
    void *p, *p2;

    /*** msg ***/
    printf("[msg]\n");
    errno = 0;
    id = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    CHECK(id >= 0, "msgget(IPC_PRIVATE, CREAT)");
    if (id < 0) goto sem_part;

    id2 = msgget(0x504d, IPC_CREAT | 0600);   /* 'PM' */
    CHECK(id2 >= 0, "msgget(key, CREAT)");
    if (id2 < 0) goto sem_part;

    errno = 0;
    r = msgget(0x504d, 0);
    CHECK(r == id2, "msgget(key,0) 重复获取同 id");

    errno = 0;
    r = msgget(0x504d, IPC_CREAT | IPC_EXCL | 0600);
    CHECK(r == -1 && errno == EEXIST, "msgget(CREAT|EXCL) 已存在 -> EEXIST");

    errno = 0;
    r = msgget(0x504e, 0);
    CHECK(r == -1 && errno == ENOENT, "msgget 无 CREAT 不存在 -> ENOENT");

    /* 基础收发 */
    m.mtype = 1; memcpy(m.mtext, "hello", 6);
    errno = 0;
    r = msgsnd(id2, &m, 6, 0);
    CHECK(r == 0, "msgsnd");
    memset(&m, 0, sizeof(m));
    errno = 0;
    r = msgrcv(id2, &m, sizeof(m.mtext), 0, 0);
    CHECK(r == 6, "msgrcv len");
    CHECK(m.mtype == 1 && memcmp(m.mtext, "hello", 6) == 0, "msgrcv content");

    /* msgtyp > 0 选型 */
    m.mtype = 2; memcpy(m.mtext, "two", 4);
    msgsnd(id2, &m, 4, 0);
    m.mtype = 5; memcpy(m.mtext, "five", 5);
    msgsnd(id2, &m, 5, 0);
    memset(&m, 0, sizeof(m));
    r = msgrcv(id2, &m, sizeof(m.mtext), 5, 0);
    CHECK(r == 5 && m.mtype == 5 && memcmp(m.mtext, "five", 5) == 0, "msgrcv msgtyp>0");

    /* msgtyp < 0: 最低 mtype 且 <= -msgtyp */
    memset(&m, 0, sizeof(m));
    r = msgrcv(id2, &m, sizeof(m.mtext), -3, 0);
    CHECK(r == 4 && m.mtype == 2, "msgrcv msgtyp<0 最低");

    /* MSG_EXCEPT: 排除指定类型 */
    m.mtype = 3; memcpy(m.mtext, "aaa", 4);
    msgsnd(id2, &m, 4, 0);
    m.mtype = 7; memcpy(m.mtext, "bbb", 4);
    msgsnd(id2, &m, 4, 0);
    memset(&m, 0, sizeof(m));
    r = msgrcv(id2, &m, sizeof(m.mtext), 3, MSG_EXCEPT);
    CHECK(r == 4 && m.mtype == 7, "msgrcv MSG_EXCEPT");

    /* 清空残余 (还有 mtype=3 "aaa"), 保证后续截断测试独立 */
    while (msgrcv(id2, &m, sizeof(m.mtext), 0, IPC_NOWAIT) > 0) ;

    /* MSG_NOERROR 截断 */
    m.mtype = 1; memcpy(m.mtext, "truncate", 9);
    msgsnd(id2, &m, 9, 0);
    memset(&m, 0, sizeof(m));
    r = msgrcv(id2, &m, 4, 0, MSG_NOERROR);
    CHECK(r == 4 && memcmp(m.mtext, "trun", 4) == 0, "msgrcv MSG_NOERROR 截断");

    /* 清空残余, 准备空队列测试 */
    while (msgrcv(id2, &m, sizeof(m.mtext), 0, IPC_NOWAIT) > 0) ;

    /* 空队列 + NOWAIT -> ENOMSG */
    errno = 0;
    r = msgrcv(id2, &m, sizeof(m.mtext), 0, IPC_NOWAIT);
    CHECK(r == -1 && errno == ENOMSG, "msgrcv 空队列 NOWAIT -> ENOMSG");

    /* 满队列 (32 槽) + NOWAIT -> EAGAIN */
    {
        int full = 1;
        m.mtype = 1;
        for (i = 0; i < 32; i++)
            if (msgsnd(id2, &m, 1, IPC_NOWAIT) != 0) { full = 0; break; }
        errno = 0;
        r = msgsnd(id2, &m, 1, IPC_NOWAIT);
        CHECK(full == 1 && r == -1 && errno == EAGAIN, "msgsnd 满队列 NOWAIT -> EAGAIN");
    }
    while (msgrcv(id2, &m, sizeof(m.mtext), 0, IPC_NOWAIT) > 0) ;

    /* IPC_STAT: msg_qnum==0 */
    errno = 0;
    r = msgctl(id2, IPC_STAT, &msqds);
    CHECK(r == 0, "msgctl IPC_STAT");
    if (r == 0) CHECK(msqds.msg_qnum == 0, "msgctl msg_qnum==0");

    /* 无效 id */
    errno = 0;
    r = msgsnd(0x7fffffff, &m, 1, 0);
    CHECK(r == -1 && errno == EINVAL, "msgsnd 无效 id -> EINVAL");

    /* RMID 后无 CREAT -> ENOENT */
    errno = 0;
    r = msgctl(id2, IPC_RMID, 0);
    CHECK(r == 0, "msgctl IPC_RMID");
    errno = 0;
    r = msgget(0x504d, 0);
    CHECK(r == -1 && errno == ENOENT, "msgget 删除后 -> ENOENT");
    msgctl(id, IPC_RMID, 0);

sem_part:
    /*** sem ***/
    printf("[sem]\n");
    errno = 0;
    id = semget(0x5350, 3, IPC_CREAT | 0600);  /* 'SP' */
    CHECK(id >= 0, "semget(CREAT)");
    if (id < 0) goto shm_part;

    errno = 0;
    r = semget(0x5350, 3, 0);
    CHECK(r == id, "semget 重复获取同 id");

    errno = 0;
    r = semget(0x5350, 3, IPC_CREAT | IPC_EXCL | 0600);
    CHECK(r == -1 && errno == EEXIST, "semget(CREAT|EXCL) -> EEXIST");

    errno = 0;
    r = semget(0x5351, 3, 0);
    CHECK(r == -1 && errno == ENOENT, "semget 无 CREAT 不存在 -> ENOENT");

    errno = 0;
    r = semget(0x5350, 33, IPC_CREAT | 0600);
    CHECK(r == -1 && errno == EINVAL, "semget nsems=33 -> EINVAL");

    /* SETVAL/GETVAL */
    semarg.val = 1;
    errno = 0;
    r = semctl(id, 0, SETVAL, semarg);
    CHECK(r == 0, "semctl SETVAL(1)");
    errno = 0;
    r = semctl(id, 0, GETVAL, semarg);
    CHECK(r == 1, "semctl GETVAL==1");

    /* semop P(-1) */
    sop[0].sem_num = 0; sop[0].sem_op = -1; sop[0].sem_flg = 0;
    errno = 0;
    r = semop(id, sop, 1);
    CHECK(r == 0, "semop P(-1)");
    r = semctl(id, 0, GETVAL, semarg);
    CHECK(r == 0, "GETVAL==0 after P");

    /* 不可执行 + NOWAIT -> EAGAIN */
    sop[0].sem_op = -1; sop[0].sem_flg = IPC_NOWAIT;
    errno = 0;
    r = semop(id, sop, 1);
    CHECK(r == -1 && errno == EAGAIN, "semop NOWAIT -> EAGAIN");

    /* semop V(+1) */
    sop[0].sem_op = 1; sop[0].sem_flg = 0;
    errno = 0;
    r = semop(id, sop, 1);
    CHECK(r == 0, "semop V(+1)");
    r = semctl(id, 0, GETVAL, semarg);
    CHECK(r == 1, "GETVAL==1 after V");

    /* 多操作原子: [0]+2 [1]+1 一次应用 */
    sop[0].sem_num = 0; sop[0].sem_op = 2; sop[0].sem_flg = 0;
    sop[1].sem_num = 1; sop[1].sem_op = 1; sop[1].sem_flg = 0;
    errno = 0;
    r = semop(id, sop, 2);
    CHECK(r == 0, "semop 多操作");
    r = semctl(id, 0, GETVAL, semarg);
    CHECK(r == 3, "GETVAL[0]==3");
    r = semctl(id, 1, GETVAL, semarg);
    CHECK(r == 1, "GETVAL[1]==1");

    /* GETPID */
    errno = 0;
    r = semctl(id, 0, GETPID, semarg);
    CHECK(r > 0, "semctl GETPID>0");

    /* SETALL/GETALL */
    sarray[0] = 5; sarray[1] = 6; sarray[2] = 7;
    semarg.array = sarray;
    errno = 0;
    r = semctl(id, 0, SETALL, semarg);
    CHECK(r == 0, "semctl SETALL");
    memset(sarray, 0, sizeof(sarray));
    semarg.array = sarray;
    r = semctl(id, 0, GETALL, semarg);
    CHECK(r == 0 && sarray[0] == 5 && sarray[1] == 6 && sarray[2] == 7, "semctl GETALL");

    /* IPC_STAT: sem_nsems==3 */
    semarg.buf = &semds;
    errno = 0;
    r = semctl(id, 0, IPC_STAT, semarg);
    CHECK(r == 0 && semds.sem_nsems == 3, "semctl IPC_STAT sem_nsems==3");

    /* semtimedop 超时 -> EAGAIN (val[0]=0, P(-1) 10ms 超时) */
    semarg.val = 0;
    semctl(id, 0, SETVAL, semarg);
    sop[0].sem_num = 0; sop[0].sem_op = -1; sop[0].sem_flg = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 10000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    errno = 0;
    r = semtimedop(id, sop, 1, &ts);
    CHECK(r == -1 && errno == EAGAIN, "semtimedop 超时 -> EAGAIN");

    /* RMID 后 semop -> EINVAL */
    errno = 0;
    r = semctl(id, 0, IPC_RMID, semarg);
    CHECK(r == 0, "semctl IPC_RMID");
    errno = 0;
    r = semop(id, sop, 1);
    CHECK(r == -1 && errno == EINVAL, "semop 删除后 -> EINVAL");

shm_part:
    /*** shm ***/
    printf("[shm]\n");
    errno = 0;
    id = shmget(0x5348, 1024, IPC_CREAT | 0600);  /* 'SH' */
    CHECK(id >= 0, "shmget(CREAT)");
    if (id < 0) goto done;

    errno = 0;
    r = shmget(0x5348, 1024, 0);
    CHECK(r == id, "shmget 重复获取同 id");

    errno = 0;
    r = shmget(0x5348, 1024, IPC_CREAT | IPC_EXCL | 0600);
    CHECK(r == -1 && errno == EEXIST, "shmget(CREAT|EXCL) -> EEXIST");

    errno = 0;
    r = shmget(0x5349, 1024, 0);
    CHECK(r == -1 && errno == ENOENT, "shmget 无 CREAT 不存在 -> ENOENT");

    /* 已存在且请求更大 size -> EINVAL (Linux 语义) */
    errno = 0;
    r = shmget(0x5348, 2048, 0);
    CHECK(r == -1 && errno == EINVAL, "shmget 更大 size -> EINVAL");

    /* 新 key 超容量 -> ENOMEM */
    errno = 0;
    r = shmget(0x5350, 200000, IPC_CREAT | 0600);
    CHECK(r == -1 && errno == ENOMEM, "shmget 超容量 -> ENOMEM");

    /* shmat 映射 + 读写 */
    errno = 0;
    p = shmat(id, 0, 0);
    CHECK(p != (void *)-1 && p != 0, "shmat");
    if (p == (void *)-1 || p == 0) goto done;
    strcpy((char *)p, "shared-data");
    CHECK(memcmp(p, "shared-data", 12) == 0, "shm 写入读回");

    errno = 0;
    p2 = shmat(id, 0, 0);
    CHECK(p2 != (void *)-1 && p2 != 0, "shmat 第二次");
    if (p2 == (void *)-1 || p2 == 0) goto done;

    errno = 0;
    r = shmctl(id, IPC_STAT, &shmds);
    CHECK(r == 0, "shmctl IPC_STAT");
    if (r == 0) {
        CHECK(shmds.shm_nattch == 2, "shmctl STAT nattch==2");
        CHECK(shmds.shm_segsz == 1024, "shmctl STAT segsz==1024");
    }

    errno = 0;
    r = shmdt(p2);
    CHECK(r == 0, "shmdt");
    r = shmctl(id, IPC_STAT, &shmds);
    CHECK(r == 0 && shmds.shm_nattch == 1, "nattch==1 after shmdt");

    errno = 0;
    r = shmdt((void *)0x1234);
    CHECK(r == -1 && errno == EINVAL, "shmdt 无效地址 -> EINVAL");

    errno = 0;
    r = shmctl(id, IPC_RMID, 0);
    CHECK(r == 0, "shmctl IPC_RMID");
    errno = 0;
    p = shmat(id, 0, 0);
    CHECK(p == (void *)-1 && errno == EINVAL, "shmat 删除后 -> EINVAL");

done:
    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("ipc ok\n");
    return 0;
}
