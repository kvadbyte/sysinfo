/*
 * sysinfo.c - one-shot Linux system report built from /proc files.
 * Only <stdio.h> is used; all string helpers are hand-written.
 *
 * Options:
 *   --version    print the program version and exit
 *   --no-color   disable ANSI colors (useful when piping to a file)
 *   --help       show usage
 *
 * Build: gcc -std=c99 -Wall -Wextra -pedantic -o sysinfo sysinfo.c
 */
#include <stdio.h>

#define VERSION_STRING "1"   /* reported by --version */
#define LINE_SIZE 512        /* size of every line buffer */
#define BAR_WIDTH 20         /* characters inside the [....] memory bar */

/* ANSI escape sequences. "\033" is the ESC character. */
#define C_RESET   "\033[0m"
#define C_DIM     "\033[2m"
#define C_RED     "\033[1;31m"
#define C_GREEN   "\033[1;32m"
#define C_YELLOW  "\033[1;33m"
#define C_BLUE    "\033[1;34m"
#define C_MAGENTA "\033[1;35m"
#define C_CYAN    "\033[1;36m"

/* 1 = emit colors, 0 = plain text. Changed by --no-color. */
static int use_color = 1;

/* ---------- small string helpers (no <string.h>) ---------- */

/* Return the length of a NUL-terminated string. */
static int my_strlen(const char *s)
{
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* Return 1 if s begins with prefix, otherwise 0. */
static int starts_with(const char *s, const char *prefix)
{
    while (*prefix != '\0') {
        if (*s != *prefix) {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

/* Return 1 if the two strings are identical, otherwise 0.
 * Used to recognise command-line options. */
static int str_equal(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Return a pointer to the text after the first ':' (and any spaces/tabs).
 * Used for lines like "model name\t: Intel(R) Core(TM) ...". */
static const char *after_colon(const char *s)
{
    while (*s != '\0' && *s != ':') {
        s++;
    }
    if (*s == ':') {
        s++;
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

/* ---------- color helpers ---------- */

/* Return the escape sequence if colors are on, or an empty string if off.
 * Lets every printf stay the same whether or not color is enabled. */
static const char *clr(const char *code)
{
    return use_color ? code : "";
}

/* Pick a color for a usage percentage: green is fine, yellow is getting
 * high, red is critical. */
static const char *level_color(double pct)
{
    if (pct >= 85.0) {
        return C_RED;
    }
    if (pct >= 60.0) {
        return C_YELLOW;
    }
    return C_GREEN;
}

/* Print a colored, left-aligned row label such as "Kernel  : ". */
static void print_label(const char *color, const char *name)
{
    printf("%s%-8s%s: ", clr(color), name, clr(C_RESET));
}

/* ---------- file helpers ---------- */

/* Read one line into buf (at most size-1 chars), strip the trailing newline.
 * If the line was longer than the buffer, the rest is discarded so it is
 * not mistaken for a new line (cpuinfo "flags" lines can be very long).
 * Returns 1 if a line was read, 0 on EOF/error. */
static int read_line(FILE *fp, char *buf, int size)
{
    int len;
    int c;

    if (fgets(buf, size, fp) == NULL) {
        return 0;
    }
    len = my_strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else {
        /* No newline: line was truncated (or file ended). Drain the rest. */
        while ((c = fgetc(fp)) != EOF && c != '\n') {
            /* discard */
        }
    }
    return 1;
}

/* If line starts with key (e.g. "MemTotal:"), parse the number that follows
 * into *out and return 1. Otherwise (or if parsing fails) return 0. */
static int parse_kb(const char *line, const char *key, unsigned long long *out)
{
    if (!starts_with(line, key)) {
        return 0;
    }
    return sscanf(line + my_strlen(key), "%llu", out) == 1;
}

/* ---------- report sections ---------- */

/* Print the kernel version line from /proc/version. */
static void print_kernel(void)
{
    char line[LINE_SIZE];
    FILE *fp = fopen("/proc/version", "r");

    if (fp == NULL) {
        fprintf(stderr, "sysinfo: cannot open /proc/version\n");
        return;
    }
    if (read_line(fp, line, (int)sizeof line)) {
        print_label(C_MAGENTA, "Kernel");
        printf("%s\n", line);
    } else {
        fprintf(stderr, "sysinfo: /proc/version is empty\n");
    }
    fclose(fp);
}

/* Print uptime from /proc/uptime as "X days, HH:MM:SS".
 * The first field is seconds since boot as a decimal number. */
static void print_uptime(void)
{
    char line[LINE_SIZE];
    double up = 0.0;
    long total, days, hours, mins, secs;
    FILE *fp = fopen("/proc/uptime", "r");

    if (fp == NULL) {
        fprintf(stderr, "sysinfo: cannot open /proc/uptime\n");
        return;
    }
    if (read_line(fp, line, (int)sizeof line) &&
        sscanf(line, "%lf", &up) == 1) {
        if (up < 0.0) {
            up = 0.0;
        }
        total = (long)up;               /* drop the fractional part */
        days  = total / 86400L;
        total %= 86400L;
        hours = total / 3600L;
        total %= 3600L;
        mins  = total / 60L;
        secs  = total % 60L;
        print_label(C_GREEN, "Uptime");
        printf("%ld days, %02ld:%02ld:%02ld\n", days, hours, mins, secs);
    } else {
        fprintf(stderr, "sysinfo: could not parse /proc/uptime\n");
    }
    fclose(fp);
}

/* Print the 1, 5 and 15 minute load averages from /proc/loadavg. */
static void print_load(void)
{
    char line[LINE_SIZE];
    double l1, l5, l15;
    FILE *fp = fopen("/proc/loadavg", "r");

    if (fp == NULL) {
        fprintf(stderr, "sysinfo: cannot open /proc/loadavg\n");
        return;
    }
    if (read_line(fp, line, (int)sizeof line) &&
        sscanf(line, "%lf %lf %lf", &l1, &l5, &l15) == 3) {
        print_label(C_YELLOW, "Load");
        printf("%.2f %.2f %.2f\n", l1, l5, l15);
    } else {
        fprintf(stderr, "sysinfo: could not parse /proc/loadavg\n");
    }
    fclose(fp);
}

/* Print the CPU model (first "model name" line) and the number of logical
 * processors (lines starting with "processor") from /proc/cpuinfo. */
static void print_cpu(void)
{
    char line[LINE_SIZE];
    char model[LINE_SIZE] = "unknown";
    int have_model = 0;
    int count = 0;
    const char *value;
    int i;
    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (fp == NULL) {
        fprintf(stderr, "sysinfo: cannot open /proc/cpuinfo\n");
        return;
    }
    while (read_line(fp, line, (int)sizeof line)) {
        if (starts_with(line, "processor")) {
            count++;
        } else if (!have_model && starts_with(line, "model name")) {
            value = after_colon(line);
            /* copy safely into model[], bounded by its size */
            for (i = 0; value[i] != '\0' && i < (int)sizeof model - 1; i++) {
                model[i] = value[i];
            }
            model[i] = '\0';
            have_model = 1;
        }
    }
    fclose(fp);

    print_label(C_CYAN, "CPU");
    printf("%s (%d logical processors)\n", model, count);
}

/* Print a [####----] bar for a percentage (clamped to 0..100).
 * The filled part takes the usage-level color; the empty part is dimmed. */
static void draw_bar(double pct)
{
    const char *level;
    int filled, i;

    if (pct < 0.0) {
        pct = 0.0;
    }
    if (pct > 100.0) {
        pct = 100.0;
    }
    level = level_color(pct);
    filled = (int)(pct / 100.0 * BAR_WIDTH + 0.5);   /* round to nearest */

    printf("          [%s", clr(level));
    for (i = 0; i < filled; i++) {
        putchar('#');
    }
    printf("%s", clr(C_DIM));
    for (; i < BAR_WIDTH; i++) {
        putchar('-');
    }
    printf("%s] %s%.1f%%%s\n", clr(C_RESET), clr(level), pct, clr(C_RESET));
}

/* Print memory and swap statistics from /proc/meminfo.
 * Values in the file are in kiB; they are converted to MiB for display.
 * Used memory = MemTotal - MemAvailable. */
static void print_memory(void)
{
    char line[LINE_SIZE];
    unsigned long long mem_total = 0, mem_avail = 0;
    unsigned long long swap_total = 0, swap_free = 0;
    unsigned long long used_kb, swap_used_kb;
    int have_mt = 0, have_ma = 0, have_st = 0, have_sf = 0;
    double pct, swap_pct;
    FILE *fp = fopen("/proc/meminfo", "r");

    if (fp == NULL) {
        fprintf(stderr, "sysinfo: cannot open /proc/meminfo\n");
        return;
    }
    while (read_line(fp, line, (int)sizeof line)) {
        if (parse_kb(line, "MemTotal:", &mem_total)) {
            have_mt = 1;
        } else if (parse_kb(line, "MemAvailable:", &mem_avail)) {
            have_ma = 1;
        } else if (parse_kb(line, "SwapTotal:", &swap_total)) {
            have_st = 1;
        } else if (parse_kb(line, "SwapFree:", &swap_free)) {
            have_sf = 1;
        }
    }
    fclose(fp);

    if (have_mt && have_ma) {
        /* Guard against underflow if the numbers are inconsistent. */
        used_kb = (mem_total >= mem_avail) ? mem_total - mem_avail : 0;
        pct = (mem_total > 0)
                  ? (double)used_kb * 100.0 / (double)mem_total
                  : 0.0;
        print_label(C_BLUE, "Memory");
        printf("%llu / %llu MiB used (%s%.1f%%%s)\n",
               used_kb / 1024ULL, mem_total / 1024ULL,
               clr(level_color(pct)), pct, clr(C_RESET));
        draw_bar(pct);
        printf("          total %llu MiB, available %llu MiB\n",
               mem_total / 1024ULL, mem_avail / 1024ULL);
    } else {
        fprintf(stderr,
                "sysinfo: MemTotal/MemAvailable missing in /proc/meminfo\n");
    }

    if (have_st && have_sf) {
        swap_used_kb = (swap_total >= swap_free) ? swap_total - swap_free : 0;
        swap_pct = (swap_total > 0)
                       ? (double)swap_used_kb * 100.0 / (double)swap_total
                       : 0.0;
        print_label(C_BLUE, "Swap");
        printf("%s%llu%s / %llu MiB used\n",
               clr(level_color(swap_pct)), swap_used_kb / 1024ULL,
               clr(C_RESET), swap_total / 1024ULL);
    } else {
        fprintf(stderr,
                "sysinfo: SwapTotal/SwapFree missing in /proc/meminfo\n");
    }
}

/* ---------- command line ---------- */

/* Print usage text to the given stream. */
static void print_usage(FILE *out)
{
    fprintf(out,
            "Usage: sysinfo [--version] [--no-color] [--help]\n"
            "  --version    print the program version and exit\n"
            "  --no-color   disable ANSI colors\n"
            "  --help       show this help\n");
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    int i;

    /* Parse options; --version and --help print and exit immediately. */
    for (i = 1; i < argc; i++) {
        if (str_equal(argv[i], "--version")) {
            printf("sysinfo version %s\n", VERSION_STRING);
            return 0;
        } else if (str_equal(argv[i], "--help")) {
            print_usage(stdout);
            return 0;
        } else if (str_equal(argv[i], "--no-color")) {
            use_color = 0;
        } else {
            fprintf(stderr, "sysinfo: unknown option '%s'\n", argv[i]);
            print_usage(stderr);
            return 1;
        }
    }

    printf("%s===== System Information =====%s\n",
           clr(C_CYAN), clr(C_RESET));
    print_kernel();
    print_uptime();
    print_load();
    print_cpu();
    print_memory();
    return 0;
}
