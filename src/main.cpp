#include <cstring>  // For strcmp

#include "daisy_pod.h"
#include "daisysp.h"

// needed for sd writing
#include "daisy_core.h"
#include "fatfs.h"
#include "ff.h"
#include "util/wav_format.h"

using namespace daisy;
using namespace daisysp;

// #define LOGG  // uncomment to start serial over USB Logger class

/**
    this code was adapted from https://github.com/willemOH/daisy_looper
 */

DaisyPod hardware;

SdmmcHandler sdcard;
FatFSInterface fsi;
WavWriter<16384> writer;  // 16-bit
bool saved = true;

FIL fp;
bool stereo = true;

#define BUFFER_LENGTH (48000 * 349)  // 349.52 secs; 48k * 2 (stereo) * 2  (16-bit or 2 bytes per sample) = 192k/s
int16_t DSY_SDRAM_BSS Buffer[BUFFER_LENGTH];

float bufferIndex = 0;
size_t length = 0;
uint32_t recordedLength;

float sysSampleRate;

bool record = false;
bool play = false;

#ifdef LOGG
auto logger = Logger<LOGGER_INTERNAL>();
#endif

struct StereoPair {
    float left;
    float right;
};

float GetBufferValueInterpolated(float index) {
    int32_t indexInt = static_cast<int32_t>(index);  // strips decimal
    float indexFraction = index - indexInt;          // gets decimal

    float currentChannel = s162f(Buffer[indexInt]);
    float nextChannel = s162f(Buffer[indexInt + 2]);  // if currentChannel is left channel, nextChannel will be left channel. Same with right

    // Linear interpolation (creating value halfway between samples)
    float sig = currentChannel + (nextChannel - currentChannel) * indexFraction;

    return {sig};
}

void SetBufferValue(uint32_t index, StereoPair signal) {
    Buffer[index] = f2s16(signal.left);
    Buffer[index + 1] = f2s16(signal.right);
}

/* adds given wav file to the buffer. Only supports 16bit PCM 48kHz.
 * If Stereo samples are interleaved left then right.
 * return 0: succesful, 1: file read failed, 2: invalid format, 3: file too large
 */
int SetSample(TCHAR *fname) {
    UINT bytesread;
    WAV_FormatTypeDef wav_data;

    memset(Buffer, 0, BUFFER_LENGTH);

    if (f_open(&fp, fname, (FA_OPEN_EXISTING | FA_READ)) == FR_OK) {
        // Populate the WAV Info
        if (f_read(&fp, (void *)&wav_data, sizeof(WAV_FormatTypeDef), &bytesread) != FR_OK) return 1;
    } else
        return 4;

    if (wav_data.SampleRate != 48000 || wav_data.BitPerSample != 16) return 2;
    if (wav_data.SubCHunk2Size > BUFFER_LENGTH || wav_data.NbrChannels > 2) return 3;
    stereo = wav_data.NbrChannels == 2;

    if (f_lseek(&fp, sizeof(WAV_FormatTypeDef)) != FR_OK) return 5;

    if (f_read(&fp, Buffer, wav_data.SubCHunk2Size, &bytesread) != FR_OK) return 6;
    length = bytesread / (wav_data.BitPerSample / 8) / (stereo ? 2 : 1);

#ifdef LOGG
    // shows if there is a discrepancy between the following values after sample load (if not the same, wavwriter.h needs the bugfix)
    logger.PrintLine("SubCHunk2Size: %d", wav_data.SubCHunk2Size);
    logger.PrintLine("Bytes read: %d", bytesread);
#endif
    f_close(&fp);
    return 0;
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t size) {
    float left = 0;
    float right = 0;
    for (size_t i = 0; i < size; i += 2) {
        if (record) {
            SetBufferValue(bufferIndex * 2.0f, StereoPair{in[i], in[i + 1]});  //* 2 to access the buffer as if it contained single left and right channel structs
            bufferIndex += 1.0f;
            length = bufferIndex;
            // wav writing
            float sampleArray[2] = {in[i], in[i + 1]};
            writer.Sample(sampleArray);
        } else if (play) {
            if (bufferIndex < length) {
                right = GetBufferValueInterpolated(bufferIndex * 2.0f);
                left = GetBufferValueInterpolated(bufferIndex * 2.0f + 1.0f);
                bufferIndex += 1.0f;  // change to value other than 1 for different playback speed
            } else {
                bufferIndex = 0.0f;
            }
        }
        out[i] = left + in[i];
        out[i + 1] = right + in[i + 1];
    }
}

int GetNextFileIndex(const char *baseName, const char *extension) {
    DIR dir;
    FILINFO fno;
    int maxIndex = 0;

    // Open the root directory
    if (f_opendir(&dir, "/") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
            // Check if the file matches the pattern "baseNameX.extension"
            if (strstr(fno.fname, baseName) == fno.fname) {
                const char *numPart = fno.fname + strlen(baseName);
                if (isdigit(numPart[0])) {
                    int index = atoi(numPart);
                    const char *extPart = numPart + std::to_string(index).length();
                    if (strcmp(extPart, extension) == 0) {
                        maxIndex = std::max(maxIndex, index);
                    }
                }
            }
        }
        f_closedir(&dir);
    }
    return maxIndex == 0 ? 1 : maxIndex + 1;  // Start at 1 if no files exist
}

int main(void) {
    // hardware initialize
    hardware.Init();

#ifdef LOGG
    logger.StartLog(true);
    logger.PrintLine("logging activated");
#endif

    // store audio settings
    size_t blocksize = 4;
    sysSampleRate = hardware.AudioSampleRate();

    // start callback
    hardware.SetAudioBlockSize(blocksize);
    hardware.StartAudio(AudioCallback);

    // mount sd card
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    if (sdcard.Init(sd_cfg) != SdmmcHandler::Result::OK) {
#ifdef LOGG
        logger.PrintLine("SD card initialization failed");
#endif
        return 1;
    }
    if (fsi.Init(FatFSInterface::Config::MEDIA_SD) != FatFSInterface::Result::OK) {
#ifdef LOGG
        logger.PrintLine("File system initialization failed");
#endif
        return 1;
    }
    if (f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
#ifdef LOGG
        logger.PrintLine("File system mount failed");
#endif
        return 1;
    }

    System::Delay(100);

    // // read from sd card to buffer
    // char sampleName[] = "loop.wav";
    // int error = SetSample(sampleName);
    // logger.PrintLine("setsample error: %d", error);

    // intialze wavwriter
    WavWriter<16384>::Config config;
    config.samplerate = sysSampleRate;
    config.channels = 2;
    config.bitspersample = 16;
    writer.Init(config);

    // update loop
    for (;;) {
        hardware.ProcessDigitalControls();
        hardware.led1.Set(hardware.button1.Pressed(), 0.0f, 0.0f);  // onboard led indicates recording state
        hardware.UpdateLeds();

        if (hardware.button1.Pressed()) {
            record = true;
            saved = false;
            writer.Write();
        } else {
            record = false;
        }

        if (!record && !saved) {
            writer.SaveFile();
            recordedLength = writer.GetLengthSamps();
            saved = true;
        }

        if (hardware.button1.RisingEdge()) {
            char nextFileName[32];
            int nextIndex = GetNextFileIndex("rec", ".wav");
            snprintf(nextFileName, sizeof(nextFileName), "rec%d.wav", nextIndex);
            writer.OpenFile(nextFileName);  // Open WAV file
            bufferIndex = 0;
        }

        if (hardware.button2.RisingEdge()) {  // toggles playback
            bufferIndex = 0;
            play = !play;
        }
    }
}
