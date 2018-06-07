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

//pts:��ǰ������
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
			if (diff <= -sync_threshold)	//��Ƶ��
				delay = FFMAX(0, delay + diff);
			else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)//��Ƶ̫����
				delay = delay + diff;
			else if (diff >= sync_threshold)//��Ƶ��
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
	//������ʵʱ�������λ����
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
	//�����ǰPTS��ֵ���޶���Χ�ڣ����¼
	if (!isnan(last_duration) && last_duration > 0 && last_duration < max_frame_duration) {
		/* if duration of the last frame was sane, update last_duration in video state */
		frame_last_duration = last_duration;
	}
	else {
		//LOG("PTS:%lf LAST:%lf Dura:%lf\n", pts, frame_last_pts, last_duration);
	}

	double delay = Compute_target_delay(frame_last_duration, max_frame_duration);

	double time = (av_gettime() /1000 ) / TIME_GRAD;
	//�����ǰʱ��С��֡ʱ����ӳ�ʱ��,��ȴ�
	if (time < frame_timer + delay) {
		remaining_time = FFMIN(frame_timer + delay - time, remaining_time);
		return 1;
	}

	//��֡ʱ������ӳټ��㵱ǰ֡��ʾʱ��
	frame_timer += delay;
	//����ӳٴ���0��ͬʱ��ǰʱ�����֡����ʱ��֮�����AV_SYNC_THRESHOLD_MAX,��ǰ֡����ʱ��Ϊtime
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
		//ͨ���������ʱ���
		pts = av_rescale_q(pts, audio_steam_time_base, tb);
	}
	//������ʵʱ�������λ����
	AVRational tb = { 0 };
	tb.num = 1;
	tb.den = frame->sample_rate;
	double dpts = (pts * av_q2d(tb) + (double)frame->nb_samples / frame->sample_rate) * 1000;
	return dpts;
}