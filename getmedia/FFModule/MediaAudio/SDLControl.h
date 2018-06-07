#pragma once

#include <SDL/SDL.H>

#include "MediaSynchronise.h"
#include "MediaSource/MediaDecoder.h"
#include "MemManager/EvoQueue.hpp"
#include "MemManager/EvoPacket.h"
#include "core/thread.h"

void SDLDeleteEvoPacket(EvoPacket**);

class SDLControl
{
public:
	SDLControl();
	~SDLControl();
	void AttachSynchronise(MediaSynchronise *pSynchronise);

	void initSDL();

	int initVideo(int displayW, int displayH, int textureW, int textureH,Uint32 format);
	int openAudio(int freq, uint8_t channels, uint16_t samples);
	void playAudio();
	void pauseAudio();
	void closeAudio();
	void muteAudio(bool mute);
	bool isMuteAudio();
	static void audioCallback(void *userdata, Uint8 *stream, int len);
	void audioCall(Uint8 *stream, int len);
	void updateVideo(uint8_t* data, int length);

	void setAudioFlag(bool b);
	void setVolume(int v);
	int getVolume();

	int AddAudioData(EvoPacket *packet);
	EvoPacket *GetAudioData();
	void clearAudioData();
	void resetAudioData();
	int GetAudioDataLength();
private:
	SDL_Window*   screen;
	SDL_Renderer* render;
	SDL_Texture*  texture;
	SDL_AudioSpec audioSpec;
	SDL_AudioSpec audioSource;

	uint8_t audioBuff[192000 * 3 / 2];
	uint8_t dummyBuff[4096*2];
	int audioBuffIndex;
	int audioBuffSize;

	MediaSynchronise *Synchronise;
	int64_t audio_frame_next_pts;//ÏÂÒ»°üPTS

	bool muteVolume;
	int volume;
	bool hasAudio;
	std::mutex  mutex;
	
	volatile bool isSync;
	double videoCurrentPts;
	double videoCurrentPtsTime;
	volatile int channels;
	double audioDiffAvgCoef;
	int audioDiffAvgCount;
	double audioDiffAvgThreshold;
	double audioDiffCum;

	EvoQueue<EvoPacket, SDLDeleteEvoPacket> audioQueue;
};

