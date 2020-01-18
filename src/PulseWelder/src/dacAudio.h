/*
   File: dacAudio.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.1
   Creation: Sep-11-2019
   Revised: Dec-29-2019.
   Public Release: Jan-03-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.

   The female voice is Microsoft Zira. https://developer.microsoft.com/en-us/microsoft-edge/testdrive/demos/speechsynthesis/
   All wav files were prepared in Audacity V2.3.0.
   Sample Rate: 16KHz. This provides a good compromise of file size versus audio quality.
   The audio levels were set to maximum (no clipping) using the Effect->Amplify tool. Remove unnecessary silent gaps.
   Export using the WAV format; Save as type "Other Uncompressed File," WAV header, unsigned 8-bit PCM encoding.
 */

#include "PulseWelder.h"
#include "XT_DAC_Audio.h"

const int AUDIO_BUFF_SZ = 5000; // Size of audio buffer.

int8_t PROGMEM highBeepTone[] = {
  NOTE_A5, BEAT_05, SCORE_END };

int8_t PROGMEM lowBeepTone[] = {
  NOTE_E5, BEAT_075, SCORE_END };

// Audio Files
extern const uint8_t promo_wav_start[] asm ("_binary_src_wav_promo_wav_start");
// extern const uint8_t promo_wav_end[] asm("_binary_src_wav_promo_wav_end");
const unsigned char *promo_wav = (const unsigned char *)promo_wav_start;

extern const uint8_t ding_wav_start[] asm ("_binary_src_wav_ding_wav_start");
// extern const uint8_t ding_wav_end[] asm("_binary_src_wav_ding_wav_end");
const unsigned char *ding_wav = (const unsigned char *)ding_wav_start;

extern const uint8_t beep_wav_start[] asm ("_binary_src_wav_beep_wav_start");
// extern const uint8_t beep_wav_end[] asm("_binary_src_wav_beep_wav_end");
const unsigned char *beep_wav = (const unsigned char *)beep_wav_start;

extern const uint8_t blip_wav_start[] asm ("_binary_src_wav_blip_wav_start");
// extern const uint8_t blip_wav_end[] asm("_binary_src_wav_blip_wav_end");
const unsigned char *blip_wav = (const unsigned char *)blip_wav_start;

extern const uint8_t bleep_wav_start[] asm ("_binary_src_wav_bleep_wav_start");
// extern const uint8_t bleep_wav_end[] asm("_binary_src_wav_bleep_wav_end");
const unsigned char *bleep_wav = (const unsigned char *)bleep_wav_start;

extern const uint8_t bloop_wav_start[] asm ("_binary_src_wav_bloop_wav_start");
// extern const uint8_t bloop_wav_end[] asm("_binary_src_wav_bloop_wav_end");
const unsigned char *bloop_wav = (const unsigned char *)bloop_wav_start;

extern const uint8_t currentOn_wav_start[] asm ("_binary_src_wav_currentOn_wav_start");
// extern const uint8_t currentOn_wav_end[] asm("_binary_src_wav_currentOn_wav_end");
const unsigned char *currentOn_wav = (const unsigned char *)currentOn_wav_start;

extern const uint8_t currentOff_wav_start[] asm ("_binary_src_wav_currentOff_wav_start");
// extern const uint8_t currentOn_wav_end[] asm("_binary_src_wav_currentOff_wav_end");
const unsigned char *currentOff_wav = (const unsigned char *)currentOff_wav_start;

extern const uint8_t overheat_wav_start[] asm ("_binary_src_wav_overheat_wav_start");
// extern const uint8_t overheat_wav_end[] asm("_binary_src_wav_overheat_wav_end");
const unsigned char *overheat_wav = (const unsigned char *)overheat_wav_start;

extern const uint8_t increaseMsg_wav_start[] asm ("_binary_src_wav_increaseMsg_wav_start");
// extern const uint8_t increaseMsg_wav_end[] asm("_binary_src_wav_increaseMsg_wav_end");
const unsigned char *increaseMsg_wav = (const unsigned char *)increaseMsg_wav_start;

