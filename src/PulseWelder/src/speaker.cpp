/*
   File: speaker.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.1
   Creation: Jan-15-2020
   Revised:
   Public Release:
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include "PulseWelder.h"
#include "screen.h"
#include "XT_DAC_Audio.h"
#include "digPot.h"
#include "config.h"
#include "speaker.h"

#include "dacAudio.h"

// Global Audio Generation
extern XT_DAC_Audio_Class DacAudio;

// Global Wave Files.
extern XT_Wav_Class n000;
extern XT_Wav_Class n001;
extern XT_Wav_Class n002;
extern XT_Wav_Class n003;
extern XT_Wav_Class n004;
extern XT_Wav_Class n005;
extern XT_Wav_Class n006;
extern XT_Wav_Class n007;
extern XT_Wav_Class n008;
extern XT_Wav_Class n009;
extern XT_Wav_Class n010;

// Global Music Audio Generation
extern XT_MusicScore_Class highBeep;
extern XT_MusicScore_Class lowBeep;

// Global Audio Sequencer
extern XT_Sequence_Class Sequence;

Speaker spkr;

extern byte spkrVolSwitch; // Audio Volume, five levels.

Speaker::Speaker() {
    promoMsg.Speed     = 1.0;           // Normal Playback Speed.
    promoMsg.Volume    = 127;           // Maximum Sub-Volume (0-127 allowed).
    Sequence.Volume = 127;         // Maximum sub-volume.
    Sequence.Repeat = 0;           // Don't Repeat.
 
}
void Speaker::stopSounds() {
  DacAudio.StopAllSounds();
  Sequence.RemoveAllPlayItems();
}

void Speaker::fillBuffer() {
  DacAudio.FillBuffer();
}
void Speaker::volume(byte vol) {
  DacAudio.DacVolume = vol;
}

void Speaker::play(XT_PlayListItem_Class& sound) {
  if (spkrVolSwitch != VOL_OFF) {
    DacAudio.Play(&sound, true);
  }
}

void Speaker::playToEnd(XT_PlayListItem_Class& sound) {
  if (spkrVolSwitch != VOL_OFF) {
    DacAudio.Play(&sound, true);

    while (sound.TimeLeft) {
      DacAudio.FillBuffer();
    }
  }
}

void Speaker::lowBeep() {
  play(::lowBeep);
}

void Speaker::highBeep() {
  play(::highBeep);
}

void Speaker::bleep() {
  play(::bleep);
}

void Speaker::bloop() {
  play(::bloop);
}

void Speaker::blip() {
  play(::blip);
}

void Speaker::beep() {
  play(::beep);
}

void Speaker::ding() {
  play(::ding);
}

void Speaker::limitHit(XT_PlayListItem_Class& sound, boolean condition) {
  if (condition) { // End of travel reached.
    bloop();
  }
  else {
    play(sound);
  }
}

// *********************************************************************************************
// Add the wav file item for the 0-9 number passed by calller
static void AddNumberToSequence(int theNumber) {
  static XT_Wav_Class* digit[10] = { &n000, &n001, &n002, &n003, &n004, &n005, &n006, &n007, &n008, &n009 };
  Sequence.AddPlayItem( (theNumber < 10 && theNumber >= 0) ? digit[theNumber] : &silence100ms );
}

void Speaker::addDigitSounds(uint32_t val) {        
        if (spkrVolSwitch != VOL_OFF) {
          const int max_num_digits = 10; // 32 bits == 10 base10 digits max.
          uint32_t digits[max_num_digits];
          int count = 0;
          for (; val > 0 && count < max_num_digits; count++ ) {
            digits[count] = val % 10;
            val /= 10; // shift to next digit
          }
          if (count == 0) { count = 1; digits[0] = 0; }
          // special case for value == 0 

          for (; count; count-- ) { AddNumberToSequence(digits[count]); }
          DacAudio.Play(&Sequence, true);
        }
}

void Speaker::addSoundList(std::vector<XT_Wav_Class*> sounds) {
  if (spkrVolSwitch != VOL_OFF) {
        std::for_each(sounds.begin(),sounds.end(), [](XT_Wav_Class* item) { Sequence.AddPlayItem(item); });
  }
}

void Speaker::playSoundList() {
  DacAudio.Play(&Sequence);
}


// EOF
