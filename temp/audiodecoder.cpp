#include "audiodecoder.h"
#include <QtCore/QDebug>

AudioDecoder::AudioDecoder(QObject *parent)
    : QObject(parent)
    , m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_swrContext(nullptr)
    , m_packet(nullptr)
    , m_frame(nullptr)
    , m_audioStreamIndex(-1)
    , m_sampleRate(0)
    , m_channels(0)
    , m_isAtEnd(false)
    , m_totalDuration(0.0)
    , m_currentPosition(0.0)
    , m_totalSamples(0)
    , m_processedSamples(0)
    , m_resampledData(nullptr)
    , m_resampledDataSize(0)
{
}

AudioDecoder::~AudioDecoder()
{
    cleanup();
}

bool AudioDecoder::openFile(const QString &filename)
{
    cleanup();
    
    // 打开输入文件
    if (avformat_open_input(&m_formatContext, filename.toUtf8().constData(), nullptr, nullptr) < 0) {
        emit error("无法打开音频文件");
        return false;
    }
    
    // 查找流信息
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        emit error("无法找到流信息");
        cleanup();
        return false;
    }
    
    // 查找音频流
    m_audioStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (m_audioStreamIndex < 0) {
        emit error("文件中没有找到音频流");
        cleanup();
        return false;
    }
    
    AVStream *audioStream = m_formatContext->streams[m_audioStreamIndex];
    
    // 查找解码器
    const AVCodec *codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!codec) {
        emit error("不支持的音频编码格式");
        cleanup();
        return false;
    }
    
    // 分配解码器上下文
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        emit error("无法分配解码器上下文");
        cleanup();
        return false;
    }
    
    // 复制流参数到解码器上下文
    if (avcodec_parameters_to_context(m_codecContext, audioStream->codecpar) < 0) {
        emit error("无法复制解码器参数");
        cleanup();
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        emit error("无法打开解码器");
        cleanup();
        return false;
    }
    
    // 获取音频参数
    m_sampleRate = m_codecContext->sample_rate;
    m_channels = m_codecContext->ch_layout.nb_channels;
    
    // 计算总时长
    if (m_formatContext->duration != AV_NOPTS_VALUE) {
        m_totalDuration = (double)m_formatContext->duration / AV_TIME_BASE;
    } else if (audioStream->duration != AV_NOPTS_VALUE) {
        m_totalDuration = (double)audioStream->duration * av_q2d(audioStream->time_base);
    } else {
        m_totalDuration = 0.0;
    }
    
    // 计算总采样数
    m_totalSamples = (int64_t)(m_totalDuration * m_sampleRate);
    m_processedSamples = 0;
    m_currentPosition = 0.0;
    
    qDebug() << "音频信息:" 
             << "采样率:" << m_sampleRate 
             << "声道数:" << m_channels
             << "格式:" << av_get_sample_fmt_name(m_codecContext->sample_fmt)
             << "总时长:" << m_totalDuration << "秒";
    
    // 初始化重采样器
    if (!initializeResampler()) {
        cleanup();
        return false;
    }
    
    // 分配数据包和帧
    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    
    if (!m_packet || !m_frame) {
        emit error("无法分配内存");
        cleanup();
        return false;
    }
    
    m_isAtEnd = false;
    return true;
}

void AudioDecoder::close()
{
    cleanup();
}

bool AudioDecoder::initializeResampler()
{
    // 创建重采样上下文
    m_swrContext = swr_alloc();
    if (!m_swrContext) {
        emit error("无法创建重采样上下文");
        return false;
    }
    
    // 设置输入参数
    av_opt_set_chlayout(m_swrContext, "in_chlayout", &m_codecContext->ch_layout, 0);
    av_opt_set_int(m_swrContext, "in_sample_rate", m_codecContext->sample_rate, 0);
    av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", m_codecContext->sample_fmt, 0);
    
    // 设置输出参数 (16位PCM)
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, m_channels);
    av_opt_set_chlayout(m_swrContext, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(m_swrContext, "out_sample_rate", m_sampleRate, 0);
    av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    
    // 初始化重采样上下文
    if (swr_init(m_swrContext) < 0) {
        emit error("无法初始化重采样器");
        return false;
    }
    
    return true;
}

