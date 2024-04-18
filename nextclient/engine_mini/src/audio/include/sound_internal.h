#pragma once
#include "alure2.h"
#include "../../hlsdk.h"

struct sfxcache_t
{
  int  length;
  int  loopstart;
  int  speed;
  int  width;
  int  stereo;
  byte data[1];  // variable sized
};

struct dma_t
{
  qboolean		gamealive;
  qboolean		soundalive;
  qboolean		splitbuffer;
  int				channels;
  int				samples;				// mono samples in buffer
  int				submission_chunk;		// don't mix less than this #
  int				samplepos;				// in mono samples
  int				samplebits;
  int				speed;
  int				dmaspeed;
  unsigned char*	buffer;
};

struct channel_t
{
  sfx_t*	sfx;			// sfx number
  int		leftvol;		// 0-255 volume
  int		rightvol;		// 0-255 volume

  int end;			// end time in global paintsamples
  int pos;			// sample position in sfx

  int looping;		// where to loop, -1 = no looping

  int entnum;			// to allow overriding a specific sound
  int entchannel;		// TODO: Define as enum, modify const.h then -Enko

  vec3_t origin;		// origin of sound effect
  vec_t dist_mult;	// distance multiplier (attenuation/clipK)

  int master_vol;		// 0-255 master volume

  int isentence;		// true if playing linked sentence
  int iword;

  int pitch;
};

struct voxword_t
{
  int volume;             // increase percent, ie: 125 = 125% increase
  int pitch;              // pitch shift up percent
  int start;              // offset start of wave percent
  int end;                // offset end of wave percent
  uint64_t cbtrim;        // end of wave after being trimmed to 'end'
  int fKeepCached;        // 1 if this word was already in cache before sentence referenced it
  uint64_t samplefrac;    // if pitch shifting, this is position into wav * 256
  int timecompress;       // % of wave to skip during playback (causes no pitch shift)
  sfx_t *sfx;             // name and cache pointer
};

struct wavinfo_t
{
  uint64_t samples;
  uint64_t loopstart;
  uint64_t loopend;
  ALuint samplerate;
  alure::SampleType stype;
  alure::ChannelConfig channels;
  bool looping;
};

// a sound with no channel is a local only sound
constexpr auto SND_VOLUME = (1 << 0);       // a byte
constexpr auto SND_ATTENUATION = (1<<1);    // a byte
constexpr auto SND_LARGE_INDEX = (1<<2);    // a long
constexpr auto SND_PITCH = (1<<3);
constexpr auto SND_SENTENCE = (1<<4);
constexpr auto SND_STOP = (1<<5);
constexpr auto SND_CHANGE_VOL = (1<<6);
constexpr auto SND_CHANGE_PITCH = (1<<7);
constexpr auto SND_SPAWNING = (1<<8);

constexpr auto CVOXWORDMAX = 32;
constexpr auto CVOXZEROSCANMAX = 255;
constexpr auto CAVGSAMPLES = 10;
constexpr auto CSXROOM = 29;