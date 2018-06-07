#pragma once

#include "MClock.h"
#include <stdint.h>

class MediaSynchronise
{
public:
	MediaSynchronise();
	~MediaSynchronise();
	void Init(AVRational videoStream, AVRational audioStream);
	void UpdateAuioPts(int64_t pts,int nb_samples, int sample_rate);
	int GetVideoDelay(int64_t pts);
	int64_t GetVideoTimestamp(int64_t pts);
	int64_t GetAudioTimestamp(AVFrame * frame);
private:
	int64_t GetAudioPTS(int64_t pts, int nb_samples, int SampleRate);
	void UpdateVideoPts(int64_t npts, double pts);
	double GetClockDelay();
	double Compute_target_delay(double delay, double max_frame_duration);

	int CalculateDelay(int64_t npts, double pts, double &remaining_time);
private:
	Clock AudioClock;
	Clock VideoClock;

	AVRational time_base;
	AVRational video_steam_time_base;
	AVRational audio_steam_time_base;

	double frame_last_pts;
	double frame_last_duration;
	double max_frame_duration;
	double frame_timer;
	double remaining_time;

	int64_t audio_frame_next_pts = AV_NOPTS_VALUE;//ÏÂÒ»°üPTS

};

