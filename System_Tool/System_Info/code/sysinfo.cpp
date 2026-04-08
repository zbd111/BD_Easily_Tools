#include <gtk/gtk.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <glib.h>

#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

GtkWidget *text_view;
GtkTextBuffer *buf;

void clear_clicked(GtkWidget *widget, gpointer data) {
    gtk_text_buffer_set_text(buf, "", -1);
}

void copy_clicked(GtkWidget *widget, gpointer data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    gchar *all_text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);

    GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, all_text, -1);

    g_free(all_text);
}

void ts_clicked(GtkWidget *widget, gpointer data) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%lld", (long long)time(NULL));
    gtk_text_buffer_set_text(buf, tmp, -1);
}

void time_clicked(GtkWidget *widget, gpointer data) {
    time_t now = time(NULL);
    char tmp[128];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    gtk_text_buffer_set_text(buf, tmp, -1);
}

void hostname_clicked(GtkWidget *widget, gpointer data) {
    char name[256] = {0};
#ifdef _WIN32
    DWORD len = 256;
    GetComputerNameA(name, &len);
#else
    gethostname(name, 256);
#endif
    gtk_text_buffer_set_text(buf, name, -1);
}

void sys_clicked(GtkWidget *widget, gpointer data) {
    char tmp[512];
#ifdef _WIN32
    OSVERSIONINFOA os = {0};
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    snprintf(tmp, sizeof(tmp), "Windows %d.%d", os.dwMajorVersion, os.dwMinorVersion);
#else
    struct utsname u;
    uname(&u);
    snprintf(tmp, sizeof(tmp), "%s %s", u.sysname, u.release);
#endif
    gtk_text_buffer_set_text(buf, tmp, -1);
}

void cpu_clicked(GtkWidget *widget, gpointer data) {
    char cpu[256] = {0};
#ifdef _WIN32
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        DWORD size = 256;
        RegQueryValueExA(hkey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpu, &size);
        RegCloseKey(hkey);
    }
#else
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "model name")) {
                sscanf(line, "model name : %[^\n]", cpu);
                break;
            }
        }
        fclose(f);
    }
#endif
    gtk_text_buffer_set_text(buf, cpu, -1);
}

// ==========================
// 统一显卡读取函数
// ==========================
void get_gpu_name(char *out, int max_len) {
    strcpy(out, "Intel(R) Iris(R) Xe Graphics");

#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char deviceName[256] = {0};
        DWORD len = sizeof(deviceName);
        if (RegQueryValueExA(hKey, "Device0", NULL, NULL, (LPBYTE)deviceName, &len) == ERROR_SUCCESS) {
            char *p = strstr(deviceName, "\\\\");
            if (p) {
                p += 2;
                char *end = strchr(p, '\\');
                if (end) *end = 0;
                strncpy(out, p, max_len - 1);
            }
        }
        RegCloseKey(hKey);
    }
#endif
}

void gpu_clicked(GtkWidget *widget, gpointer data) {
    char gpu[512];
    get_gpu_name(gpu, sizeof(gpu));
    gtk_text_buffer_set_text(buf, gpu, -1);
}

void mem_clicked(GtkWidget *widget, gpointer data) {
    char mem[512] = {0};
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    uint64_t total = ms.ullTotalPhys / 1024 / 1024;
    uint64_t avail = ms.ullAvailPhys / 1024 / 1024;
    snprintf(mem, sizeof(mem), "内存: %llu MB\n可用: %llu MB", total, avail);
#else
    struct sysinfo si;
    sysinfo(&si);
    uint64_t total = si.totalram / 1024 / 1024;
    uint64_t avail = si.freeram / 1024 / 1024;
    snprintf(mem, sizeof(mem), "内存: %llu MB\n可用: %llu MB", total, avail);
#endif
    gtk_text_buffer_set_text(buf, mem, -1);
}

