/*
 * chat_client.c - IT4060 Chuong 4: Chat Client (Nang cao)
 * Dai hoc Bach Khoa Ha Noi (HUST)
 *
 * Tinh nang:
 *   - ncurses: man hinh chat cuon phia tren, input bar co dinh phia duoi
 *   - Mau sac: moi nickname mot mau rieng, phan loai tin nhan bang mau
 *   - Timestamp: [HH:MM:SS] truoc moi dong
 *   - Lich su lenh: phim Up/Down duyet lai lenh da go (toi da 100 lenh)
 *   - Tab-complete: go nick mot phan + Tab -> tu dien nickname
 *
 * Build:
 *   gcc -o chat_client chat_client.c -lpthread -lncurses
 *
 * Run:
 *   ./chat_client [server_ip] [port]
 *   (mac dinh: 127.0.0.1 8000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <ncurses.h>

/* --------------- CAU HINH --------------- */
#define DEFAULT_HOST    "127.0.0.1"
#define DEFAULT_PORT    8000
#define BUF_SIZE        4096
#define NICK_LEN        64
#define MAX_USERS       64
#define HIST_SIZE       100   /* so lenh luu trong history */
#define INPUT_MAX       512

/* --------------- MAU SAC (ncurses color pairs) --------------- */
#define C_DEFAULT   1   /* trang - text thuong */
#define C_SYSTEM    2   /* xanh la - thong bao he thong */
#define C_ERROR     3   /* do - loi */
#define C_TIMESTAMP 4   /* xam - timestamp */
#define C_PM        5   /* tim - tin nhan rieng */
#define C_OP        6   /* vang - OP/TOPIC */
#define C_NICK_1    7   /* cyan */
#define C_NICK_2    8   /* xanh duong */
#define C_NICK_3    9   /* xanh la dam */
#define C_NICK_4    10  /* do nhat */
#define C_NICK_5    11  /* magenta */
#define C_NICK_6    12  /* vang nhat */
#define C_INPUT_BAR 13  /* thanh input */
#define C_BORDER    14  /* duong ke */
#define C_MYNAME    15  /* ten minh - trang dam */
#define C_SENT      16  /* mau lenh minh gui */

/* --------------- TRANG THAI TOAN CUC --------------- */
static int   server_fd = -1;
static char  my_nick[NICK_LEN] = "";
static int   joined   = 0;
static volatile int running = 1;

/* Danh sach nguoi dung trong phong (de tab-complete) */
static char  users[MAX_USERS][NICK_LEN];
static int   user_count = 0;
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ncurses windows */
static WINDOW *chat_win  = NULL;  /* vung hien thi tin nhan */
static WINDOW *input_win = NULL;  /* thanh nhap lenh */
static WINDOW *sep_win   = NULL;  /* duong ke ngang ngan cach */
static pthread_mutex_t ncurses_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Lich su lenh */
static char  history[HIST_SIZE][INPUT_MAX];
static int   hist_count = 0;   /* so lenh da luu */
static int   hist_idx   = -1;  /* vi tri dang duyet (-1 = khong duyet) */

/* Buffer input hien tai */
static char  input_buf[INPUT_MAX];
static int   input_len = 0;
static int   input_cur = 0;   /* vi tri con tro trong input_buf */

/* ????????????????????????????????????????????????????
 *  TIEN ICH
 * ???????????????????????????????????????????????????? */

/* Lay chuoi timestamp HH:MM:SS */
static void get_timestamp(char *buf, int len) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%H:%M:%S", tm);
}

/* Hash nickname -> chi so mau C_NICK_1..C_NICK_6 */
static int nick_color(const char *nick) {
    unsigned int h = 5381;
    for (int i = 0; nick[i]; i++)
        h = ((h << 5) + h) + (unsigned char)nick[i];
    return C_NICK_1 + (h % 6);
}

/* Them user vao danh sach tab-complete */
static void add_user(const char *nick) {
    pthread_mutex_lock(&users_mutex);
    /* Kiem tra trung */
    for (int i = 0; i < user_count; i++)
        if (strcmp(users[i], nick) == 0) { pthread_mutex_unlock(&users_mutex); return; }
    if (user_count < MAX_USERS) {
        strncpy(users[user_count], nick, NICK_LEN - 1);
        users[user_count][NICK_LEN - 1] = '\0';
        user_count++;
    }
    pthread_mutex_unlock(&users_mutex);
}

