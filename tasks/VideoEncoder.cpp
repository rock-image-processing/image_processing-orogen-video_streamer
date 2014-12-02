/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "VideoEncoder.hpp"
extern "C"
{
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libswscale/swscale.h"
#include "libavcodec/version.h"
}
using namespace video_streamer;

VideoEncoder::VideoEncoder(std::string const& name, TaskCore::TaskState initial_state)
    : VideoEncoderBase(name, initial_state)
{
}

VideoEncoder::VideoEncoder(std::string const& name, RTT::ExecutionEngine* engine, TaskCore::TaskState initial_state)
    : VideoEncoderBase(name, engine, initial_state)
{
}

VideoEncoder::~VideoEncoder()
{
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See VideoEncoder.hpp for more detailed
// documentation about them.

bool VideoEncoder::configureHook()
{
    if (! VideoEncoderBase::configureHook())
        return false;

#if LIBAVCODEC_VERSION_MAJOR < 56    
    CodecID codec_id = CODEC_ID_H264;
#else
    AVCodecID codec_id = AV_CODEC_ID_H264;
#endif
    /* register all the codecs */
    avcodec_register_all();
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
	std::cout << "Error, could not find codec" << std::endl;
	return false;
    }

    //alocate memory for pictures
    rgb_picture= avcodec_alloc_frame();
    yuv_picture= avcodec_alloc_frame();



    //set codec parameters
    c = avcodec_alloc_context3(codec);
    
#if LIBAVCODEC_VERSION_MAJOR < 56    
    if(codec_id == CODEC_ID_H264)
#else
    if(codec_id == AV_CODEC_ID_H264)
#endif
    {
        av_opt_set(c->priv_data, "preset", "slow", 0);
        if(av_opt_set(c->priv_data, "preset", "ultrafast", 1))
	    std::cout << "Error Setting opt ultrafast " << std::endl;
    }
    
    c->bit_rate = _bitrate.get();
    /* frames per second */
    c->time_base= (AVRational){1, _framerate.get()};
    c->gop_size = 0;
    c->max_b_frames=0;
    c->pix_fmt = PIX_FMT_YUV420P;
    c->thread_count = 1;

    outbuffer_size = 1024*1024;
    //1 megabyte as output buffer 
    outbuffer.data.reserve(outbuffer_size);
    outbuffer.data.resize(outbuffer_size, 0);
    
    initialized = false;
    last_height = 0;
    last_width = 0;
    frameCounter = 0;
    sws_context = NULL;

    return true;
}

void VideoEncoder::updateHook()
{
    RTT::extras::ReadOnlyPointer<base::samples::frame::Frame> picture;
    while(_raw_pictures.read(picture) == RTT::NewData)
    {
	if(picture->getFrameMode() != base::samples::frame::MODE_RGB)
	{
	    std::cout << "Error, at the moment we only support RGB images as input" << std::endl;
	    exception();
	    return;
	}
	
	//got new frame, increase framecounter
	frameCounter++;
	
	const uint16_t height = picture->getHeight();
	const uint16_t width = picture->getWidth();

	if(last_height != height || last_width != width)
	{
	    if(initialized)
	    {
		avcodec_close(c);
		av_free(rgb_picture->data[0]);
		av_free(yuv_picture->data[0]);
	    }
	    last_height = height;
	    last_width = width;
	
	    c->width =  width;
	    c->height = height;
	    
	    if (avcodec_open2(c, codec, NULL) < 0) {
		std::cout << "Error, could not open codec" << std::endl;
		exception();
		return;
	    }
	    
	    av_image_alloc(rgb_picture->data, rgb_picture->linesize,
			   c->width, c->height, PIX_FMT_RGB24, 1);

	    av_image_alloc(yuv_picture->data, yuv_picture->linesize,
			   c->width, c->height, c->pix_fmt, 1);

	    initialized = true;
	}

	//always copy the image.
	//the memory needs to be aligned, 
	//because sws_scale uses MMX or SSE
	memcpy(rgb_picture->data[0], picture->getImageConstPtr(), picture->getNumberOfBytes() );

	//convert picture from RGB24 to YUV420P
	//Note the image is copied by sws_scale
	sws_context = sws_getCachedContext(sws_context, width, height, PIX_FMT_RGB24, width, height, PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
	sws_scale(sws_context, rgb_picture->data, rgb_picture->linesize, 0,  height, yuv_picture->data, yuv_picture->linesize);

	//set correct frame number
	//not this is critical for the encoding.
	yuv_picture->pts = frameCounter;
	
	//encode yuv image to video stream
        encodeAndSendFrame(yuv_picture);
    }

    VideoEncoderBase::updateHook();
}

void VideoEncoder::encodeAndSendFrame(AVFrame *frame)
{
#if LIBAVCODEC_VERSION_MAJOR < 56    
        int out_size;
        out_size = avcodec_encode_video(c, outbuffer.data.data(), outbuffer_size, frame);
        if(out_size)
        {
            //crop vector for sending
            outbuffer.data.resize(out_size);
            _mpeg_stream.write(outbuffer);
            //resize to original, this should not allocate
            //memory, as reserve was called on the vector
            outbuffer.data.resize(outbuffer_size);          
        }
#else
        AVPacket avpkt;
        int got_output_pkg = 0;
        if(!avcodec_encode_video2(c, &avpkt, frame, &got_output_pkg))
        {
            if(got_output_pkg)
            {
                //crop vector for sending
                outbuffer.data.resize(avpkt.size);
                for(size_t i = 0; i < avpkt.size; i++)
                {
                    outbuffer.data[i] = avpkt.data[i];
                }
                _mpeg_stream.write(outbuffer);
                //resize to original, this should not allocate
                //memory, as reserve was called on the vector
                outbuffer.data.resize(outbuffer_size);
            }
            av_free_packet(&avpkt);
        }
        else
        {
            std::cout << "Error enconding video stream" << std::endl;
        }
#endif
}

void VideoEncoder::stopHook()
{
    //FIXME THIS KILLS ENCODING IN CASE OF RESTART

    //send out frames that are buffered by the video encoder
    encodeAndSendFrame(NULL);

    VideoEncoderBase::stopHook();
}

void VideoEncoder::cleanupHook()
{
    avcodec_close(c);
    av_free(c);
    av_free(rgb_picture->data[0]);
    av_free(yuv_picture->data[0]);
    av_free(rgb_picture);
    av_free(yuv_picture);
    av_free(sws_context);

    VideoEncoderBase::cleanupHook();
}

