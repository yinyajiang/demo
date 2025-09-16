#include "audioplayer.h"
#include <QtCore/QDebug>
#include <QtMultimedia/QMediaDevices>
#include <cstring>

// PcmDataSource implementation
PcmDataSource::PcmDataSource(QObject *parent)
    : QIODevice(parent)
    , m_pos(0)
    , m_balance(0.0)
    , m_channels(2)
    , m_sampleRate(44100)
{
    open(QIODevice::ReadOnly);
}

void PcmDataSource::addData(const QByteArray &data)
{
    if (!data.isEmpty()) {
        m_buffer.append(data);
    }
}

void PcmDataSource::clear()
{
    m_buffer.clear();
    m_pos = 0;
    seek(0); // 重置位置
}

bool PcmDataSource::atEnd() const
{
    return m_pos >= m_buffer.size();
}

qint64 PcmDataSource::bytesAvailable() const
{
    return m_buffer.size() - m_pos + QIODevice::bytesAvailable();
}

qint64 PcmDataSource::readData(char *data, qint64 maxlen)
{
    if (m_pos >= m_buffer.size()) {
        return 0;
    }
    
    qint64 readBytes = qMin(maxlen, (qint64)(m_buffer.size() - m_pos));
    if (readBytes > 0) {
        // 如果是立体声且有平衡调整，需要处理数据
        if (m_channels == 2 && qAbs(m_balance) > 0.001) {
            // 确保读取的字节数是完整的立体声样本（4字节对齐）
            readBytes = (readBytes / 4) * 4;
            
            // 复制数据并应用平衡调整
            memcpy(data, m_buffer.constData() + m_pos, readBytes);
            applyBalance(reinterpret_cast<int16_t*>(data), readBytes / 2); // 除以2因为是16位样本
        } else {
            // 直接复制数据
            memcpy(data, m_buffer.constData() + m_pos, readBytes);
        }
        m_pos += readBytes;
    }
    
    return readBytes;
}

qint64 PcmDataSource::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)
    return -1; // 只读设备
}

void PcmDataSource::setBalance(qreal balance)
{
    m_balance = qBound(-1.0, balance, 1.0);
}

void PcmDataSource::setChannelInfo(int channels, int sampleRate)
{
    m_channels = channels;
    m_sampleRate = sampleRate;
}

void PcmDataSource::applyBalance(int16_t *samples, int sampleCount)
{
    if (m_channels != 2) return; // 只处理立体声
    
    // 计算左右声道的增益
    qreal leftGain = 1.0;
    qreal rightGain = 1.0;
    
    if (m_balance < 0) {
        // 偏向左声道，减小右声道音量
        rightGain = 1.0 + m_balance; // balance是负数，所以相当于减小
    } else if (m_balance > 0) {
        // 偏向右声道，减小左声道音量
        leftGain = 1.0 - m_balance;
    }
    
    // 应用增益到交错的立体声样本
    for (int i = 0; i < sampleCount; i += 2) {
        // 左声道 (偶数索引)
        samples[i] = (int16_t)(samples[i] * leftGain);
        // 右声道 (奇数索引)
        samples[i + 1] = (int16_t)(samples[i + 1] * rightGain);
    }
}

// AudioPlayer implementation
AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_audioSink(nullptr)
    , m_dataSource(nullptr)
    , m_bufferTimer(nullptr)
    , m_initialized(false)
    , m_volume(1.0)
    , m_balance(0.0)
    , m_requestCounter(0)
    , m_pausedDuration(0.0)
    , m_isPlayingForProgress(false)
    , m_trackingSampleRate(0)
    , m_trackingChannels(0)
{
    m_audioDevice = QMediaDevices::defaultAudioOutput();
    m_dataSource = new PcmDataSource(this);
    
    m_bufferTimer = new QTimer(this);
    m_bufferTimer->setInterval(50); // 每50ms检查一次缓冲区，更频繁
    connect(m_bufferTimer, &QTimer::timeout, this, &AudioPlayer::checkBufferLevel);
}

AudioPlayer::~AudioPlayer()
{
    cleanup();
}

bool AudioPlayer::initialize(int sampleRate, int channels, int bitsPerSample)
{
    cleanup();
    
    // 设置音频格式 (Qt6 API)
    m_format.setSampleRate(sampleRate);
    m_format.setChannelCount(channels);
    
    // Qt6中使用setSampleFormat代替setSampleSize/setCodec/setByteOrder/setSampleType
    if (bitsPerSample == 16) {
        m_format.setSampleFormat(QAudioFormat::Int16);
    } else if (bitsPerSample == 32) {
        m_format.setSampleFormat(QAudioFormat::Int32);
    } else {
        // 默认使用16位
        m_format.setSampleFormat(QAudioFormat::Int16);
        qDebug() << "不支持的位深度" << bitsPerSample << "，使用16位";
    }
    
    qDebug() << "音频格式:" 
             << "采样率:" << m_format.sampleRate()
             << "声道数:" << m_format.channelCount()
             << "位深度:" << bitsPerSample;
    
    // Qt6中QAudioDevice没有isFormatSupported方法，
    // 格式兼容性由QAudioSink在创建时自动处理
    qDebug() << "使用音频格式:" 
             << "采样率:" << m_format.sampleRate()
             << "声道数:" << m_format.channelCount();
    
    // 创建音频输出 (Qt6 QAudioSink API)
    m_audioSink = new QAudioSink(m_audioDevice, m_format, this);
    m_audioSink->setBufferSize(0);
    
    // Qt6中QAudioSink没有setBufferSize方法，缓冲由系统自动管理
    // 在Qt6中，缓冲区大小由音频系统自动优化
    
    // 连接状态变化信号
    connect(m_audioSink, &QAudioSink::stateChanged, this, &AudioPlayer::onStateChanged);
    
    // 设置音量
    m_audioSink->setVolume(m_volume);
    
    m_initialized = true;
    return true;
}