/* Xoa user khoi danh sach */
static void remove_user(const char *nick) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i], nick) == 0) {
            memmove(users[i], users[i+1], (user_count - i - 1) * NICK_LEN);
            user_count--;
            break;
        }
    }
    pthread_mutex_unlock(&users_mutex);
}

/* Loai bo newline/CR cuoi chuoi */
static void trim_nl(char *s) {
    int n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

/* ????????????????????????????????????????????????????
 *  HIEN THI TIN NHAN LEN chat_win
 * ???????????????????????????????????????????????????? */

/*
 * Ham cot loi: in mot dong vao chat_win voi timestamp + mau sac.
 * Goi WScrollOK truoc de tu cuon.
 *
 * color_pair: mau cua phan noi dung chinh
 * prefix:     phan tieu de (vi du "[alice]"), co the NULL
 * prefix_color: mau cua prefix
 * content:    noi dung tin nhan
 */
static void chat_print(int color_pair, const char *prefix, int prefix_color,
                        const char *content) {
    pthread_mutex_lock(&ncurses_mutex);

    char ts[16];
    get_timestamp(ts, sizeof(ts));

    /* Timestamp xam */
    wattron(chat_win, COLOR_PAIR(C_TIMESTAMP));
    wprintw(chat_win, "[%s] ", ts);
    wattroff(chat_win, COLOR_PAIR(C_TIMESTAMP));

    /* Prefix (ten nguoi gui) */
    if (prefix) {
        wattron(chat_win, COLOR_PAIR(prefix_color) | A_BOLD);
        wprintw(chat_win, "%s ", prefix);
        wattroff(chat_win, COLOR_PAIR(prefix_color) | A_BOLD);
    }

    /* Noi dung */
    wattron(chat_win, COLOR_PAIR(color_pair));
    wprintw(chat_win, "%s\n", content);
    wattroff(chat_win, COLOR_PAIR(color_pair));

    wrefresh(chat_win);
    pthread_mutex_unlock(&ncurses_mutex);
}

/* In thong bao he thong (JOIN, QUIT, OP, TOPIC...) */
static void chat_system(const char *icon, const char *msg, int color) {
    char full[BUF_SIZE];
    snprintf(full, sizeof(full), "%s %s", icon, msg);
    chat_print(color, NULL, 0, full);
}

/* In lenh minh vua gui len man hinh - de chup minh chung */
static void echo_sent(const char *cmd) {
    pthread_mutex_lock(&ncurses_mutex);
    char ts[16];
    get_timestamp(ts, sizeof(ts));
    wattron(chat_win, COLOR_PAIR(C_TIMESTAMP));
    wprintw(chat_win, "[%s] ", ts);
    wattroff(chat_win, COLOR_PAIR(C_TIMESTAMP));
    wattron(chat_win, COLOR_PAIR(C_SENT) | A_BOLD);
    wprintw(chat_win, "[>>] ");
    wattroff(chat_win, COLOR_PAIR(C_SENT) | A_BOLD);
    wattron(chat_win, COLOR_PAIR(C_SENT));
    wprintw(chat_win, "%s\n", cmd);
    wattroff(chat_win, COLOR_PAIR(C_SENT));
    wrefresh(chat_win);
    pthread_mutex_unlock(&ncurses_mutex);
}


/* ????????????????????????????????????????????????????
 *  XU LY PHAN HOI TU SERVER
 * ???????????????????????????????????????????????????? */
static void handle_server_line(const char *line) {

    /* -- Ma phan hoi -- */
    if (strncmp(line, "100", 3) == 0) return;  /* OK, im lang */

    if (strncmp(line, "200", 3) == 0) {
        chat_system("[X]", "Nickname da duoc dung, hay chon ten khac.", C_ERROR); return;
    }
    if (strncmp(line, "201", 3) == 0) {
        chat_system("[X]", "Nickname khong hop le (chi chu thuong a-z va so 0-9).", C_ERROR); return;
    }
    if (strncmp(line, "202", 3) == 0) {
        chat_system("[X]", "Nickname khong ton tai trong phong.", C_ERROR); return;
    }
    if (strncmp(line, "203", 3) == 0) {
        chat_system("[X]", "Khong du quyen (chi OP moi duoc).", C_ERROR); return;
    }
    if (strncmp(line, "999", 3) == 0) {
        chat_system("!", "Loi khong xac dinh tu server.", C_ERROR); return;
    }

    /* -- Broadcast su kien -- */

    /* JOIN <nickname> */
    if (strncmp(line, "JOIN ", 5) == 0) {
        const char *nick = line + 5;
        add_user(nick);
        if (strcmp(nick, my_nick) == 0) {
            joined = 1;
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "Ban da vao phong voi ten [%s]. Go /help de xem lenh.", my_nick);
            chat_system("[OK]", msg, C_SYSTEM);
        } else {
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "%s da tham gia phong.", nick);
            chat_system("->", msg, C_SYSTEM);
        }
        return;
    }

    /* QUIT <nickname> */
    if (strncmp(line, "QUIT ", 5) == 0) {
        const char *nick = line + 5;
        remove_user(nick);
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "%s da roi phong.", nick);
        chat_system("<-", msg, C_SYSTEM);
        return;
    }

    /* MSG <nickname> <message> */
    if (strncmp(line, "MSG ", 4) == 0) {
        const char *rest = line + 4;
        const char *sp = strchr(rest, ' ');
        if (!sp) return;
        char nick[NICK_LEN];
        size_t nlen = (size_t)(sp - rest);
        if (nlen >= NICK_LEN) nlen = NICK_LEN - 1;
        strncpy(nick, rest, nlen); nick[nlen] = '\0';
        const char *msg = sp + 1;

        int is_me = (strcmp(nick, my_nick) == 0);
        char prefix[NICK_LEN + 4];
        snprintf(prefix, sizeof(prefix), "[%s]", nick);
        int pcol = is_me ? C_MYNAME : nick_color(nick);
        chat_print(C_DEFAULT, prefix, pcol, msg);
        return;
    }

    /* PMSG <nickname> <message> */
    if (strncmp(line, "PMSG ", 5) == 0) {
        const char *rest = line + 5;
        const char *sp = strchr(rest, ' ');
        if (!sp) return;
        char nick[NICK_LEN];
        size_t nlen = (size_t)(sp - rest);
        if (nlen >= NICK_LEN) nlen = NICK_LEN - 1;
        strncpy(nick, rest, nlen); nick[nlen] = '\0';
        const char *msg = sp + 1;

        char prefix[NICK_LEN + 16];
        snprintf(prefix, sizeof(prefix), "[PM/%s->ban]", nick);
        chat_print(C_PM, prefix, C_PM, msg);
        return;
    }

    /* OP <nickname> */
    if (strncmp(line, "OP ", 3) == 0) {
        const char *nick = line + 3;
        char msg[BUF_SIZE];
        if (strcmp(nick, my_nick) == 0)
            snprintf(msg, sizeof(msg), "Ban da duoc trao quyen chu phong (OP)!");
        else
            snprintf(msg, sizeof(msg), "%s hien la chu phong (OP).", nick);
        chat_system("[OP]", msg, C_OP);
        return;
    }

    /* KICK <kicked> <op> */
    if (strncmp(line, "KICK ", 5) == 0) {
        const char *rest = line + 5;
        const char *sp = strchr(rest, ' ');
        char kicked[NICK_LEN], by[NICK_LEN];
        if (sp) {
            size_t n = (size_t)(sp - rest);
            if (n >= NICK_LEN) n = NICK_LEN - 1;
            strncpy(kicked, rest, n); kicked[n] = '\0';
            strncpy(by, sp + 1, NICK_LEN - 1); by[NICK_LEN-1] = '\0';
        } else {
            strncpy(kicked, rest, NICK_LEN - 1); kicked[NICK_LEN-1] = '\0';
            strcpy(by, "?");
        }
        remove_user(kicked);
        char msg[BUF_SIZE];
        if (strcmp(kicked, my_nick) == 0) {
            snprintf(msg, sizeof(msg), "Ban da bi duoi khoi phong boi %s!", by);
            chat_system("[X]", msg, C_ERROR);
            running = 0;
        } else {
            snprintf(msg, sizeof(msg), "%s da bi duoi boi %s.", kicked, by);
            chat_system("[X]", msg, C_ERROR);
        }
        return;
    }

    /* TOPIC <op> <topic> */
    if (strncmp(line, "TOPIC ", 6) == 0) {
        const char *rest = line + 6;
        const char *sp = strchr(rest, ' ');
        char op[NICK_LEN]; const char *topic;
        if (sp) {
            size_t n = (size_t)(sp - rest);
            if (n >= NICK_LEN) n = NICK_LEN - 1;
            strncpy(op, rest, n); op[n] = '\0';
            topic = sp + 1;
        } else {
            strncpy(op, rest, NICK_LEN-1); op[NICK_LEN-1] = '\0';
            topic = "";
        }
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "Chu de: \"%s\" (dat boi %s)", topic, op);
        chat_system("[*]", msg, C_OP);
        return;
    }

    /* Raw fallback */
    chat_print(C_TIMESTAMP, NULL, 0, line);
}

