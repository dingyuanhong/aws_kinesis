#include "MediaDecoder.h"
#include "MemManager/EvoLock.h"
#include "core/log.h"

#ifdef USE_NEW_DECODER
static AVCodecContext *CreateCodecContent(AVCodecParameters *codecpar)
{
	AVCodecContext *codecContext = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(codecContext, codecpar);
	return codecContext;
}
#endif

MediaDecoder::MediaDecoder(enum DECODER_TYPE type)
	: AudioDataSet(10)
	, VideoDataSet(10)
	, AudioDataSourceSet(10)
{
	this->DecoderType = type;
	this->IsGetAudioSource = false;
	this->Init();
	uv_rwlock_init(&ConvertMutex);
}

MediaDecoder::~MediaDecoder()
{
	Clear();
	this->ClearAllQueue();
	this->ResetAllQueue();
	uv_rwlock_destroy(&ConvertMutex);
}

void MediaDecoder::Clear()
{
	this->Close();
	this->CloseBuffer();
}

void MediaDecoder::Close()
{
	this->AudioIndex = -1;
	this->VideoIndex = -1;
	this->DecoderIndex = -1;

	if (this->VideoCodecCtx != NULL) {
		avcodec_close(this->VideoCodecCtx);
#ifdef USE_NEW_DECODER
		avcodec_free_context(&VideoCodecCtx);
#endif
		this->VideoCodecCtx = NULL;
	}

	if (this->AudioCodecCtx != NULL) {
		avcodec_close(this->AudioCodecCtx);
#ifdef USE_NEW_DECODER
		avcodec_free_context(&AudioCodecCtx);
#endif
		this->AudioCodecCtx = NULL;
	}
	if (this->FormatCtx != NULL) {
		avformat_close_input(&this->FormatCtx);
		this->FormatCtx = NULL;
	}

	if (this->VDecoder)
	{
		delete this->VDecoder;
		this->VDecoder = NULL;
	}

	if (this->ADecoder)
	{
		delete this->ADecoder;
		this->ADecoder = NULL;
	}

	if (this->Convert)
	{
		delete this->Convert;
		this->Convert = NULL;
	}
}

void MediaDecoder::CloseBuffer()
{
	this->AudioIndex = -1;
	this->VideoIndex = -1;

	if (this->MediaPacket != NULL) {
		av_packet_free(&this->MediaPacket);
		this->MediaPacket = NULL;
	}
}

void MediaDecoder::Init()
{
	this->AudioIndex = -1;
	this->VideoIndex = -1;
	this->DecoderIndex = -1;

	this->avioCtx = NULL;
	this->FormatCtx = NULL;
	this->AudioCodecCtx = NULL;
	this->VideoCodecCtx = NULL;
	this->AudioCodec = NULL;
	this->VideoCodec = NULL;

	this->AudioStream = NULL;
	this->VideoStream = NULL;

	//this->MediaPacket = (AVPacket *)av_mallocz(sizeof(AVPacket));
	this->MediaPacket = (AVPacket *)av_packet_alloc();

	this->StartPos = 0;
	this->IsEnd = false;
	this->VideoFrameCount = 0;
	this->AudioFrameCount = 0;
	this->LastVideoPTS = 0;

	this->VDecoder = NULL;
	this->ADecoder = NULL;
	this->Convert = NULL;

	this->DesInfo.Width = 0;
	this->DesInfo.Height = 0;
	this->DesInfo.Format = AV_PIX_FMT_NONE;
	av_register_all();
	avformat_network_init();
}

