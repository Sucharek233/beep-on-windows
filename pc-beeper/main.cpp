#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

#include <Windows.h>
#include <windowsx.h>

#include <winring0.h>
#include <getopt.h>

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define BIT(nr)				(1 << (nr))

#define PIT_CHNL0_PORT			(0x40)
#define PIT_CHNL1_PORT			(0x41)
#define PIT_CHNL2_PORT			(0x42)

#define PIT_CMD_PORT			(0x43)

#define PIT_CMD_CHNL2			((0x02) << 6)
#define PIT_CMD_LMSB_RW			((0x03) << 4)
#define PIT_CMD_OUT_SQUARE_WAVE		((0x03) << 1)
#define PIT_CMD_COUNT_MODE_BIN		((0x00) << 0)

#define SYS_SPKR_PORT			(0x61)

#define SYS_SPKR_ENABLE			BIT(0)
#define SYS_SPKR_GATE2_ENABLE		BIT(1)

#define PCLK_8284_HZ			(1193182)	// 1193180 // 1193182

#define BEEP_FREQ_HZ			(440)
#define BEEP_DURATION			(100)

static uint32_t beep_freq = BEEP_FREQ_HZ;
static uint32_t duration_ms = BEEP_DURATION;

typedef struct {
	unsigned int freq;
	unsigned int duration;
	unsigned int delay;
} BeepStep;

#define MAX_STEPS 99999
BeepStep steps[MAX_STEPS];
int num_steps = 0;


void print_help(void)
{
	fprintf_s(stdout, "Usage:\n");
	fprintf_s(stdout, "    beep.exe [-f <Hz>] [-l <ms>] [-D <ms>] [-? / -h]\n");
	fprintf_s(stdout, "Options:\n");
    fprintf_s(stdout, "    -f    Beep frequency, in Hz.\n");
	fprintf_s(stdout, "    -l    Duration, in ms.\n");
    fprintf_s(stdout, "    -D    Wait time between new beeps, in ms\n");
}

int parse_opts(int argc, char* argv[])
{
    int c;
    opterr = 0;

    BeepStep current = { 1000, 200, 0 }; // default values

    while ((c = getopt(argc, argv, "hf:l:D:n")) != -1) {
        switch (c) {
        case 'h':
            return -1;

        case 'f':
            if (sscanf_s(optarg, "%u", &current.freq) != 1) {
                fprintf(stderr, "%s(): failed to parse -f\n", __func__);
                return -1;
            }
            break;

        case 'l':
            if (sscanf_s(optarg, "%u", &current.duration) != 1) {
                fprintf(stderr, "%s(): failed to parse -l\n", __func__);
                return -1;
            }
            break;

        case 'D':
            if (sscanf_s(optarg, "%u", &current.delay) != 1) {
                fprintf(stderr, "%s(): failed to parse -D\n", __func__);
                return -1;
            }
            break;

        case 'n':
            if (num_steps < MAX_STEPS) {
                steps[num_steps++] = current;
            }
            else {
                fprintf(stderr, "%s(): too many steps\n", __func__);
                return -1;
            }

            current.freq = 1000;
            current.duration = 200;
            current.delay = 0;

            break;

        case '?':
            fprintf(stderr, "%s(): unknown or invalid option -%c\n", __func__, optopt);
            return -1;

        default:
            break;
        }
    }

    // Add last step if `-n` wasn't the last argument
    if (num_steps < MAX_STEPS) {
        steps[num_steps++] = current;
    }

    return 0;
}

void beep(uint32_t freq)
{
	uint32_t div;
	uint8_t reg;

	div = PCLK_8284_HZ / freq;

	// Config CMD PORT
	reg = PIT_CMD_CHNL2 | PIT_CMD_LMSB_RW | PIT_CMD_OUT_SQUARE_WAVE | PIT_CMD_COUNT_MODE_BIN; //0xb6
	WriteIoPortByte(PIT_CMD_PORT, reg);

	// Send data to channel 2
	WriteIoPortByte(PIT_CHNL2_PORT, (uint8_t)(div & 0xff));
	WriteIoPortByte(PIT_CHNL2_PORT, (uint8_t)((div) >> 8));

	reg = ReadIoPortByte(SYS_SPKR_PORT);

	WriteIoPortByte(SYS_SPKR_PORT, reg | SYS_SPKR_ENABLE | SYS_SPKR_GATE2_ENABLE);
}

void beep_stop()
{
	uint8_t reg = ReadIoPortByte(SYS_SPKR_PORT) & ~(SYS_SPKR_ENABLE | SYS_SPKR_GATE2_ENABLE);

	WriteIoPortByte(SYS_SPKR_PORT, reg);
}

void sigint_handle(int sig)
{
	#define UNUSED_PARAM(sig)

	beep_stop();

	WinRing0_deinit();

	exit(0);
}

int main(int argc, char* argv[])
{
    timeBeginPeriod(1);

    if (parse_opts(argc, argv)) {
        print_help();
        return 0;
    }

    if (WinRing0_init()) {
        fprintf(stderr, "%s(): failed to init WinRing0 driver\n", __func__);
        return -1;
    }

    signal(SIGINT, sigint_handle);

    for (int i = 0; i < num_steps; ++i) {
        BeepStep b = steps[i];

        beep(b.freq);
        Sleep(b.duration);
        beep_stop();

        if (i > 0 && b.delay > 0)
            Sleep(b.delay);
    }

    WinRing0_deinit();
    timeEndPeriod(1);

    return 0;
}