/* ????????????????????????????????????????????????????
 *  THREAD NHAN DU LIEU TU SERVER
 * ???????????????????????????????????????????????????? */
static void *recv_thread(void *arg) {
    (void)arg;
    char buf[BUF_SIZE];
    char line_buf[BUF_SIZE];
    int  line_len = 0;

    while (running) {
        int n = recv(server_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (running)
                chat_system("!", "Mat ket noi toi server.", C_ERROR);
            running = 0;
            break;
        }
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                line_buf[line_len] = '\0';
                if (line_len > 0) handle_server_line(line_buf);
                line_len = 0;
            } else if (c != '\r' && line_len < BUF_SIZE - 1) {
                line_buf[line_len++] = c;
            }
        }
    }
    return NULL;
}

/* ????????????????????????????????????????????????????
 *  GUI LENH LEN SERVER
 * ???????????????????????????????????????????????????? */
static void send_cmd(const char *cmd) {
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "%s\n", cmd);
    if (send(server_fd, buf, len, 0) < 0)
        chat_system("!", "Loi gui lenh.", C_ERROR);
}

/* ????????????????????????????????????????????????????
 *  LICH SU LENH
 * ???????????????????????????????????????????????????? */
static void hist_push(const char *line) {
    if (line[0] == '\0') return;
    /* Khong luu trung lenh lien tiep */
    if (hist_count > 0 && strcmp(history[(hist_count-1) % HIST_SIZE], line) == 0) return;
    strncpy(history[hist_count % HIST_SIZE], line, INPUT_MAX - 1);
    history[hist_count % HIST_SIZE][INPUT_MAX - 1] = '\0';
    hist_count++;
    hist_idx = -1;
}

