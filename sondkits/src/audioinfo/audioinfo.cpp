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

void FetchAudioInfo::stop() { m_stoped.store(true); }

AudioInfo FetchAudioInfo::fetchAudioInfo(std::filesystem::path in_fpath) {
  m_stoped.store(false);
  AudioInfo info;
  info.key = 0;
  auto start_time = std::chrono::high_resolution_clock::now();

  auto audio_decoder =
      std::make_shared<AudioDecoder>(WORKING_SAMPLE_RATE, 1, AV_SAMPLE_FMT_FLT);
  audio_decoder->open(in_fpath);

#if USE_AUBIO_BPM
  // info.bpm = detectBPMUseAubio(audio_decoder);
#else
  // info.bpm = detectBPMUseSoundtouch(audio_decoder);
#endif
  info.key = demo2(audio_decoder);
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
  info.duration_seconds = (int)audio_decoder->duration();
  info.sample_format = av_get_sample_fmt_name(audio_decoder->sampleFormat());
  info.consume_time_ms = duration.count();

  audio_decoder->close();
  return info;
}

float FetchAudioInfo::detectBPMUseSoundtouch(
    std::shared_ptr<AudioDecoder> audio_decoder) {
  assert(audio_decoder->targetSampleFormat() == AV_SAMPLE_FMT_FLT);

  soundtouch::BPMDetect bpm(audio_decoder->targetChannels(),
                            audio_decoder->targetSampleRate());

  foreachDecoderData(audio_decoder, [&](uint8_t *data, int size) {
    auto num_samples =
        size / sizeof(soundtouch::SAMPLETYPE) / audio_decoder->targetChannels();
    bpm.inputSamples(reinterpret_cast<soundtouch::SAMPLETYPE *>(data),
                     num_samples);
    return !m_stoped.load();
  });

  if (m_stoped.load()) {
    return 0;
  }
  auto r = bpm.getBpm();
  return r + 0.5f;
}

float FetchAudioInfo::detectBPMUseAubio(
    std::shared_ptr<AudioDecoder> audio_decoder) {
  return demo(audio_decoder);

  assert(audio_decoder->targetChannels() == 1);
  assert(audio_decoder->targetSampleFormat() == AV_SAMPLE_FMT_FLT);

  int hop_size = 96;
  int buf_size = 512;
  auto tempo = new_aubio_tempo("default", buf_size, hop_size,
                               audio_decoder->targetSampleRate());

  if (!tempo) {
    return 0;
  }

  fvec_t *input_vec = new_fvec(hop_size);
  fvec_t *output_vec = new_fvec(2);

  // 检查向量是否创建成功
  if (!input_vec || !output_vec) {
    if (input_vec)
      del_fvec(input_vec);
    if (output_vec)
      del_fvec(output_vec);
    del_aubio_tempo(tempo);
    return 0;
  }

  int hop_byte_size = hop_size * sizeof(float);
  int batch_size = 1024;
  int batch_byte_size = hop_byte_size * batch_size;

  foreachDecoderData(
      audio_decoder,
      [&](uint8_t *data, int size) {
        if (!data || size <= 0) {
          return true;
        }

        for (int i = 0; i < size / hop_byte_size; i++) {
          fvec_zeros(input_vec);
          memcpy(input_vec->data, data + i * hop_byte_size, hop_byte_size);
          aubio_tempo_do(tempo, input_vec, output_vec);
        }

        if (size % hop_byte_size != 0) {
          fvec_zeros(input_vec);
          int offset = size / hop_byte_size * hop_byte_size;
          memcpy(input_vec->data, data + offset, size - offset);
          aubio_tempo_do(tempo, input_vec, output_vec);
        }

        return !m_stoped.load();
      },
      batch_byte_size, batch_byte_size);

  float bpm = 0;
  if (!m_stoped.load()) {
    bpm = aubio_tempo_get_bpm(tempo);
  }
  del_aubio_tempo(tempo);
  del_fvec(input_vec);
  del_fvec(output_vec);
  return bpm;
}

float FetchAudioInfo::demo(std::shared_ptr<AudioDecoder> audio_decoder) {
  int64_t frame_size =
      audio_decoder->targetChannels() *
      av_get_bytes_per_sample(audio_decoder->targetSampleFormat());

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();

  auto bpm_filter = std::make_shared<BPMFilter>(audio_decoder);
  DecodeDataSource source(bpm_filter, frame_size, decode_queue);
  source.consumeAll();
  return bpm_filter->getBPM();
}

int FetchAudioInfo::demo2(std::shared_ptr<AudioDecoder> audio_decoder) {
  int64_t frame_size =
      audio_decoder->targetChannels() *
      av_get_bytes_per_sample(audio_decoder->targetSampleFormat());

  std::shared_ptr<DecodeQueue> decode_queue =
      std::make_shared<DecodeQueue>(audio_decoder);
  decode_queue->start();

  auto key_filter = std::make_shared<KeyFilter>(audio_decoder);
  DecodeDataSource source(key_filter, frame_size, decode_queue);
  source.consumeAll();

  auto key = key_filter->getKey();

  return key;
}