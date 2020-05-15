#define __bool_true_false_are_defined 1

#include "syscall.h"
#include "runtime.h"

static const char *logo[] = {
    "                                   .\"\"=.           _..._  ",
    "                                  (     \\        .'     `.",
    "                                  L      ;.~``. /,       ;",
    "                                   q     '   .`/;        p",
    "                       .=**.        \\     ;~` /;.       / ",
    "               .\"*.   r    p         \\;;  |_ /II;;,   .\"  ",
    "              ;    `. |.   .*\"``\"*~.._\"I..p``\"*~I;;.*`    ",
    "  ..~~**~._    ;     \\|;  (       _,;I::II;`.    `;       ",
    ".;;\"*~I;. `*.  '.    '|I.. ;~...;;II*'`.`.`        `.     ",
    "'      `I;   `. \\;; ,;qI;;I/ _=\"    .:.             i    ",
    "         \\I;.  `..I;II;\\II.=\"    .:.:.:;H:.:.        ;   ",
    "          \\III;._`.*III*`_.~   :.:.::.:;H.:.:.       b   ",
    "           `III;;..``**`` __:.:.;;;;;;II;H;;.:.      (    ",
    "             `IIIIII;;;;;;;;;;;;;;IIIIIII;I;.:.      ;    ",
    "               `*IIIIIIIIIIIIIIIIIII*\"@:( \\II;;.\"  / &.   ",
    "                   `\"*/:.=II*8:(`8::  q::. `.`r   r  .~*\"~",
    "                     ;:; ;:( q:b q::.  \\:;   `|   '  ..|  ",
    "                     LJ  '.'  `:\" `::   `     q  :;  `\",  ",
    "                                               `.  .&.,   ",
    "            . .  .`, .  _. , .. _.`_ ..`         `~`\"`    ",
    "         .`.:._.:.:;;226988888888888696222;;;:.::..       ",
    "      ,;`::;;2222296969688888888888888888888886922;:.`    ",
    "       .:.:;;;11122226969888888888888888888888888888888.  ",
    "       `&``;;;;12222222696969696988888888888696969669.`   ",
    "          .``:``:`.;;;;;122226969696696969696222269;`     ",
    "             `       `.`;``'`' ;;;\"`\":':`.``\"`.`  rscr   ",
};

static void set_fg(int color) {
    char buf[16];
    unsigned int pos = 0;
    buf[pos++] = '\033';
    buf[pos++] = '[';
    buf[pos++] = '0' + (color / 100) % 10;
    buf[pos++] = '0' + (color / 10) % 10;
    buf[pos++] = '0' + color % 10;
    while (pos > 2 && buf[2] == '0') {
        for (unsigned int i = 2; i + 1 < pos; i++)
            buf[i] = buf[i + 1];
        pos--;
    }
    buf[pos++] = 'm';
    sys_write(1, buf, pos);
}

static void reset_color(void) {
    sys_write(1, "\033[0m", 4);
}

extern "C" int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    set_fg(36);
    for (int i = 0; i < 26; i++) {
        sys_write(1, logo[i], u_strlen(logo[i]));
        sys_write(1, "\n", 1);
    }
    reset_color();

    struct linux_utsname uts;
    if (sys_uname(&uts) == 0) {
        u_puts("\n");
        set_fg(32);
        u_puts("  OS");
        reset_color();
        u_puts("          ");
        set_fg(36);
        u_puts(uts.sysname);
        reset_color();
        u_puts("\n");

        set_fg(32);
        u_puts("  Kernel");
        reset_color();
        u_puts("      ");
        set_fg(36);
        u_puts(uts.release);
        reset_color();
        u_puts("\n");

        set_fg(32);
        u_puts("  Host");
        reset_color();
        u_puts("        ");
        set_fg(36);
        u_puts(uts.nodename);
        reset_color();
        u_puts("\n");
    }

    unsigned int mem[5];
    if (sys_memstat(mem) == 0) {
        unsigned int total_mb = mem[0] / (1024u * 1024u);
        unsigned int used_mb = mem[3] / (1024u * 1024u);
        unsigned int free_mb = mem[4] / (1024u * 1024u);

        set_fg(32);
        u_puts("  Memory");
        reset_color();
        u_puts("       ");
        set_fg(36);
        u_put_uint(total_mb);
        u_puts(" MB");
        reset_color();
        u_puts(" (");
        set_fg(31);
        u_put_uint(used_mb);
        reset_color();
        u_puts(" used, ");
        set_fg(32);
        u_put_uint(free_mb);
        reset_color();
        u_puts(" free)\n");
    }

    u_puts("\n");
    return 0;
}
