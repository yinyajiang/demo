#include "audioinfo.h"
#include "BPMDetect.h"
#include "audiodecoder.h"
#include "audioutils.h"
#include "decodedatasource.h"
#include <chrono>
#include <iostream>
extern "C" {
#include "aubio.h"
}
#include "audioutils.h"
#include "bpmfilter.h"
#include "common.h"
#include "keyfilter.h"

FetchAudioInfo::FetchAudioInfo() : m_stoped(false) {}

FetchAudioInfo::~FetchAudioInfo() {}

void FetchAudioInfo::abort() { m_stoped.store(true); }

AudioFileInfo FetchAudioInfo::fetchAudioInfo(std::filesystem::path in_fpath) {
  m_stoped.store(false);
  AudioFileInfo info;
  info.key = 0;
  auto start_time = std::chrono::high_resolution_clock::now();

  auto audio_decoder =
      std::make_shared<AudioDecoder>(WORKING_SAMPLE_RATE, 1, AV_SAMPLE_FMT_FLT);
  audio_decoder->open(in_fpath);

  info.bpm = detectBPM(audio_decoder);
  info.key = detectKey(audio_decoder);
  info.key_string =
      KeyFilter::keyToString(static_cast<KeyFinder::key_t>(info.key));

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
#if PRINT_READ_CONSUME_TIME
  std::cout << "### detct bpm duration: " << duration.count() << "ms";
#endif

  info.channels = audio_decoder->channels();
  info.sample_rate = audio_decoder->sampleRate();
  info.duration_seconds = (int)audio_decoder->durationSecond();
  info.sample_format = av_get_sample_fmt_name(audio_decoder->sampleFormat());
  info.consume_time_ms = duration.count();

  audio_decoder->close();
  return info;
}

float FetchAudioInfo::detectBPM(std::shared_ptr<AudioDecoder> audio_decoder) {
  int64_t frame_size =
      audio_decoder->targetChannels() *
      av_get_bytes_per_sample(audio_decoder->targetSampleFormat());

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();

  auto bpm_filter = std::make_shared<BPMFilter>(audio_decoder);
  DecodeDataSource source(frame_size, decode_queue);
  source.addFilter(bpm_filter);
  source.consumeAll();
  decode_queue->stop();
  return bpm_filter->getBPM();
}

int FetchAudioInfo::detectKey(std::shared_ptr<AudioDecoder> audio_decoder) {
  int64_t frame_size =
      audio_decoder->targetChannels() *
      av_get_bytes_per_sample(audio_decoder->targetSampleFormat());

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();

  auto key_filter = std::make_shared<KeyFilter>(audio_decoder);
  DecodeDataSource source(frame_size, decode_queue);
  source.addFilter(key_filter);
  source.consumeAll();

  auto key = key_filter->getKey();

  return key;
}