int MediaDecoder::Open(PIO_Read_Packet read_cb,void* opaque, int io_size)
{
	if (this->DecoderType != MediaDecoder::STREAN)
	{
		return -1;
	}
	
	if (io_size <= 0)
	{
		return -1;
	}

	unsigned char * IOBuffer = (unsigned char *)av_malloc(io_size);

	avioCtx = avio_alloc_context(IOBuffer, io_size, 0, opaque, read_cb, NULL, NULL);

	if (NULL == avioCtx)
	{
		LOGD("create avio_alloc_context error !!\n");
		return -1;
	}

	this->FormatCtx = avformat_alloc_context();

	FormatCtx->pb = avioCtx;
	FormatCtx->flags = AVFMT_FLAG_CUSTOM_IO;
	AVDictionary* options = NULL;
	//调整探测头避免过长的等待
	av_dict_set(&options, "max_analyze_duration", "100", 0);
	av_dict_set(&options, "probesize", "1024", 0);

	if (avformat_open_input(&(this->FormatCtx), NULL, NULL, &options) != 0) {
		return -1;
	}
	if (avformat_find_stream_info(this->FormatCtx, NULL) < 0) {
		return -1;
	}
#ifndef USE_NEW_DECODER
	for (size_t i = 0; i < this->FormatCtx->nb_streams; i++) {
		if (this->FormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			this->VideoIndex = (int)i;
		}
		else if (this->FormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			this->AudioIndex = (int)i;
		}
	}
#else
	for (size_t i = 0; i < this->FormatCtx->nb_streams; i++) {
		if (this->FormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			this->VideoIndex = (int)i;
		}
		else if (this->FormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			this->AudioIndex = (int)i;
		}
	}
#endif

	bool audioRet = this->FindAudio();
	bool videoRet = this->FindVideo();

	InitDecoder(videoRet, audioRet);

	//音频跟视频至少要有一种
	if (audioRet || videoRet) {
		return 0;
	}
	else {
		return -1;
	}
}

int MediaDecoder::Open(const char* url)
{
	if (this->DecoderType == MediaDecoder::STREAN)
	{
		return -1;
	}

	size_t i = 0;

	this->FormatCtx = avformat_alloc_context();

	if (avformat_open_input(&(this->FormatCtx), url, NULL, NULL) != 0) {
		return -1;
	}
	if (avformat_find_stream_info(this->FormatCtx, NULL) < 0) {
		return -1;
	}
#ifndef USE_NEW_DECODER
	for (size_t i = 0; i < this->FormatCtx->nb_streams; i++) {
		if (this->FormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			this->VideoIndex = (int)i;
		}
		else if (this->FormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			this->AudioIndex = (int)i;
		}
	}
#else
	for (i = 0; i < this->FormatCtx->nb_streams; i++) {
		if (this->FormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			this->VideoIndex = (int)i;
		}
		else if (this->FormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			this->AudioIndex = (int)i;
		}
	}
#endif
	bool audioRet = this->FindAudio();
	bool videoRet = this->FindVideo();

	InitDecoder(videoRet,audioRet);

	//音频跟视频至少要有一种
	if (audioRet || videoRet) {
		if (DecoderIndex == -1)
		{
			return -1;
		}
		this->LastVideoPTS = 0;
		return 0;
	}
	else {
		return -1;
	}
}

void MediaDecoder::InitDecoder(bool videoret,bool audioret)
{
	if (VDecoder != NULL)
	{
		delete VDecoder;
		VDecoder = NULL;
	}

	if (ADecoder != NULL)
	{
		delete ADecoder;
		ADecoder = NULL;
	}
	
	//创建实际解码器
	if (this->DecoderType == MediaDecoder::VIDEO) {
		if (videoret)
		{
			VDecoder = new VideoDecoder(VideoCodecCtx);
		}
	}
	else if (this->DecoderType == MediaDecoder::AUDIO)
	{
		if (audioret) ADecoder = new AudioDecoder(AudioCodecCtx);
	}
	else
	{
		if (videoret)
		{
			VDecoder = new VideoDecoder(VideoCodecCtx);
		}
		if (audioret) ADecoder = new AudioDecoder(AudioCodecCtx);
	}

	SetConvert(DesInfo);
}

