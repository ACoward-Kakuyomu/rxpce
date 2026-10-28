#pragma once
// Device implementation and deterministic buffer tests only.
#include "ds.h"
#include <mmsystem.h>
#include <dsound.h>
#include <wrl/client.h>

extern Microsoft::WRL::ComPtr<IDirectSound> lpDSound;
extern Microsoft::WRL::ComPtr<IDirectSoundBuffer> lpSoundBuffer;
extern int ds_nBits;
extern int ds_nChannels;
extern int ds_nSampleRate;
extern int ds_nLockLatency;
extern DWORD ds_dwLockSize;
extern DWORD ds_dwBufferSize;
extern int ds_CoopMode;

BOOL StartSound(void);
BOOL ReleaseSoundBuffer(void);
BOOL StopSound(void);
BOOL WriteDSoundBuffer(DWORD bytes = 0);
BOOL ServiceDSoundBuffer(void);
