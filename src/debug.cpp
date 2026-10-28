// Debug.cpp

#include "win32_support.h"
#include "pce_cpu.h"
#include "Debug.h"
#include "debug_command.h"
#include "host_runtime.h"

#ifdef _DEBUG

static BOOL CommandGo(PceCpu* R);
static BOOL CommandExamine(PceCpu* R);
static BOOL CommandBreak(PceCpu* R);
static BOOL CommandDump(PceCpu* R, rxpce_cpu::Bus bus);
static BOOL CommandTrace(PceCpu* R);
static int DebugPrompt(void);
static BOOL Buf_ReadWord(WORD* p);
static void PrintRegister(PceCpu* R);
static rxpce_debug::CommandLine command_line;

bool DebugTrace(PceCpu* R, rxpce_cpu::Bus bus) {
    BOOL ret;
    int i;

    for (i = 0; i < 255; i++)
        GetAsyncKeyState(i);

    PrintRegister(R);
    do {
        ret = TRUE;
        switch (DebugPrompt()) {
        case 'G':
            ret = CommandGo(R);
            break;

        case 'X':
            ret = CommandExamine(R);
            break;

        case 'B':
            ret = CommandBreak(R);
            break;

        case 'D':
            ret = CommandDump(R, bus);
            break;

        case 'T':
            ret = CommandTrace(R);
            break;

        case 'Q':
            return FALSE;
        }
    } while (ret);

    return TRUE;
}

BOOL CommandGo(PceCpu* R) {
    R->Trace = FALSE;
    return FALSE;
}

BOOL CommandExamine(PceCpu* R) {
    PrintRegister(R);
    return TRUE;
}

BOOL CommandBreak(PceCpu* R) {
    WORD w;

    if (!Buf_ReadWord(&w))
        TRACE("Current break point is %04X\n", R->Trap);
    else {
        R->Trap = w;
        TRACE("Break point set at %04X\n", R->Trap);
    }
    return TRUE;
}

BOOL CommandDump(PceCpu* R, rxpce_cpu::Bus bus) {
    static WORD adr = 0;
    int i, j;
    WORD w;

    if (Buf_ReadWord(&w))
        adr = w;

    for (i = 0; i < 8; i++) {
        TRACE("%04X:", adr);
        for (j = 0; j < 8; j++) {
            TRACE(" %02X", bus.read((std::uint32_t(R->cpu.mpr[adr >> 13]) << 13) | (adr & 8191),
                                    bus.context));
            adr++;
        }
        TRACE("\n");
    }
    return TRUE;
}

BOOL CommandTrace(PceCpu*) {
    return FALSE;
}

int DebugPrompt(void) {
    command_line.clear();
    TRACE(">");
    int key;
    do {
        do {
            if (IsMachineStopRequested())
                return 'Q';
            for (key = 0; key < 0x70; ++key)
                if (GetAsyncKeyState(key) & 1)
                    break;
            if (key == 0x70)
                Sleep(1);
        } while (key == 0x70);
        if (key >= 0x20 && command_line.append(static_cast<char>(key)))
            TRACE("%c", key);
        if (key == VK_BACK) {
            command_line.pop();
            const auto text = command_line.text();
            TRACE("\n>%.*s", static_cast<int>(text.size()), text.data());
        }
    } while (key != VK_RETURN);
    TRACE("\n");
    return command_line.command();
}

BOOL Buf_ReadWord(WORD* output) {
    const auto address = command_line.address();
    if (!address)
        return FALSE;
    *output = *address;
    return TRUE;
}

void PrintRegister(PceCpu* R) {
    int i;

    TRACE("PC:%04X\n"
          "A:%02X, P:%02X, X:%02X, Y:%02X, S:%02X\n"
          "MPR:",
          R->cpu.pc, R->cpu.a, R->cpu.p, R->cpu.x, R->cpu.y, R->cpu.sp);
    for (i = 0; i < 8; i++)
        TRACE(" %02X", R->cpu.mpr[i]);
    TRACE("\n");
}

#endif // _DEBUG