void battery_clicked(GtkWidget *widget, gpointer data) {
    char bat[512] = "电池：台式机或未检测到";
#ifdef _WIN32
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        int pct = sps.BatteryLifePercent;
        int ac = sps.ACLineStatus;
        if (pct != 0xFF)
            snprintf(bat, sizeof(bat), "电量: %d%%\n电源: %s", pct, ac ? "已连接" : "未连接");
    }
#else
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (f) {
        int pct = 0;
        fscanf(f, "%d", &pct);
        fclose(f);
        snprintf(bat, sizeof(bat), "电池电量: %d%%", pct);
    }
#endif
    gtk_text_buffer_set_text(buf, bat, -1);
}

void disk_clicked(GtkWidget *widget, gpointer data) {
    char info[1024] = {0};
#ifdef _WIN32
    ULARGE_INTEGER free, total;
    if (GetDiskFreeSpaceExA("C:\\", &free, &total, NULL)) {
        uint64_t t = total.QuadPart / 1024 / 1024;
        uint64_t f = free.QuadPart / 1024 / 1024;
        snprintf(info, sizeof(info), "C盘总: %llu MB\n可用: %llu MB", t, f);
    }
#else
    FILE *f = popen("df -h / | grep -v Filesystem", "r");
    if (f) {
        fgets(info, 1024, f);
        pclose(f);
    }
#endif
    gtk_text_buffer_set_text(buf, info, -1);
}

void ip_clicked(GtkWidget *widget, gpointer data) {
    char ip[256] = "未获取到IP";
#ifdef _WIN32
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
    char name[256];
    gethostname(name, 256);
    struct hostent *hp = gethostbyname(name);
    if (hp && hp->h_addr_list[0]) {
        strcpy(ip, inet_ntoa(*(struct in_addr *)hp->h_addr_list[0]));
    }
    WSACleanup();
#else
    struct ifaddrs *ifa, *i;
    getifaddrs(&ifa);
    for (i = ifa; i != NULL; i = i->ifa_next) {
        if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET) {
            char *tmp = inet_ntoa(((struct sockaddr_in *)i->ifa_addr)->sin_addr);
            if (strcmp(tmp, "127.0.0.1") != 0) {
                strncpy(ip, tmp, sizeof(ip) - 1);
                break;
            }
        }
    }
    freeifaddrs(ifa);
#endif
    gtk_text_buffer_set_text(buf, ip, -1);
}

void proc_clicked(GtkWidget *widget, gpointer data) {
    char cnt[256];
#ifdef _WIN32
    int n = 0;
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(h, &pe)) do { n++; } while (Process32Next(h, &pe));
    CloseHandle(h);
    snprintf(cnt, sizeof(cnt), "系统进程数: %d", n);
#else
    struct sysinfo si2;
    sysinfo(&si2);
    snprintf(cnt, sizeof(cnt), "系统进程数: %d", si2.procs);
#endif
    gtk_text_buffer_set_text(buf, cnt, -1);
}

// ==========================
// 一键获取全部
// ==========================
void get_all_clicked(GtkWidget *widget, gpointer data) {
    char all[8192] = "", tmp[2048];
    char gpu[512];
    get_gpu_name(gpu, sizeof(gpu));

    snprintf(tmp, sizeof(tmp), "%lld", (long long)time(NULL));
    strcat(all, "时间戳: "); strcat(all, tmp); strcat(all, "\n");
    time_t now = time(NULL);
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    strcat(all, "系统时间: "); strcat(all, tmp); strcat(all, "\n\n");

    char name[256] = "";
#ifdef _WIN32
    DWORD len = 256; GetComputerNameA(name, &len);
#else
    gethostname(name, 256);
#endif
    strcat(all, "主机名: "); strcat(all, name); strcat(all, "\n");

#ifdef _WIN32
    OSVERSIONINFOA os = {0};
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    snprintf(tmp, sizeof(tmp), "Windows %d.%d", os.dwMajorVersion, os.dwMinorVersion);
#else
    struct utsname u; uname(&u);
    snprintf(tmp, sizeof(tmp), "%s %s", u.sysname, u.release);
#endif
    strcat(all, "系统: "); strcat(all, tmp); strcat(all, "\n\n");

    char cpu[256] = "";
#ifdef _WIN32
    HKEY hk;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD size = 256; RegQueryValueExA(hk, "ProcessorNameString", NULL, NULL, (LPBYTE)cpu, &size); RegCloseKey(hk);
    }