static void hist_up(void) {
    if (hist_count == 0) return;
    if (hist_idx == -1) hist_idx = hist_count - 1;
    else if (hist_idx > 0) hist_idx--;
    const char *entry = history[hist_idx % HIST_SIZE];
    strncpy(input_buf, entry, INPUT_MAX - 1);
    input_buf[INPUT_MAX - 1] = '\0';
    input_len = strlen(input_buf);
    input_cur = input_len;
}

static void hist_down(void) {
    if (hist_idx == -1) return;
    hist_idx++;
    if (hist_idx >= hist_count) {
        hist_idx = -1;
        input_buf[0] = '\0'; input_len = 0; input_cur = 0;
    } else {
        const char *entry = history[hist_idx % HIST_SIZE];
        strncpy(input_buf, entry, INPUT_MAX - 1);
        input_buf[INPUT_MAX - 1] = '\0';
        input_len = strlen(input_buf);
        input_cur = input_len;
    }
}

/* ????????????????????????????????????????????????????
 *  TAB-COMPLETE NICKNAME
 * ???????????????????????????????????????????????????? */
static void do_tab_complete(void) {
    /*
     * Tim token cuoi cung truoc con tro va thu complete bang nickname.
     * Vi du: "PMSG ali" -> "PMSG alice "
     */
    if (input_cur == 0) return;

    /* Tim diem bat dau cua tu cuoi */
    int word_start = input_cur - 1;
    while (word_start > 0 && input_buf[word_start - 1] != ' ') word_start--;

    char prefix[NICK_LEN];
    int prefix_len = input_cur - word_start;
    if (prefix_len <= 0 || prefix_len >= NICK_LEN) return;
    strncpy(prefix, input_buf + word_start, prefix_len);
    prefix[prefix_len] = '\0';

    /* Tim nickname khop */
    pthread_mutex_lock(&users_mutex);
    char match[NICK_LEN] = "";
    int  match_count = 0;
    for (int i = 0; i < user_count; i++) {
        if (strncasecmp(users[i], prefix, prefix_len) == 0) {
            strncpy(match, users[i], NICK_LEN - 1);
            match_count++;
        }
    }
    pthread_mutex_unlock(&users_mutex);

    if (match_count != 1) {
        /* Khong tim thay hoac nhieu ket qua -> khong lam gi / beep */
        if (match_count > 1) beep();
        return;
    }

    /* Thay the prefix bang match day du + khoang trang */
    int match_len = strlen(match);
    int tail_len  = input_len - input_cur;
    int new_len   = word_start + match_len + 1 + tail_len; /* +1 cho space */
    if (new_len >= INPUT_MAX) return;

    /* Xay dung buffer moi */
    char new_buf[INPUT_MAX];
    memcpy(new_buf, input_buf, word_start);
    memcpy(new_buf + word_start, match, match_len);
    new_buf[word_start + match_len] = ' ';
    memcpy(new_buf + word_start + match_len + 1, input_buf + input_cur, tail_len);
    new_buf[new_len] = '\0';

    memcpy(input_buf, new_buf, new_len + 1);
    input_len = new_len;
    input_cur = word_start + match_len + 1;
}