bool MediaDecoder::FindAudio()
{
	if (this->AudioIndex != -1) {
		if (this->DecoderType == MediaDecoder::AUDIO) {
			this->DecoderIndex = this->AudioIndex;
		}
		this->AudioStream = this->FormatCtx->streams[this->AudioIndex];
#ifndef USE_NEW_DECODER
		this->AudioCodecCtx = this->FormatCtx->streams[this->AudioIndex]->codec;
#else
		this->AudioCodecCtx = CreateCodecContent(this->FormatCtx->streams[this->AudioIndex]->codecpar);
#endif
		this->AudioCodec = avcodec_find_decoder(this->AudioCodecCtx->codec_id);

		if (avcodec_open2(this->AudioCodecCtx, this->AudioCodec, NULL) < 0) {
			return false;
		}
		return true;
	}
	else {
		return false;
	}
}

bool MediaDecoder::FindVideo()
{
	if (this->VideoIndex != -1) {
		if (this->DecoderType == MediaDecoder::VIDEO) {
			this->DecoderIndex = this->VideoIndex;
		}
		this->VideoStream = this->FormatCtx->streams[this->VideoIndex];
#ifndef USE_NEW_DECODER
		this->VideoCodecCtx = this->FormatCtx->streams[this->VideoIndex]->codec;
#else
		this->VideoCodecCtx = CreateCodecContent(this->FormatCtx->streams[this->VideoIndex]->codecpar);
#endif
		this->VideoCodec = GetBestVideoDecoder(this->VideoCodecCtx->codec_id);
		if(this->VideoCodec == NULL) this->VideoCodec = avcodec_find_decoder(this->VideoCodecCtx->codec_id);

		if (true) this->VideoCodecCtx->flags2 |= CODEC_FLAG2_FAST;
		if (this->VideoCodec->capabilities & CODEC_CAP_DR1)
			this->VideoCodecCtx->flags |= CODEC_FLAG_EMU_EDGE;

		if (avcodec_open2(this->VideoCodecCtx, this->VideoCodec, NULL) < 0) {
			return false;
		}

		if (this->VideoCodecCtx->hwaccel == NULL) {
			LOGD("未开启硬件加速.\n");
		}
		else {
			LOGD("硬件加速中(%s).\n", this->VideoCodecCtx->hwaccel->name);
		}

		return true;
	}
	else {
		return false;
	}
}

AVStream * MediaDecoder::GetAudioStream()
{
	return this->AudioStream;
}

AVStream * MediaDecoder::GetVideoStream()
{
	return this->VideoStream;
}

void MediaDecoder::ResetIsEnd()
{
	IsEnd = false;
}

bool MediaDecoder::IsReadEnd() {
	return IsEnd;
}

bool MediaDecoder::CheckIsEnd()
{
	if (IsEnd == true
		&& VideoDataSet.Count() == 0
		&& AudioDataSet.Count() == 0
		&& AudioDataSourceSet.Count() == 0
		)
	{
		return true;
	}
	return false; 
}

bool MediaDecoder::CheckAudioAvailable()
{
	if (AudioIndex == -1) return false;
	if (ADecoder == NULL) return false;
	if (DecoderType == VIDEO) return false;
	return true;
}

bool MediaDecoder::CheckVideoAvailable()
{
	if (VideoIndex == -1) return false;
	if (VDecoder == NULL) return false;
	if (DecoderType == AUDIO) return false;
	return true;
}

int64_t MediaDecoder::GetDuration()
{
	if (this->FormatCtx != NULL) {
		return this->FormatCtx->duration / 1000;
	}
	return 0;
}

struct EvoAudioInfo MediaDecoder::GetAudioInfo() const
{
	struct EvoAudioInfo info = { 0,0,AV_PIX_FMT_NONE };

	if (AudioCodecCtx != NULL) 
	{
		if (this->AudioCodecCtx->codec_id == AV_CODEC_ID_MP2 || this->AudioCodecCtx->codec_id == AV_CODEC_ID_MP3)
			info.SampleSize = 1152;
		else
			info.SampleSize = 1024;

		info.SampleRate = this->AudioCodecCtx->sample_rate;
		info.Channels = this->AudioCodecCtx->channels;
		info.BitSize = this->AudioCodecCtx->bit_rate;
	}
	return info;
}