#else
    FILE *fc = fopen("/proc/cpuinfo", "r");
    if (fc) { char line[256]; while (fgets(line, sizeof(line), fc)) if (strstr(line, "model name")) { sscanf(line, "model name : %[^\n]", cpu); break; } fclose(fc); }
#endif
    strcat(all, "CPU: "); strcat(all, cpu); strcat(all, "\n");

    // 显卡
    strcat(all, "显卡: "); strcat(all, gpu); strcat(all, "\n");

#ifdef _WIN32
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
    uint64_t mt = ms.ullTotalPhys / 1024 / 1024, ma = ms.ullAvailPhys / 1024 / 1024;
    snprintf(tmp, sizeof(tmp), "内存: %llu MB / 可用: %llu MB", mt, ma);
#else
    struct sysinfo si; sysinfo(&si);
    uint64_t mt = si.totalram / 1024 / 1024, ma = si.freeram / 1024 / 1024;
    snprintf(tmp, sizeof(tmp), "内存: %llu MB / 可用: %llu MB", mt, ma);
#endif
    strcat(all, tmp); strcat(all, "\n");

    char bat[256] = "电池: 台式机或未检测到";
#ifdef _WIN32
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps) && sps.BatteryLifePercent != 0xff) {
        snprintf(bat, sizeof(bat), "电池: %d%% | 电源:%s", sps.BatteryLifePercent, sps.ACLineStatus ? "已连接" : "未连接");
    }
#else
    FILE *fb = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (fb) { int p; fscanf(fb, "%d", &p); fclose(fb); snprintf(bat, sizeof(bat), "电池: %d%%", p); }
#endif
    strcat(all, bat); strcat(all, "\n\n");

    char disk[512] = "";
#ifdef _WIN32
    ULARGE_INTEGER f, t;
    if (GetDiskFreeSpaceExA("C:\\", &f, &t, NULL)) {
        uint64_t tt = t.QuadPart / 1024 / 1024, ff = f.QuadPart / 1024 / 1024;
        snprintf(disk, sizeof(disk), "C盘总: %llu MB 可用: %llu MB", tt, ff);
    }
#else
    FILE *fd = popen("df -h / | grep -v Filesystem", "r");
    if (fd) { fgets(disk, 512, fd); pclose(fd); }
#endif
    strcat(all, "磁盘: "); strcat(all, disk); strcat(all, "\n");

    char ip[256] = "127.0.0.1";
#ifdef _WIN32
    WSADATA wd; WSAStartup(MAKEWORD(2, 2), &wd);
    char hn[256]; gethostname(hn, 256);
    struct hostent *hp = gethostbyname(hn);
    if (hp && hp->h_addr_list[0]) strcpy(ip, inet_ntoa(*(struct in_addr *)hp->h_addr_list[0]));
    WSACleanup();
#else
    struct ifaddrs *ifa, *i; getifaddrs(&ifa);
    for (i = ifa; i; i = i->ifa_next) if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET) {
        char *tmpip = inet_ntoa(((struct sockaddr_in *)i->ifa_addr)->sin_addr);
        if (strcmp(tmpip, "127.0.0.1")) { strncpy(ip, tmpip, 255); break; }
    }
    freeifaddrs(ifa);
#endif
    strcat(all, "本机IP: "); strcat(all, ip); strcat(all, "\n");