/* ????????????????????????????????????????????????????
 *  VE LAI THANH INPUT
 * ???????????????????????????????????????????????????? */
static void redraw_input(void) {
    pthread_mutex_lock(&ncurses_mutex);

    int width = getmaxx(input_win);

    /* Xoa thanh */
    werase(input_win);
    wattron(input_win, COLOR_PAIR(C_INPUT_BAR));
    wmove(input_win, 0, 0);

    /* Prompt */
    char prompt[NICK_LEN + 8];
    if (joined && my_nick[0])
        snprintf(prompt, sizeof(prompt), " [%s]> ", my_nick);
    else
        snprintf(prompt, sizeof(prompt), " > ");
    wattron(input_win, A_BOLD);
    waddstr(input_win, prompt);
    wattroff(input_win, A_BOLD);

    int prompt_len = strlen(prompt);
    int avail = width - prompt_len - 1;

    /* Hien thi phan input co the nhin thay (scroll ngang neu dai) */
    int view_start = 0;
    if (input_cur > avail - 1)
        view_start = input_cur - avail + 1;

    int print_len = input_len - view_start;
    if (print_len > avail) print_len = avail;
    if (print_len > 0)
        waddnstr(input_win, input_buf + view_start, print_len);

    wattroff(input_win, COLOR_PAIR(C_INPUT_BAR));

    /* Dat con tro */
    wmove(input_win, 0, prompt_len + (input_cur - view_start));
    wrefresh(input_win);

    pthread_mutex_unlock(&ncurses_mutex);
}

/* ????????????????????????????????????????????????????
 *  KHOI TAO NCURSES
 * ???????????????????????????????????????????????????? */
static void init_ncurses(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);   /* bat phim dac biet: mui ten, F-keys, v.v. */
    start_color();
    use_default_colors();

    /* Dinh nghia cac color pair */
    init_pair(C_DEFAULT,   COLOR_WHITE,   -1);
    init_pair(C_SYSTEM,    COLOR_GREEN,   -1);
    init_pair(C_ERROR,     COLOR_RED,     -1);
    init_pair(C_TIMESTAMP, COLOR_BLACK,   -1);   /* xam dam */
    init_pair(C_PM,        COLOR_MAGENTA, -1);
    init_pair(C_OP,        COLOR_YELLOW,  -1);
    init_pair(C_NICK_1,    COLOR_CYAN,    -1);
    init_pair(C_NICK_2,    COLOR_BLUE,    -1);
    init_pair(C_NICK_3,    COLOR_GREEN,   -1);
    init_pair(C_NICK_4,    COLOR_RED,     -1);
    init_pair(C_NICK_5,    COLOR_MAGENTA, -1);
    init_pair(C_NICK_6,    COLOR_YELLOW,  -1);
    init_pair(C_INPUT_BAR, COLOR_BLACK,   COLOR_CYAN);
    init_pair(C_BORDER,    COLOR_CYAN,    -1);
    init_pair(C_MYNAME,    COLOR_WHITE,   -1);
    init_pair(C_SENT,      COLOR_CYAN,    -1);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* Layout:
     *  row 0         : header
     *  row 1..rows-3 : chat window (cuon)
     *  row rows-2    : duong ke ngang
     *  row rows-1    : input bar
     */
    int chat_rows = rows - 3;

    /* Header */
    attron(COLOR_PAIR(C_BORDER) | A_BOLD);
    mvprintw(0, 0, "%-*s", cols,
             "  IT4060 Chat Room - HUST  |  Tab: complete nick  |  UpDown: lich su  |  /help");
    attroff(COLOR_PAIR(C_BORDER) | A_BOLD);
    refresh();

    /* Chat window - cuon tu dong */
    chat_win = newwin(chat_rows, cols, 1, 0);
    scrollok(chat_win, TRUE);       /* bat cuon tu dong */
    idlok(chat_win, TRUE);
    wattron(chat_win, COLOR_PAIR(C_DEFAULT));

    /* Duong ke ngang */
    sep_win = newwin(1, cols, rows - 2, 0);
    wattron(sep_win, COLOR_PAIR(C_BORDER) | A_BOLD);
    for (int i = 0; i < cols; i++) waddch(sep_win, ACS_HLINE);
    wrefresh(sep_win);

    /* Input bar */
    input_win = newwin(1, cols, rows - 1, 0);
    keypad(input_win, TRUE);        /* bat phim dac biet trong input_win */
}

