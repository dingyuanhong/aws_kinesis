#include "MediaSynchronise.h"

MediaSynchronise::MediaSynchronise()
{
	frame_last_pts = 0.0;
	frame_last_duration = 0.0;
	max_frame_duration = 0.0;
	frame_timer = 0.0;
	remaining_time = 0.0;
}

MediaSynchronise::~MediaSynchronise()
{
}

void MediaSynchronise::Init(AVRational videoStream, AVRational audioStream)
{
	init_clock(&AudioClock);
	init_clock(&VideoClock);

	video_steam_time_base = videoStream;
	audio_steam_time_base = audioStream;
	//AVRational time_base;
	time_base.num = 1000;
	time_base.den = (int)(TIME_GRAD);

	max_frame_duration = (3600.0/1000)*av_q2d(time_base);
}

//pts:当前样本数
void MediaSynchronise::UpdateAuioPts(int64_t pts,int nb_samples, int sample_rate)
{
	pts = GetAudioPTS(pts, nb_samples, sample_rate);

	AVRational tb = { 0 };
	tb.num = 1;
	tb.den = sample_rate;
	double audio_clock = (pts * av_q2d(tb) + (double)nb_samples / sample_rate)*av_q2d(time_base);
	//LOGA("AudioTime:%lf PTS:%d\n", audio_clock,pts);
	set_clock(&AudioClock, audio_clock, pts);
}

int64_t MediaSynchronise::GetAudioPTS(int64_t pts, int nb_samples,int SampleRate)
{
	AVRational tb = { 0 };
	tb.num = 1;
	tb.den = SampleRate;

	if (pts != AV_NOPTS_VALUE)
	{
		pts = av_rescale_q(pts, audio_steam_time_base, tb);
	}
	else if (audio_frame_next_pts != AV_NOPTS_VALUE)
	{
		AVRational AVR_tmp = { 0 };
		AVR_tmp.num = 1;
		AVR_tmp.den = SampleRate;
		pts = av_rescale_q(audio_frame_next_pts, AVR_tmp, tb);
	}
	if (pts != AV_NOPTS_VALUE) {
		audio_frame_next_pts = pts + nb_samples;
	}
	return pts;
}

void MediaSynchronise::UpdateVideoPts(int64_t npts,double pts)
{
	//LOGA("VideoTime:%lf PTS:%d\n", pts,npts);
	set_clock(&VideoClock, pts, npts);
}

double MediaSynchronise::Compute_target_delay(double delay, double max_frame_duration)
{
	double sync_threshold, diff;

	/* update delay to follow master synchronisation source */
	{
		/* if video is slave, we try to correct big delays by
		duplicating or deleting a frame */
		diff = GetClockDelay();

		/* skip or repeat frame. We take into account the
		delay to compute the threshold. I still don't know
		if it is the best guess */
		sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
		if (!isnan(diff) && fabs(diff) < max_frame_duration) {
			if (diff <= -sync_threshold)	//视频慢
				delay = FFMAX(0, delay + diff);
			else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)//视频太快了
				delay = delay + diff;
			else if (diff >= sync_threshold)//视频快
				delay = 2 * delay;
		}
	}

	av_dlog(NULL, "video: delay=%0.3f A-V=%f\n",
		delay, -diff);

	return delay;
}

double MediaSynchronise::GetClockDelay()
{
	double diff = get_clock(&VideoClock) - get_clock(&AudioClock);
	return diff;
}

int64_t MediaSynchronise::GetVideoTimestamp(int64_t pts) {
	//计算真实时间戳，单位毫秒
	double dpts = (pts == AV_NOPTS_VALUE) ? NAN : (pts * av_q2d(video_steam_time_base)*1000);
	return dpts;
}

int MediaSynchronise::GetVideoDelay(int64_t pts)
{
	double remaining_time = av_q2d(time_base);
	remaining_time = (remaining_time / 60);

	double dpts = GetVideoTimestamp(pts);
	dpts = (pts == AV_NOPTS_VALUE) ? NAN : (dpts /1000) * av_q2d(time_base);

	int nRet = CalculateDelay(pts,dpts, remaining_time);

	remaining_time = remaining_time / av_q2d(time_base);
	if (nRet == 1) {
		return (int)(remaining_time*1000);
	}
	return nRet;
}

int MediaSynchronise::CalculateDelay(int64_t npts,double pts, double &remaining_time)
{
	/* compute nominal last_duration */
	double last_duration = pts - frame_last_pts;
	//如果当前PTS差值在限定范围内，则记录
	if (!isnan(last_duration) && last_duration > 0 && last_duration < max_frame_duration) {
		/* if duration of the last frame was sane, update last_duration in video state */
		frame_last_duration = last_duration;
	}
	else {
		//LOG("PTS:%lf LAST:%lf Dura:%lf\n", pts, frame_last_pts, last_duration);
	}

	double delay = Compute_target_delay(frame_last_duration, max_frame_duration);

	double time = (av_gettime() /1000 ) / TIME_GRAD;
	//如果当前时间小于帧时间加延迟时间,则等待
	if (time < frame_timer + delay) {
		remaining_time = FFMIN(frame_timer + delay - time, remaining_time);
		return 1;
	}

	//上帧时间加上延迟计算当前帧显示时间
	frame_timer += delay;
	//如果延迟大于0，同时当前时间和上帧播放时间之差大于AV_SYNC_THRESHOLD_MAX,则当前帧播放时间为time
	if (delay > 0 && time - frame_timer > AV_SYNC_THRESHOLD_MAX)
		frame_timer = time;

	if (!isnan(pts)) {
		UpdateVideoPts(npts,pts);
		frame_last_pts = pts;
	}
	return 0;
}

int64_t MediaSynchronise::GetAudioTimestamp(AVFrame * frame)
{
	int64_t pts = frame->pts;

	if (pts != AV_NOPTS_VALUE)
	{
		AVRational tb = { 0 };
		tb.num = 1;
		tb.den = frame->sample_rate;
		//通过自身调整时间戳
		pts = av_rescale_q(pts, audio_steam_time_base, tb);
	}
	//计算真实时间戳，单位毫秒
	AVRational tb = { 0 };
	tb.num = 1;
	tb.den = frame->sample_rate;
	double dpts = (pts * av_q2d(tb) + (double)frame->nb_samples / frame->sample_rate) * 1000;
	return dpts;
}