void AudioPlayer::play()
{
    if (!m_initialized || !m_audioSink) {
        qDebug() << "音频播放器未初始化";
        return;
    }
    
    if (m_audioSink->state() == QAudio::StoppedState) {
        m_dataSource->seek(0);
        m_audioSink->start(m_dataSource);
        m_bufferTimer->start();
        
        // 开始播放进度跟踪
        m_playStartTime = QTime::currentTime();
        m_isPlayingForProgress = true;
        qDebug() << "开始播放";
    } else if (m_audioSink->state() == QAudio::SuspendedState) {
        m_audioSink->resume();
        m_bufferTimer->start();
        
        // 恢复播放进度跟踪
        m_playStartTime = QTime::currentTime();
        m_isPlayingForProgress = true;
        qDebug() << "恢复播放";
    }
}

void AudioPlayer::pause()
{
    if (m_audioSink && m_audioSink->state() == QAudio::ActiveState) {
        // 计算并累加已播放时长
        if (m_isPlayingForProgress) {
            double elapsed = m_playStartTime.msecsTo(QTime::currentTime()) / 1000.0;
            m_pausedDuration += elapsed;
            m_isPlayingForProgress = false;
        }
        
        m_audioSink->suspend();
        m_bufferTimer->stop();
        qDebug() << "暂停播放";
    }
}

void AudioPlayer::stop()
{
    if (m_audioSink) {
        // 停止播放进度跟踪
        m_isPlayingForProgress = false;
        
        m_audioSink->stop();
        m_bufferTimer->stop();
        m_dataSource->clear();
        qDebug() << "停止播放";
    }
}

void AudioPlayer::addPcmData(const QByteArray &data)
{
    if (m_dataSource) {
        m_dataSource->addData(data);
    }
}

void AudioPlayer::clearBuffer()
{
    if (m_dataSource) {
        m_dataSource->clear();
    }
}

bool AudioPlayer::isPlaying() const
{
    return m_audioSink && m_audioSink->state() == QAudio::ActiveState;
}

void AudioPlayer::setVolume(qreal volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    if (m_audioSink) {
        m_audioSink->setVolume(m_volume);
    }
}

qreal AudioPlayer::volume() const
{
    return m_volume;
}

void AudioPlayer::onStateChanged()
{
    if (m_audioSink) {
        QAudio::State state = m_audioSink->state();
        qDebug() << "音频状态变化:" << state;
        
        // 检查是否有错误
        if (m_audioSink->error() != QAudio::NoError) {
            qDebug() << "音频播放错误:" << m_audioSink->error();
            m_bufferTimer->stop();
        }
        
        // 如果停止播放，停止缓冲区检查
        if (state == QAudio::StoppedState) {
            m_bufferTimer->stop();
        }
        
        emit stateChanged();
    }
}

void AudioPlayer::checkBufferLevel()
{
    if (!m_dataSource || !m_audioSink || !m_initialized) {
        return;
    }
    
    // 如果缓冲区数据少于5秒，立即请求更多数据
    qint64 available = m_dataSource->bytesAvailable();
    // 计算每个样本的字节数
    int bytesPerSample = 2; // 默认16位
    if (m_format.sampleFormat() == QAudioFormat::Int32 || m_format.sampleFormat() == QAudioFormat::Float) {
        bytesPerSample = 4;
    }
    qint64 threshold = m_format.sampleRate() * m_format.channelCount() * bytesPerSample * 5; // 5秒的数据
    
    if (available < threshold) {
        // 立即请求数据，不使用防抖
        emit needMoreData();
    }
    
    // 如果状态变为Idle，说明数据不足
    if (m_audioSink->state() == QAudio::IdleState) {
        qDebug() << "音频状态变为Idle，缓冲区可用数据:" << available << "字节";
        emit needMoreData();
    }
}

void AudioPlayer::setAudioFormat(int sampleRate, int channels)
{
    m_trackingSampleRate = sampleRate;
    m_trackingChannels = channels;
    
    // 同时设置数据源的声道信息
    if (m_dataSource) {
        m_dataSource->setChannelInfo(channels, sampleRate);
    }
}

double AudioPlayer::getPlayedDuration() const
{
    double totalDuration = m_pausedDuration;
    
    if (m_isPlayingForProgress) {
        double currentElapsed = m_playStartTime.msecsTo(QTime::currentTime()) / 1000.0;
        totalDuration += currentElapsed;
    }
    
    return totalDuration;
}

void AudioPlayer::resetPlayedDuration()
{
    m_pausedDuration = 0.0;
    m_isPlayingForProgress = false;
    m_playStartTime = QTime::currentTime();
}

void AudioPlayer::setPlayedDuration(double seconds)
{
    m_pausedDuration = seconds;
    m_isPlayingForProgress = false;
    m_playStartTime = QTime::currentTime();
}

void AudioPlayer::setBalance(qreal balance)
{
    m_balance = qBound(-1.0, balance, 1.0);
    if (m_dataSource) {
        m_dataSource->setBalance(m_balance);
    }
}

qreal AudioPlayer::balance() const
{
    return m_balance;
}

void AudioPlayer::cleanup()
{
    if (m_bufferTimer) {
        m_bufferTimer->stop();
    }
    
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    
    if (m_dataSource) {
        m_dataSource->clear();
    }
    
    m_initialized = false;
    m_pausedDuration = 0.0;
    m_isPlayingForProgress = false;
}
