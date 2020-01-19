/*
   File: speaker.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.2
   Creation: Jan-14-2020
   Revised: Jan-14-2020
   Public Release: Jan-15-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */
#ifndef __SPEAKER_H__
#define __SPEAKER_H__

#include "XT_DAC_Audio.h"

extern XT_Wav_Class beep;
extern XT_Wav_Class blip;
extern XT_Wav_Class bleep;
extern XT_Wav_Class bloop;
extern XT_Wav_Class ding;
extern XT_Wav_Class silence100ms;

extern XT_Wav_Class overHeatMsg;
extern XT_Wav_Class promoMsg;

// Global Music Audio Generation
extern XT_MusicScore_Class highBeep;
extern XT_MusicScore_Class lowBeep;


class Speaker {
public:
  Speaker();

  // low-level functions

  // play a sound instantly, mixed with any sounds currently playing
  void play(XT_PlayListItem_Class& sound);

  // play a sound instantly and return once last data is filled into buffer
  // and sound is fully played
  void playToEnd(XT_PlayListItem_Class& sound);

  // set volume of speaker
  void volume(byte vol);

  // feed data into audio buffer, has to be called frequently
  void fillBuffer();

  // stop playing any sounds, empty sound sequence
  void stopSounds();

  // play all sounds in a sound sequence
  void playSoundList();

  // add a list of sound files to the play sequence
  void addSoundList(std::vector<XT_Wav_Class*> sounds);

  // translate unsigned number to digits
  void addDigitSounds(uint32_t val);

  // some commonly used functions/sounds

  // if condition is true, play error sound,
  // otherwise play sound
  void limitHit(XT_PlayListItem_Class& sound,
                boolean                condition);

  // shortcut to standard sounds
  void lowBeep();
  void highBeep();
  void bleep();
  void bloop();
  void blip();
  void beep();
  void ding();
};

extern Speaker spkr;

#endif
// EOF