struct EvoVideoInfo MediaDecoder::GetVideoInfo() const
{
	struct EvoVideoInfo info = { 0,0,AV_PIX_FMT_NONE };

	if (this->VideoCodecCtx != NULL)
	{
		info.Width = this->VideoCodecCtx->width;
		info.Height = this->VideoCodecCtx->height;
		info.Format = this->VideoCodecCtx->pix_fmt;
	}

	return info;
}

struct EvoVideoInfo MediaDecoder::GetDesVideoInfo() const
{
	struct EvoVideoInfo info = DesInfo;

	if (this->Convert != NULL)
	{
		info = Convert->GetDesInfo();
	}
	else
	{
		return GetVideoInfo();
	}

	return info;
}

bool MediaDecoder::SetStartPos(int64_t pos)
{
	this->StartPos = pos;
	if (this->StartPos == 0) {
		return true;
	}

	int readFrame = -1;
	int streamIndex = -1;
	if (this->DecoderType == MediaDecoder::VIDEO) {
		streamIndex = this->VideoIndex;
	}
	else {
		streamIndex = this->AudioIndex;
	}


	while (true) {
		av_packet_unref(this->MediaPacket);
		readFrame = av_read_frame(this->FormatCtx, this->MediaPacket);
		if (readFrame >= 0) {
			if (this->MediaPacket->stream_index == streamIndex) {
				if (this->MediaPacket->duration == 0) continue;
				break;
			}
		}
	}

	if (this->MediaPacket != NULL) {
		int64_t pts = this->MediaPacket->pts;
		//获取帧持续时间
		int64_t duration = this->MediaPacket->duration;
		av_packet_unref(this->MediaPacket);
		//跳转至当前帧前两帧
		int64_t timeStamp = (pos - 1) * duration;
		av_seek_frame(this->FormatCtx, streamIndex, timeStamp, AVSEEK_FLAG_BACKWARD);

		do {
			readFrame = av_read_frame(this->FormatCtx, this->MediaPacket);
			if (readFrame >= 0) {
				if (this->MediaPacket->stream_index == streamIndex) {
					break;
				}
			}
		} while (true);

		if (this->MediaPacket != NULL) {
			int64_t frameCount = (this->MediaPacket->pts - pts) / duration;
			if (this->DecoderType == MediaDecoder::VIDEO) {
				this->VideoFrameCount = frameCount;
			}
			else {
				this->AudioFrameCount = frameCount;
			}
			av_packet_unref(this->MediaPacket);
		}
		else {
			return false;
		}

		av_seek_frame(this->FormatCtx, streamIndex, timeStamp, AVSEEK_FLAG_BACKWARD);
		return true;
	}
	else {
		return false;
	}

	return true;
}

bool MediaDecoder::SetTimePos(int64_t timePos)
{
	//清缓冲
	ClearAllQueue();

	std::unique_lock<std::mutex> lock(TimePosMutex);

	int64_t seek_target = timePos;
	int64_t timeStamp = seek_target * 1000;
	int ret = av_seek_frame(this->FormatCtx, -1, timeStamp, AVSEEK_FLAG_BACKWARD);

	/*int64_t timeStamp = timePos * 1000LL;
	int ret = avformat_seek_file(this->FormatCtx, -1, INT64_MIN, timeStamp, INT64_MAX, 0);*/
	LastVideoPTS = 0;
	if (ret >= 0) {
		ResetIsEnd();
		if (ADecoder != NULL) {
			ADecoder->Flush();
		}
		if (VDecoder != NULL)
		{
			VDecoder->Flush();
			VDecoder->SetKeepIFrame(true);
		}
		avformat_flush(this->FormatCtx);
		ClearAllQueue();
		ResetAllQueue();
		return true;
	}

	//重置缓冲
	ClearAllQueue();
	ResetAllQueue();
	return false;
}