/* ????????????????????????????????????????????????????
 *  XU LY LENH /help NOI BO
 * ???????????????????????????????????????????????????? */
static void show_help(void) {
    chat_system("-", "Danh sach lenh:", C_OP);
    chat_print(C_SYSTEM,  "JOIN <nick>",     C_OP,  "Tham gia phong (nick: chi a-z, 0-9)");
    chat_print(C_SYSTEM,  "MSG <noi dung>",  C_OP,  "Nhan tin ca phong");
    chat_print(C_SYSTEM,  "PMSG <nick> <msg>",C_OP, "Nhan rieng");
    chat_print(C_SYSTEM,  "OP <nick>",       C_OP,  "Chuyen quyen chu phong (can la OP)");
    chat_print(C_SYSTEM,  "KICK <nick>",     C_OP,  "Duoi nguoi dung (can la OP)");
    chat_print(C_SYSTEM,  "TOPIC <chu de>",  C_OP,  "Dat chu de phong (can la OP)");
    chat_print(C_SYSTEM,  "QUIT",            C_OP,  "Thoat khoi phong chat");
    chat_print(C_SYSTEM,  "Tab",             C_OP,  "Tu dien nickname");
    chat_print(C_SYSTEM,  "Up / Down",           C_OP,  "Duyet lich su lenh");
    chat_system("-", "Nhap text thuong (khong co lenh) -> tu gui MSG", C_OP);
}

/* ????????????????????????????????????????????????????
 *  MAIN - Ket noi va vong lap nhap lenh
 * ???????????????????????????????????????????????????? */
