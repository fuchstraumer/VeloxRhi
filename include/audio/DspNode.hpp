#pragma once
#ifndef VELOX_AUDIO_DSP_NODE_HPP
#define VELOX_AUDIO_DSP_NODE_HPP
#include <coroutine>
#include <cstddef>
#include "CoroutineAllocator.hpp"

namespace velox
{
    inline static CoroutinePool<4096, 128, false> g_AudioCoroutineAllocator;
}

#endif // !VELOX_AUDIO_DSP_NODE_HPP