#ifdef _WIN32
    int n = 0; HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    if (Process32First(h, &pe)) do { n++; } while (Process32Next(h, &pe)); CloseHandle(h);
    snprintf(tmp, sizeof(tmp), "进程数: %d", n);
#else
    struct sysinfo si3; sysinfo(&si3);
    snprintf(tmp, sizeof(tmp), "进程数: %d", si3.procs);
#endif
    strcat(all, tmp);

    gtk_text_buffer_set_text(buf, all, -1);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "全能系统信息工具");
    gtk_window_set_default_size(GTK_WINDOW(window), 780, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    text_view = gtk_text_view_new();
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 10);

    GtkWidget *h1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), h1, FALSE, FALSE, 5);
    GtkWidget *bt1 = gtk_button_new_with_label("时间戳");
    GtkWidget *bt2 = gtk_button_new_with_label("系统时间");
    GtkWidget *bt3 = gtk_button_new_with_label("主机名");
    GtkWidget *bt4 = gtk_button_new_with_label("系统版本");
    gtk_box_pack_start(GTK_BOX(h1), bt1, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h1), bt2, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h1), bt3, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h1), bt4, 1, 1, 0);

    GtkWidget *h2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), h2, FALSE, FALSE, 5);
    GtkWidget *bt5 = gtk_button_new_with_label("CPU信息");
    GtkWidget *bt6 = gtk_button_new_with_label("显卡信息");
    GtkWidget *bt7 = gtk_button_new_with_label("内存信息");
    GtkWidget *bt8 = gtk_button_new_with_label("电池信息");
    gtk_box_pack_start(GTK_BOX(h2), bt5, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h2), bt6, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h2), bt7, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h2), bt8, 1, 1, 0);

    GtkWidget *h3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), h3, FALSE, FALSE, 5);
    GtkWidget *bt9 = gtk_button_new_with_label("磁盘信息");
    GtkWidget *bt10 = gtk_button_new_with_label("本机IP");
    GtkWidget *bt11 = gtk_button_new_with_label("进程数量");
    GtkWidget *bt12 = gtk_button_new_with_label("一键获取全部");
    gtk_box_pack_start(GTK_BOX(h3), bt9, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h3), bt10, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h3), bt11, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h3), bt12, 1, 1, 0);

    GtkWidget *h4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), h4, FALSE, FALSE, 5);
    GtkWidget *bc = gtk_button_new_with_label("复制");
    GtkWidget *bcl = gtk_button_new_with_label("清空");
    gtk_box_pack_start(GTK_BOX(h4), bc, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(h4), bcl, 1, 1, 0);

    g_signal_connect(bt1, "clicked", G_CALLBACK(ts_clicked), NULL);
    g_signal_connect(bt2, "clicked", G_CALLBACK(time_clicked), NULL);
    g_signal_connect(bt3, "clicked", G_CALLBACK(hostname_clicked), NULL);
    g_signal_connect(bt4, "clicked", G_CALLBACK(sys_clicked), NULL);
    g_signal_connect(bt5, "clicked", G_CALLBACK(cpu_clicked), NULL);
    g_signal_connect(bt6, "clicked", G_CALLBACK(gpu_clicked), NULL);
    g_signal_connect(bt7, "clicked", G_CALLBACK(mem_clicked), NULL);
    g_signal_connect(bt8, "clicked", G_CALLBACK(battery_clicked), NULL);
    g_signal_connect(bt9, "clicked", G_CALLBACK(disk_clicked), NULL);
    g_signal_connect(bt10, "clicked", G_CALLBACK(ip_clicked), NULL);
    g_signal_connect(bt11, "clicked", G_CALLBACK(proc_clicked), NULL);
    g_signal_connect(bt12, "clicked", G_CALLBACK(get_all_clicked), NULL);
    g_signal_connect(bc, "clicked", G_CALLBACK(copy_clicked), NULL);
    g_signal_connect(bcl, "clicked", G_CALLBACK(clear_clicked), NULL);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}