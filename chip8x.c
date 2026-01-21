#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
    uint8_t m[4096];
    uint8_t v[16];
    uint16_t i;
    uint16_t pc;
    uint16_t s[16];
    uint16_t sp;
    uint8_t dt;
    uint8_t st;
    uint8_t d[64 * 32];
    uint8_t k[16];
} C8;

uint8_t f[] = {
    0xF0,0x90,0x90,0x90,0xF0,0x20,0x60,0x20,0x20,0x70,0xF0,0x10,0xF0,0x80,0xF0,0xF0,0x10,0xF0,0x10,0xF0,
    0x90,0x90,0xF0,0x10,0x10,0xF0,0x80,0xF0,0x10,0xF0,0xF0,0x80,0xF0,0x90,0xF0,0xF0,0x10,0x20,0x40,0x40,
    0xF0,0x90,0xF0,0x90,0xF0,0xF0,0x90,0xF0,0x10,0xF0,0xF0,0x90,0xF0,0x90,0x90,0xE0,0x90,0xE0,0x90,0xE0,
    0xF0,0x80,0x80,0x80,0xF0,0xE0,0x90,0x90,0x90,0xE0,0xF0,0x80,0xF0,0x80,0xF0,0xF0,0x80,0xF0,0x80,0x80
};

void init(C8 *c, char *p) {
    memset(c, 0, sizeof(C8));
    c->pc = 0x200;
    memcpy(c->m, f, sizeof(f));
    FILE *fp = fopen(p, "rb");
    if (!fp) exit(1);
    fread(c->m + 0x200, 1, 4096 - 0x200, fp);
    fclose(fp);
}

void step(C8 *c) {
    uint16_t op = (c->m[c->pc] << 8) | c->m[c->pc + 1];
    uint16_t x = (op & 0x0F00) >> 8;
    uint16_t y = (op & 0x00F0) >> 4;
    uint16_t nnn = op & 0x0FFF;
    uint8_t nn = op & 0x00FF;
    uint8_t n = op & 0x000F;
    c->pc += 2;

    switch (op & 0xF000) {
        case 0x0000:
            if (op == 0x00E0) memset(c->d, 0, 2048);
            else if (op == 0x00EE) c->pc = c->s[--c->sp];
            break;
        case 0x1000: c->pc = nnn; break;
        case 0x2000: c->s[c->sp++] = c->pc; c->pc = nnn; break;
        case 0x3000: if (c->v[x] == nn) c->pc += 2; break;
        case 0x4000: if (c->v[x] != nn) c->pc += 2; break;
        case 0x5000: if (c->v[x] == c->v[y]) c->pc += 2; break;
        case 0x6000: c->v[x] = nn; break;
        case 0x7000: c->v[x] += nn; break;
        case 0x8000:
            switch (n) {
                case 0: c->v[x] = c->v[y]; break;
                case 1: c->v[x] |= c->v[y]; break;
                case 2: c->v[x] &= c->v[y]; break;
                case 3: c->v[x] ^= c->v[y]; break;
                case 4: { uint16_t r = c->v[x] + c->v[y]; c->v[0xF] = r > 255; c->v[x] = r & 0xFF; } break;
                case 5: c->v[0xF] = c->v[x] > c->v[y]; c->v[x] -= c->v[y]; break;
                case 6: c->v[0xF] = c->v[x] & 1; c->v[x] >>= 1; break;
                case 7: c->v[0xF] = c->v[y] > c->v[x]; c->v[x] = c->v[y] - c->v[x]; break;
                case 0xE: c->v[0xF] = (c->v[x] >> 7) & 1; c->v[x] <<= 1; break;
            }
            break;
        case 0x9000: if (c->v[x] != c->v[y]) c->pc += 2; break;
        case 0xA000: c->i = nnn; break;
        case 0xB000: c->pc = nnn + c->v[0]; break;
        case 0xC000: c->v[x] = (rand() % 256) & nn; break;
        case 0xD000:
            c->v[0xF] = 0;
            for (int h = 0; h < n; h++) {
                uint8_t p = c->m[c->i + h];
                for (int w = 0; w < 8; w++) {
                    if (p & (0x80 >> w)) {
                        int idx = ((c->v[x] + w) % 64) + ((c->v[y] + h) % 32) * 64;
                        if (c->d[idx]) c->v[0xF] = 1;
                        c->d[idx] ^= 1;
                    }
                }
            }
            break;
        case 0xE000:
            if (nn == 0x9E) { if (c->k[c->v[x]]) c->pc += 2; }
            else if (nn == 0xA1) { if (!c->k[c->v[x]]) c->pc += 2; }
            break;
        case 0xF000:
            switch (nn) {
                case 0x07: c->v[x] = c->dt; break;
                case 0x0A: { int p = 0; for(int j=0; j<16; j++) if(c->k[j]) { c->v[x]=j; p=1; break; } if(!p) c->pc -= 2; } break;
                case 0x15: c->dt = c->v[x]; break;
                case 0x18: c->st = c->v[x]; break;
                case 0x1E: c->i += c->v[x]; break;
                case 0x29: c->i = c->v[x] * 5; break;
                case 0x33: c->m[c->i] = c->v[x]/100; c->m[c->i+1] = (c->v[x]/10)%10; c->m[c->i+2] = c->v[x]%10; break;
                case 0x55: for(int j=0; j<=x; j++) c->m[c->i+j] = c->v[j]; break;
                case 0x65: for(int j=0; j<=x; j++) c->v[j] = c->m[c->i+j]; break;
            }
            break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    C8 c; init(&c, argv[1]);
    Display *dpy = XOpenDisplay(NULL);
    int sc = DefaultScreen(dpy), s = 10;
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, sc), 0, 0, 64*s, 32*s, 0, 0, 0);
    XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask);
    XMapWindow(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    struct timeval t1, t2; gettimeofday(&t1, NULL);
    KeySym keys[] = { XK_x, XK_1, XK_2, XK_3, XK_q, XK_w, XK_e, XK_a, XK_s, XK_d, XK_z, XK_c, XK_4, XK_r, XK_f, XK_v };
    
    while (1) {
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            for (int i=0; i<16; i++) if (XLookupKeysym(&ev.xkey, 0) == keys[i]) c.k[i] = (ev.type == KeyPress);
        }
        step(&c);
        gettimeofday(&t2, NULL);
        if ((t2.tv_usec - t1.tv_usec + 1000000 * (t2.tv_sec - t1.tv_sec)) > 16666) {
            if (c.dt > 0) c.dt--; if (c.st > 0) c.st--;
            XClearWindow(dpy, win);
            for (int j=0; j<2048; j++) if (c.d[j]) {
                XSetForeground(dpy, gc, 0xFFFFFF);
                XFillRectangle(dpy, win, gc, (j%64)*s, (j/64)*s, s, s);
            }
            XFlush(dpy); gettimeofday(&t1, NULL);
        }
        usleep(1200);
    }
    return 0;
}
