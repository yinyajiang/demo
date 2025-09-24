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

AudioFileInfo FetchAudioInfo::fetchAudioInfo(std::filesystem::path in_fpath, FetchConfig config) {
  m_stoped.store(false);
  AudioFileInfo info;
  info.key = 0;
  info.samples.clear();
  info.consume_time_ms = 0;
  info.duration_seconds = 0;
  info.sample_format = "";
  info.channels = 0;
  info.sample_rate = 0;
  info.bpm = 0;
  info.key_string = "";

  auto start_time = std::chrono::high_resolution_clock::now();
  auto audio_decoder =
      std::make_shared<AudioDecoder>(WORKING_SAMPLE_RATE, 1, AV_SAMPLE_FMT_FLT);
  audio_decoder->open(in_fpath);

  info.channels = audio_decoder->channels();
  info.sample_rate = audio_decoder->sampleRate();
  info.duration_seconds = (int)audio_decoder->durationSecond();
  info.sample_format = av_get_sample_fmt_name(audio_decoder->sampleFormat());


  int tgt_sample_rate = audio_decoder->targetSampleRate();
  int tgt_channels = audio_decoder->targetChannels();
  AVSampleFormat tgt_format = audio_decoder->targetSampleFormat();
  int64_t tgt_frame_size = tgt_channels * av_get_bytes_per_sample(tgt_format);

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();

  auto source = std::make_shared<DecodeDataSource>(tgt_frame_size, decode_queue);
  std::shared_ptr<BPMFilter> bpm_filter;
  std::shared_ptr<KeyFilter> key_filter;
  if (config.fetch_bpm) {
    bpm_filter = std::make_shared<BPMFilter>(tgt_sample_rate, tgt_channels, tgt_format);
    source->addFilter(bpm_filter);
  }
  if (config.fetch_key) {
    key_filter = std::make_shared<KeyFilter>(tgt_sample_rate, tgt_channels, tgt_format);
    source->addFilter(key_filter);
  }

  source->consumeAll();
  if (bpm_filter) {
    info.bpm = bpm_filter->getBPM();
  }
  if (config.fetch_key) {
    info.key = key_filter->getKey();
    info.key_string = key_filter->keyToString(static_cast<KeyFinder::key_t>(info.key));
  }

  audio_decoder->close();
  decode_queue->stop();

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  info.consume_time_ms = duration.count();
  return info;
}
