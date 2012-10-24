/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "VideoDecoder.hpp"
extern "C"
{
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libswscale/swscale.h"
}
using namespace video_streamer;

VideoDecoder::VideoDecoder(std::string const& name, TaskCore::TaskState initial_state)
    : VideoDecoderBase(name, initial_state)
{
    frame = NULL;
    sws_context = NULL;
}

VideoDecoder::VideoDecoder(std::string const& name, RTT::ExecutionEngine* engine, TaskCore::TaskState initial_state)
    : VideoDecoderBase(name, engine, initial_state)
{
    frame = NULL;
    sws_context = NULL;
}

VideoDecoder::~VideoDecoder()
{
}



/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See VideoDecoder.hpp for more detailed
// documentation about them.

bool VideoDecoder::configureHook()
{
    if (! VideoDecoderBase::configureHook())
        return false;

    /* register all the codecs */
    avcodec_register_all();
    
    av_init_packet(&avpkt);

    /* find the mpeg1 video decoder */
    codec = avcodec_find_decoder(CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    yuv_picture = avcodec_alloc_frame();
    rgb_picture = avcodec_alloc_frame();
    
    c->width = 640;
    c->height = 480;
    
/*
    if(codec->capabilities&CODEC_CAP_TRUNCATED)
        c->flags|= CODEC_FLAG_TRUNCATED; */

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    AVDictionary *dic = NULL;
    if (avcodec_open2(c, codec, &dic) < 0) {
	std::cout << "Error, could not open codec" << std::endl;
	return false;
    }
    
    return true;
}

// bool VideoDecoder::startHook()
// {
//     if (! VideoDecoderBase::startHook())
//         return false;
//     return true;
// }

void VideoDecoder::WriteFrame()
{
    const uint16_t height = yuv_picture->height;
    const uint16_t width = yuv_picture->width;
    
    if(!frame)
    {
	frame = new base::samples::frame::Frame(width, height, 8, base::samples::frame::MODE_RGB);
	ro_ptr.reset(frame);

	av_image_alloc(rgb_picture->data, rgb_picture->linesize,
		yuv_picture->width, yuv_picture->height, PIX_FMT_RGB24, 1);
    }
    sws_context = sws_getCachedContext(sws_context, width, height, PIX_FMT_YUV420P, width, height, PIX_FMT_RGB24, SWS_POINT, 0, 0, 0);
    
    //switch buffer of av_picture to the frame memory
    uint8_t *tmp = rgb_picture->data[0];
    rgb_picture->data[0] = frame->getImagePtr();
    //convert and copy picture
    sws_scale(sws_context, yuv_picture->data, yuv_picture->linesize, 0,  yuv_picture->height, rgb_picture->data, rgb_picture->linesize);
    //restore original pointer
    rgb_picture->data[0] = tmp;

    _raw_pictures.write(ro_ptr);
}


void VideoDecoder::updateHook()
{
    VideoDecoderBase::updateHook();
    
    StreamData input;
    
    while(_mpeg_stream.read(input) == RTT::NewData)
    {

	std::cout << "Decoder: Got Data :" << input.data.size() << endl;
        /* NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
           and this is the only method to use them because you cannot
           know the compressed data size before analysing it.

           BUT some other codecs (msmpeg4, mpeg4) are inherently frame
           based, so you must call them with all the data for one
           frame exactly. You must also initialize 'width' and
           'height' before initializing them. */

        /* NOTE2: some codecs allow the raw parameters (frame size,
           sample rate) to be changed at any frame. We handle this, so
           you should also take care of it */

        /* here, we use a stream based decoder (mpeg1video), so we
           feed decoder and see if it could decode a frame */
	avpkt.size = input.data.size();
        avpkt.data = input.data.data();

	int got_picture, len = 0;
	while (avpkt.size > 0) {
            len = avcodec_decode_video2(c, yuv_picture, &got_picture, &avpkt);
            if (len < 0) {
		std::cout << "Error while decoding frame" << std::endl;
                exception();
            }
            if (got_picture) {
		WriteFrame();
            }
            avpkt.size -= len;
            avpkt.data += len;
        }
    }
}
// void VideoDecoder::errorHook()
// {
//     VideoDecoderBase::errorHook();
// }
void VideoDecoder::stopHook()
{
    //FIXME THIS KILLS ENCODING IN CASE OF RESTART
    
    /* some codecs, such as MPEG, transmit the I and P frame with a
    latency of one frame. You must do the following to have a
    chance to get the last frame of the video */
    avpkt.data = NULL;
    avpkt.size = 0;
    int len, got_picture;
    len = avcodec_decode_video2(c, yuv_picture, &got_picture, &avpkt);
    if (len < 0) {
	std::cout << "Error while decoding frame" << std::endl;
	exception();
    }
    if (got_picture) {
	WriteFrame();
    } 

    VideoDecoderBase::stopHook();
}
void VideoDecoder::cleanupHook()
{
    avcodec_close(c);
    av_free(c);
    av_free(rgb_picture->data[0]);
    av_free(yuv_picture->data[0]);
    av_free(rgb_picture);
    av_free(yuv_picture);
    av_free(sws_context);

    VideoDecoderBase::cleanupHook();
}