//解一帧音频或视频
bool MediaDecoder::Decode()
{
	int readFrame = -1;
	int decoded = 0;
	bool ret = false;
	int audioCount = 0;
	int videoCount = 0;
	while (true) {
		std::unique_lock<std::mutex> lock(TimePosMutex);

		readFrame = av_read_frame(this->FormatCtx, this->MediaPacket);
		do {
			if (readFrame >= 0) {
				if (this->MediaPacket->stream_index == this->VideoIndex) {
					decoded =  this->DecodeVideo();
					videoCount++;
					break;
				}
				else if (this->MediaPacket->stream_index == this->AudioIndex) {
					decoded =  this->DecodeAudio();
					audioCount++;
					break;
				}
				else {
					decoded = 0;
					break;
				}
			}
			else if (readFrame == AVERROR_EOF) {
				this->IsEnd = true;
				decoded = -1;
				break;
			}
		} while (false);

		if (this->MediaPacket != NULL) {
			av_packet_unref(this->MediaPacket);
		}

		if (decoded == -1) {
			ret = false;
			break;
		}
		else if (decoded == 1) {
			ret = true;
			break;
		}
	}
	return ret;
}

int  MediaDecoder::DecodeVideo()
{
	VideoFrameCount++;

	//滤帧
	if (this->StartPos > 0)
	{
		if (this->DecoderType == MediaDecoder::VIDEO) {
			if(!(this->VideoFrameCount <= this->StartPos))
			{
				return 0;
			}
		}
		else if (this->DecoderType == MediaDecoder::AUDIO) {
			if (!(this->AudioFrameCount <= this->StartPos))
			{
				return 0;
			}
		}
	}

	if (VDecoder != NULL)
	{
		if (this->DecoderType == DECODER_TYPE::VIDEO)
		{
			return DecodeVideoPacket(MediaPacket);
		}
		else if (this->DecoderType == DECODER_TYPE::STREAN)
		{
			return DecodeVideoPacket(MediaPacket);
		}
	}

	return 0;
}

int  MediaDecoder::DecodeAudio()
{
	AudioFrameCount++;

	//滤帧
	if (this->StartPos > 0)
	{
		if (this->DecoderType == MediaDecoder::VIDEO) {
			if (!(this->VideoFrameCount <= this->StartPos))
			{
				return 0;
			}
		}
		else if (this->DecoderType == MediaDecoder::AUDIO) {
			if (!(this->AudioFrameCount <= this->StartPos))
			{
				return 0;
			}
		}
	}
	//获取音频源
	if (IsGetAudioSource && this->DecoderType != DECODER_TYPE::VIDEO)
	{
		bool ret = AudioDataSourceSet.Enqueue(MediaPacket);
		if (ret == true)
		{
			this->MediaPacket = (AVPacket *)av_packet_alloc();
			return 0;
		}
		else {
			return -1;
		}
	}

	if (ADecoder != NULL)
	{
		if(this->DecoderType == DECODER_TYPE::AUDIO)
		{
			return DecodeAudioPacket(MediaPacket);
		}
		else if (this->DecoderType == DECODER_TYPE::STREAN)
		{
			return DecodeAudioPacket(MediaPacket);
		}
	}

	return 0;
}

int MediaDecoder::SetConvert(const EvoVideoInfo &info)
{
	WriteLock lock(&ConvertMutex);

	const EvoVideoInfo src = GetVideoInfo();
	DesInfo = info;

	if(DesInfo.Format == AV_PIX_FMT_NONE)
	{
		DesInfo.Format = src.Format;
	}

	if (DesInfo.Width <= 0)
	{
		DesInfo.Width = src.Width;
	}

	if (DesInfo.Height <= 0)
	{
		DesInfo.Height = src.Height;
	}

	if (DesInfo.Format == src.Format
		&& DesInfo.Width == src.Width
		&& DesInfo.Height == src.Height)
	{
		if (VDecoder != NULL)
		{
			VDecoder->Attach(NULL);
		}

		if (Convert != NULL)
		{
			delete Convert;
			Convert = NULL;
		}
		return 1;
	}

	MediaConvert *newConvert = new MediaConvert();
	bool ret = newConvert->Initialize(src, DesInfo);
	if (ret == true)
	{
		MediaConvert *tmpConvert = Convert;
		Convert = newConvert;

		if (VDecoder != NULL)
		{
			VDecoder->Attach(Convert);
		}

		if (tmpConvert != NULL)
		{
			delete tmpConvert;
			tmpConvert = NULL;
		}
		return 0;
	}
	else
	{
		delete newConvert;
	}

	return -1;
}

