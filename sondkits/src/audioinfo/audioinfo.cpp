#include "audioinfo.h"
#include "BPMDetect.h"
#include "audiodecoder.h"
#include "audioutils.h"
#include "decodedatasource.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <libavutil/samplefmt.h>
extern "C" {
#include "aubio.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}
#include "audioutils.h"
#include "base64/base64.hpp"
#include "bpmfilter.h"
#include "common.h"
#include "keyfilter.h"
#include "md5/md5.h"
#include "normsamplefilter.h"

FetchAudioInfo::FetchAudioInfo() : m_stoped(false) {}

FetchAudioInfo::~FetchAudioInfo() {}

void FetchAudioInfo::abort() { m_stoped.store(true); }

AudioFileInfo FetchAudioInfo::fetchAudioInfo(std::filesystem::path in_fpath,
                                             FetchConfig config) {
  m_stoped.store(false);
  AudioFileInfo info;
  info.key = 0;
  info.samples_points.clear();
  info.consume_time_ms = 0;
  info.duration_seconds = 0;
  info.sample_format = "";
  info.channels = 0;
  info.sample_rate = 0;
  info.bpm = 0;
  info.key_string = "";
  info.thumbnail = "";
  info.convert_to_wav_size = 0;
  info.convert_to_mp3_size = 0;

  auto start_time = std::chrono::high_resolution_clock::now();
  auto audio_decoder = std::make_shared<AudioDecoder>(WORKING_SAMPLE_RATE(), 1,
                                                      AV_SAMPLE_FMT_FLT);
  audio_decoder->open(in_fpath);

  info.channels = audio_decoder->channels();
  info.sample_rate = audio_decoder->sampleRate();
  info.duration_seconds = (int)audio_decoder->durationSecond();
  info.sample_format = av_get_sample_fmt_name(audio_decoder->sampleFormat());
  info.convert_to_wav_size = calculateConvertToWavSize(audio_decoder);
  info.convert_to_mp3_size = calculateConvertToMp3Size(audio_decoder);
  info.thumbnail = fetchThumbnail(audio_decoder, config.cache_dir);

  int tgt_sample_rate = audio_decoder->targetSampleRate();
  int tgt_channels = audio_decoder->targetChannels();
  AVSampleFormat tgt_format = audio_decoder->targetSampleFormat();
  int64_t tgt_frame_size = tgt_channels * av_get_bytes_per_sample(tgt_format);

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();

  auto source =
      std::make_shared<DecodeDataSource>(tgt_frame_size, decode_queue);
  std::shared_ptr<BPMFilter> bpm_filter;
  std::shared_ptr<KeyFilter> key_filter;
  std::shared_ptr<NormSampleFilter> norm_sample_filter;
  if (config.fetch_bpm) {
    bpm_filter =
        std::make_shared<BPMFilter>(tgt_sample_rate, tgt_channels, tgt_format);
    source->addFilter(bpm_filter);
  }
  if (config.fetch_key) {
    key_filter =
        std::make_shared<KeyFilter>(tgt_sample_rate, tgt_channels, tgt_format);
    source->addFilter(key_filter);
  }
  if (config.fetch_point_num > 0) {
    norm_sample_filter = std::make_shared<NormSampleFilter>(
        tgt_sample_rate, tgt_channels, tgt_format,
        audio_decoder->durationSecond(), config.fetch_point_num);
    source->addFilter(norm_sample_filter);
  }

  source->consumeAll();
  if (bpm_filter) {
    info.bpm = bpm_filter->getBPM();
  }
  if (key_filter) {
    info.key = key_filter->getKey();
    info.key_string =
        key_filter->keyToString(static_cast<KeyFinder::key_t>(info.key));
  }
  if (norm_sample_filter) {
    info.samples_points = norm_sample_filter->getSamplePoints();
  }

  audio_decoder->close();
  decode_queue->stop();

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  info.consume_time_ms = duration.count();
  return info;
}

int64_t FetchAudioInfo::calculateConvertToWavSize(
    std::shared_ptr<AudioDecoder> audio_decoder) {
  int64_t sample_rate = audio_decoder->targetSampleRate();
  int64_t channels = 2;
  int64_t sample_format = audio_decoder->sampleFormat();
  int64_t duration = audio_decoder->durationSecond();
  int64_t size =
      sample_rate * channels *
      av_get_bytes_per_sample(static_cast<AVSampleFormat>(sample_format)) *
      duration;
  return size;
}

int64_t FetchAudioInfo::calculateConvertToMp3Size(
    std::shared_ptr<AudioDecoder> audio_decoder) {
  int64_t duration = audio_decoder->durationSecond();
  int64_t size = ENCODER_MP3_BIT_RATE * duration / 8;
  return size;
}

std::filesystem::path
FetchAudioInfo::fetchThumbnail(std::shared_ptr<AudioDecoder> audio_decoder,
                               std::filesystem::path thumbnail_dir) {
  std::filesystem::path src_path = audio_decoder->inFpath();
  std::string name = md5(src_path.string());
  std::filesystem::path thumbnail_path = thumbnail_dir / name;
  if (fetchThumbnailFromMetadata(audio_decoder, thumbnail_path)) {
    return thumbnail_path;
  }
  if (fetchThumbnailFromAddStream(audio_decoder, thumbnail_path)) {
    return thumbnail_path;
  }
  return "";
}

bool FetchAudioInfo::fetchThumbnailFromMetadata(
    std::shared_ptr<AudioDecoder> audio_decoder,
    std::filesystem::path thumbnail) {
  AVFormatContext *fmt_ctx = audio_decoder->fmtCtx();
  if (!fmt_ctx) {
    return false;
  }
  std::string cover_keys[] = {
      "cover",   "coverart", "albumart",
      "picture", "APIC:",    "METADATA_BLOCK_PICTURE",
  };
  for (auto &cover_key : cover_keys) {
    auto cover_entry =
        av_dict_get(fmt_ctx->metadata, cover_key.c_str(), nullptr, 0);
    if (!cover_entry || !cover_entry->value) {
      continue;
    }
    std::string data = cover_entry->value;
    if (!hasPrefix(data, "data:image/")) {
      continue;
    }
    // data:image/gif;base64,
    // data:image/jpeg;base64,
    // data:image/png;base64,
    std::string keyw = "base64,";
    auto i = data.find(keyw);
    if (i == std::string::npos) {
      continue;
    }
    data = data.substr(i + keyw.size());
    auto data_bytes = base64::decode_into<std::vector<char>>(data);
    if (data_bytes.empty()) {
      continue;
    }
    if (!std::filesystem::exists(thumbnail.parent_path())) {
      std::filesystem::create_directories(thumbnail.parent_path());
    }
    std::ofstream out_file(thumbnail, std::ios::binary);
    if (out_file.is_open()) {
      out_file.write(reinterpret_cast<const char *>(data_bytes.data()),
                     data_bytes.size());
      out_file.close();
      return true;
    }
  }
  return false;
}

bool FetchAudioInfo::fetchThumbnailFromAddStream(
    std::shared_ptr<AudioDecoder> audio_decoder,
    std::filesystem::path thumbnail) {
  AVFormatContext *fmt_ctx = audio_decoder->fmtCtx();
  if (!fmt_ctx) {
    return false;
  }
  int cover_stream_index = -1;
  for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
    AVStream *stream = fmt_ctx->streams[i];
    if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
      cover_stream_index = i;
      break;
    }
  }
  if (cover_stream_index == -1) {
    return false;
  }

  AVStream *cover_stream = fmt_ctx->streams[cover_stream_index];
  AVPacket &cover_pkt = cover_stream->attached_pic;
  if (cover_pkt.data && cover_pkt.size > 0) {
    if (!std::filesystem::exists(thumbnail.parent_path())) {
      std::filesystem::create_directories(thumbnail.parent_path());
    }
    std::ofstream out_file(thumbnail, std::ios::binary);
    if (out_file.is_open()) {
      out_file.write(reinterpret_cast<const char *>(cover_pkt.data),
                     cover_pkt.size);
      out_file.close();
      return true;
    }
  }
  return false;
}