QByteArray AudioDecoder::decodeNextFrame()
{
    if (!m_formatContext || !m_codecContext || m_isAtEnd) {
        return QByteArray();
    }
    
    QByteArray pcmData;
    
    while (true) {
        // 读取数据包
        int ret = av_read_frame(m_formatContext, m_packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                m_isAtEnd = true;
                // 刷新解码器
                avcodec_send_packet(m_codecContext, nullptr);
                while (avcodec_receive_frame(m_codecContext, m_frame) == 0) {
                    // 处理最后的帧
                    QByteArray frameData = processFrame();
                    if (!frameData.isEmpty()) {
                        pcmData.append(frameData);
                    }
                }
            }
            break;
        }
        
        // 只处理音频流的数据包
        if (m_packet->stream_index != m_audioStreamIndex) {
            av_packet_unref(m_packet);
            continue;
        }
        
        // 发送数据包到解码器
        ret = avcodec_send_packet(m_codecContext, m_packet);
        av_packet_unref(m_packet);
        
        if (ret < 0) {
            qDebug() << "发送数据包到解码器失败";
            continue;
        }
        
        // 接收解码后的帧
        while (avcodec_receive_frame(m_codecContext, m_frame) == 0) {
            QByteArray frameData = processFrame();
            if (!frameData.isEmpty()) {
                pcmData.append(frameData);
            }
        }
        
        // 如果有数据就返回，不等待更多帧
        if (!pcmData.isEmpty()) {
            return pcmData;
        }
    }
    
    return pcmData;
}

QByteArray AudioDecoder::processFrame()
{
    if (!m_frame->data[0] || !m_swrContext) {
        return QByteArray();
    }
    
    // 计算输出采样数
    int out_samples = swr_get_out_samples(m_swrContext, m_frame->nb_samples);
    if (out_samples <= 0) {
        return QByteArray();
    }
    
    // 重新分配缓冲区以匹配所需大小
    if (m_resampledData) {
        av_freep(&m_resampledData[0]);
        av_freep(&m_resampledData);
        m_resampledData = nullptr;
    }
    
    // 分配输出缓冲区
    if (av_samples_alloc_array_and_samples(&m_resampledData, &m_resampledDataSize,
                                           m_channels, out_samples, AV_SAMPLE_FMT_S16, 0) < 0) {
        qDebug() << "无法分配重采样缓冲区";
        return QByteArray();
    }
    
    // 重采样
    int converted_samples = swr_convert(m_swrContext, m_resampledData, out_samples,
                                        (const uint8_t**)m_frame->data, m_frame->nb_samples);
    
    if (converted_samples <= 0) {
        qDebug() << "重采样失败，转换样本数:" << converted_samples;
        return QByteArray();
    }
    
    // 计算输出数据大小
    int data_size = converted_samples * m_channels * sizeof(int16_t);
    
    // 不在这里更新播放进度，因为这是解码进度而非播放进度
    // 播放进度应该由播放器根据实际播放的数据来计算
    
    // 复制数据到QByteArray
    QByteArray pcmData(reinterpret_cast<const char*>(m_resampledData[0]), data_size);
    
    return pcmData;
}

void AudioDecoder::seekToStart()
{
    if (m_formatContext) {
        av_seek_frame(m_formatContext, m_audioStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(m_codecContext);
        m_isAtEnd = false;
        m_processedSamples = 0;
        m_currentPosition = 0.0;
    }
}

bool AudioDecoder::seekToPosition(double seconds)
{
    if (!m_formatContext || seconds < 0 || seconds > m_totalDuration) {
        return false;
    }
    
    // 计算目标时间戳
    int64_t timestamp = (int64_t)(seconds * AV_TIME_BASE);
    
    // 执行定位
    if (av_seek_frame(m_formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        qDebug() << "定位失败";
        return false;
    }
    
    // 清空解码器缓冲区
    avcodec_flush_buffers(m_codecContext);
    
    // 更新当前位置
    m_currentPosition = seconds;
    m_processedSamples = (int64_t)(seconds * m_sampleRate);
    m_isAtEnd = false;
    
    qDebug() << "定位到:" << seconds << "秒";
    return true;
}

void AudioDecoder::cleanup()
{
    if (m_resampledData) {
        av_freep(&m_resampledData[0]);
        av_freep(&m_resampledData);
        m_resampledData = nullptr;
    }
    
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    
    if (m_swrContext) {
        swr_free(&m_swrContext);
    }
    
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
    }
    
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }
    
    m_audioStreamIndex = -1;
    m_sampleRate = 0;
    m_channels = 0;
    m_isAtEnd = false;
    m_totalDuration = 0.0;
    m_currentPosition = 0.0;
    m_totalSamples = 0;
    m_processedSamples = 0;
    m_resampledDataSize = 0;
}