void MediaDecoder::ClearAllQueue()
{
	VideoDataSet.Clear(true);

	AudioDataSet.Clear(true);

	AudioDataSourceSet.Clear(true);
}

void MediaDecoder::ResetAllQueue()
{
	VideoDataSet.Restart();

	AudioDataSet.Restart();

	AudioDataSourceSet.Restart();
}

void MediaDecoder::SetGetAudioSource(bool getAudioSource)
{
	IsGetAudioSource = getAudioSource;
}

EvoPacket *MediaDecoder::VideoDeQueue()
{
	return VideoDataSet.Dequeue(1);
}

EvoPacket *MediaDecoder::AudioDeQueue()
{
	return AudioDataSet.Dequeue(1);
}

AVPacket *MediaDecoder::AudioSourceDeQueue()
{
	return AudioDataSourceSet.Dequeue(1);
}

int MediaDecoder::DecodeAudioPacket(AVPacket *packet)
{
	if (ADecoder == NULL)
	{
		return 0;
	}

	EvoPacket *evoResult = NULL;
	int ret = ADecoder->DecodePacket(packet, &evoResult);
	if (evoResult != NULL)
	{
		ret = AudioDataSet.Enqueue(evoResult);
		if (ret == false)
		{
			delete evoResult;
			return -1;
		}
		return ret;
	}
	else
	{
		return ret;
	}
}

#include "MemManager/EvoAVFrame.h"
int MediaDecoder::DecodeVideoPacket(AVPacket *packet)
{
	if (VDecoder == NULL)
	{
		return 0;
	}

	ReadLock lock(&ConvertMutex);

	EvoPacket *evoResult = NULL;
	int ret = VDecoder->DecodePacket(packet, &evoResult);
	if (evoResult != NULL)
	{
		ret = VideoDataSet.Enqueue(evoResult);
		if (ret == false)
		{
			delete evoResult;
			LOGD("MediaDecoder::DecodeVideoPacket: VideoDataSet.Enqueue FALSE.\n");
			return -1;
		}
		if (evoResult->GetPacketType() == EvoPacket::TYPE_AVFRAME) {
			EvoAVFrame *pkt = (EvoAVFrame*)evoResult;
			int64_t pts = pkt->GetPTS();
			LastVideoPTS = pts;
		}
		else {
			LastVideoPTS = 0;
		}

		return ret;
	}
	else
	{
		if (ret != 0)
		{
			LOGD("MediaDecoder::DecodeVideoPacket: DecodePacket(%d).\n", ret);
		}
		return ret;
	}
}

int MediaDecoder::GetVideoDataSetCount()
{
	return VideoDataSet.Count();
}

int MediaDecoder::GetAudioDataSetCount()
{
	return AudioDataSet.Count();
}

bool MediaDecoder::CheckIsGetAudioSource()
{
	return IsGetAudioSource;
}

int64_t MediaDecoder::GetLastVideoPTS() {
	return LastVideoPTS;
}

void MediaDecoder::SetVideoDecoderName(const char * name) {
	VideoDecoderName = name;
}

AVCodec *MediaDecoder::GetBestVideoDecoder(AVCodecID id) {
	if (VideoDecoderName.length() == 0) return NULL;
	std::string name = VideoDecoderName;
	std::string decoder;
	while (true) {
		size_t pos = name.find(" ");
		if (pos != -1) {
			decoder = name.substr(0,pos);
			name = name.substr(pos + 1);
		}
		else {
			decoder = name;
			name = "";
		}
		if(decoder.length() > 0){
			AVCodec * codec = avcodec_find_decoder_by_name(decoder.c_str());
			if (codec != NULL && codec->id == id) return codec;
		}
		if (name.length() == 0) break;
	}
	return NULL;
}