extern const uint8_t decreaseMsg_wav_start[] asm ("_binary_src_wav_decreaseMsg_wav_start");
// extern const uint8_t decreaseMsg_wav_end[] asm("_binary_src_wav_decreaseMsg_wav_end");
const unsigned char *decreaseMsg_wav = (const unsigned char *)decreaseMsg_wav_start;

extern const uint8_t silence100ms_wav_start[] asm ("_binary_src_wav_silence100ms_wav_start");
// extern const uint8_t silence100ms_wav_end[] asm("_binary_src_wav_silence100ms_wav_end");
const unsigned char *silence100ms_wav = (const unsigned char *)silence100ms_wav_start;

extern const uint8_t n000_wav_start[] asm ("_binary_src_wav_0000_wav_start");
const unsigned char *n000_wav = (const unsigned char *)n000_wav_start;

extern const uint8_t n001_wav_start[] asm ("_binary_src_wav_0001_wav_start");
const unsigned char *n001_wav = (const unsigned char *)n001_wav_start;

extern const uint8_t n002_wav_start[] asm ("_binary_src_wav_0002_wav_start");
const unsigned char *n002_wav = (const unsigned char *)n002_wav_start;

extern const uint8_t n003_wav_start[] asm ("_binary_src_wav_0003_wav_start");
const unsigned char *n003_wav = (const unsigned char *)n003_wav_start;

extern const uint8_t n004_wav_start[] asm ("_binary_src_wav_0004_wav_start");
const unsigned char *n004_wav = (const unsigned char *)n004_wav_start;

extern const uint8_t n005_wav_start[] asm ("_binary_src_wav_0005_wav_start");
const unsigned char *n005_wav = (const unsigned char *)n005_wav_start;

extern const uint8_t n006_wav_start[] asm ("_binary_src_wav_0006_wav_start");
const unsigned char *n006_wav = (const unsigned char *)n006_wav_start;

extern const uint8_t n007_wav_start[] asm ("_binary_src_wav_0007_wav_start");
const unsigned char *n007_wav = (const unsigned char *)n007_wav_start;

extern const uint8_t n008_wav_start[] asm ("_binary_src_wav_0008_wav_start");
const unsigned char *n008_wav = (const unsigned char *)n008_wav_start;

extern const uint8_t n009_wav_start[] asm ("_binary_src_wav_0009_wav_start");
const unsigned char *n009_wav = (const unsigned char *)n009_wav_start;

extern const uint8_t n010_wav_start[] asm ("_binary_src_wav_0010_wav_start");
const unsigned char *n010_wav = (const unsigned char *)n010_wav_start;

// Wave Audio Generation
XT_DAC_Audio_Class DacAudio(DAC_PIN, DAC_ISR_TMR, AUDIO_BUFF_SZ); // Create the main Audio player class object.
XT_Wav_Class promoMsg(promo_wav);
XT_Wav_Class ding(ding_wav);
XT_Wav_Class beep(beep_wav);
XT_Wav_Class bloop(bloop_wav);
XT_Wav_Class blip(blip_wav);
XT_Wav_Class bleep(bleep_wav);
XT_Wav_Class currentOnMsg(currentOn_wav);
XT_Wav_Class currentOffMsg(currentOff_wav);
XT_Wav_Class overHeatMsg(overheat_wav);
XT_Wav_Class increaseMsg(increaseMsg_wav);
XT_Wav_Class decreaseMsg(decreaseMsg_wav);
XT_Wav_Class silence100ms(silence100ms_wav);

XT_Wav_Class n000(n000_wav);
XT_Wav_Class n001(n001_wav);
XT_Wav_Class n002(n002_wav);
XT_Wav_Class n003(n003_wav);
XT_Wav_Class n004(n004_wav);
XT_Wav_Class n005(n005_wav);
XT_Wav_Class n006(n006_wav);
XT_Wav_Class n007(n007_wav);
XT_Wav_Class n008(n008_wav);
XT_Wav_Class n009(n009_wav);
XT_Wav_Class n010(n010_wav);

// Music Audio Generation
XT_MusicScore_Class highBeep(highBeepTone, TEMPO_PRESTO, INSTRUMENT_PIANO);
XT_MusicScore_Class lowBeep(lowBeepTone, TEMPO_PRESTO, INSTRUMENT_PIANO);

// Audio Sequencer
XT_Sequence_Class Sequence;

// EOF