int main(int argc, char *argv[]) {
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    signal(SIGPIPE, SIG_IGN);

    /* -- Khoi tao ncurses truoc khi connect -- */
    init_ncurses();

    /* -- Ket noi TCP -- */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        endwin();
        perror("socket"); return 1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &srv.sin_addr) <= 0) {
        endwin();
        fprintf(stderr, "IP khong hop le: %s\n", host); return 1;
    }
    if (connect(server_fd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        endwin();
        perror("connect");
        fprintf(stderr, "Khong the ket noi toi %s:%d\n", host, port);
        return 1;
    }

    char conn_msg[BUF_SIZE];
    snprintf(conn_msg, sizeof(conn_msg), "Da ket noi toi %s:%d  ->  Go: JOIN <nickname>", host, port);
    chat_system("[OK]", conn_msg, C_SYSTEM);

    /* -- Khoi thread nhan -- */
    pthread_t rtid;
    pthread_create(&rtid, NULL, recv_thread, NULL);
    pthread_detach(rtid);

    /* -- Vong lap doc phim -- */
    redraw_input();

    while (running) {
        int ch = wgetch(input_win);  /* block cho phim */

        if (ch == ERR) continue;

        /* -- Phim dieu huong -- */
        if (ch == KEY_UP) {
            hist_up(); redraw_input(); continue;
        }
        if (ch == KEY_DOWN) {
            hist_down(); redraw_input(); continue;
        }
        if (ch == KEY_LEFT) {
            if (input_cur > 0) input_cur--;
            redraw_input(); continue;
        }
        if (ch == KEY_RIGHT) {
            if (input_cur < input_len) input_cur++;
            redraw_input(); continue;
        }
        if (ch == KEY_HOME || ch == 1 /* Ctrl+A */) {
            input_cur = 0; redraw_input(); continue;
        }
        if (ch == KEY_END || ch == 5 /* Ctrl+E */) {
            input_cur = input_len; redraw_input(); continue;
        }

        /* -- Xoa ky tu -- */
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (input_cur > 0) {
                memmove(input_buf + input_cur - 1,
                        input_buf + input_cur,
                        input_len - input_cur);
                input_len--;
                input_cur--;
                input_buf[input_len] = '\0';
                hist_idx = -1;
            }
            redraw_input(); continue;
        }
        if (ch == KEY_DC) {  /* Delete */
            if (input_cur < input_len) {
                memmove(input_buf + input_cur,
                        input_buf + input_cur + 1,
                        input_len - input_cur - 1);
                input_len--;
                input_buf[input_len] = '\0';
                hist_idx = -1;
            }
            redraw_input(); continue;
        }

        /* -- Tab complete -- */
        if (ch == '\t') {
            do_tab_complete();
            redraw_input(); continue;
        }

        /* -- Enter: gui lenh -- */
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            input_buf[input_len] = '\0';
            trim_nl(input_buf);
            if (input_len == 0) { redraw_input(); continue; }

            char line[INPUT_MAX];
            strncpy(line, input_buf, INPUT_MAX - 1);
            line[INPUT_MAX - 1] = '\0';

            /* Luu vao lich su */
            hist_push(line);

            /* Xoa input */
            input_buf[0] = '\0'; input_len = 0; input_cur = 0; hist_idx = -1;
            redraw_input();

            /* -- Xu ly lenh noi bo -- */
            if (strcmp(line, "/help") == 0 || strcmp(line, "HELP") == 0) {
                show_help(); continue;
            }

            /* Tach lenh */
            char upper[32] = {0};
            int ui = 0;
            for (int i = 0; line[i] && line[i] != ' ' && ui < 31; i++)
                upper[ui++] = toupper((unsigned char)line[i]);
            upper[ui] = '\0';

            /* JOIN - luu nick cuc bo */
            if (strcmp(upper, "JOIN") == 0) {
                const char *np = line + 4;
                while (*np == ' ') np++;
                strncpy(my_nick, np, NICK_LEN - 1);
                my_nick[NICK_LEN - 1] = '\0';
                char *sp = strchr(my_nick, ' ');
                if (sp) *sp = '\0';
                echo_sent(line);
                send_cmd(line);
                continue;
            }

            /* QUIT */
            if (strcmp(upper, "QUIT") == 0) {
                send_cmd(line);
                running = 0;
                break;
            }

            /* Text thuong (khong phai lenh da biet) -> gui MSG */
            int is_proto_cmd = (strcmp(upper,"MSG")  == 0 ||
                                strcmp(upper,"PMSG") == 0 ||
                                strcmp(upper,"OP")   == 0 ||
                                strcmp(upper,"KICK") == 0 ||
                                strcmp(upper,"TOPIC")== 0);
            if (is_proto_cmd) {
                echo_sent(line);
                send_cmd(line);
            } else if (joined) {
                char msg_cmd[INPUT_MAX + 8];
                snprintf(msg_cmd, sizeof(msg_cmd), "MSG %s", line);
                echo_sent(msg_cmd);
                send_cmd(msg_cmd);
            } else {
                echo_sent(line);
                send_cmd(line);
            }
            continue;
        }

        /* -- Ky tu thuong: chen vao vi tri con tro -- */
        if (ch >= 32 && ch < 256 && input_len < INPUT_MAX - 1) {
            memmove(input_buf + input_cur + 1,
                    input_buf + input_cur,
                    input_len - input_cur);
            input_buf[input_cur] = (char)ch;
            input_len++;
            input_cur++;
            input_buf[input_len] = '\0';
            hist_idx = -1;
            redraw_input();
        }
    }

    /* -- Don dep -- */
    close(server_fd);
    sleep(1);  /* cho thread recv in thong bao cuoi */
    endwin();  /* khoi phuc terminal */
    printf("Da thoat. Tam biet!\n");
    return 0;
}