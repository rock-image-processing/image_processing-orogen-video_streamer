#ifndef __VIDEO_STREAMER_TYPES_HPP__
#define __VIDEO_STREAMER_TYPES_HPP__
#include <vector>
#include <stdint.h>

namespace video_streamer
{

struct StreamData
{
    //TODO add codec widht height etc
    std::vector<uint8_t> data;
};

}

#endif