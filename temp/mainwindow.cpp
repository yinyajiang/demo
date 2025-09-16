#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_decoder(nullptr)
    , m_player(nullptr)
    , m_updateTimer(nullptr)
    , m_isPlaying(false)
    , m_isSliderPressed(false)
    , m_totalDuration(0.0)
{
    setupUI();
    setupConnections();
    
    // 创建音频组件
    m_decoder = new AudioDecoder(this);
    m_player = new AudioPlayer(this);
    
    // 创建更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(500); // 每500ms更新一次，减少频率
    
    // 连接信号
    connect(m_decoder, &AudioDecoder::error, this, &MainWindow::onDecoderError);
    connect(m_player, &AudioPlayer::stateChanged, this, &MainWindow::onPlayerStateChanged);
    connect(m_player, &AudioPlayer::needMoreData, this, &MainWindow::onPlayerNeedMoreData);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updatePlayback);
    
    updateUI();
    
    setWindowTitle("FFmpeg音频播放器");
    setMinimumSize(500, 400);
    resize(600, 450);
}

MainWindow::~MainWindow()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_player) {
        m_player->stop();
    }
    if (m_decoder) {
        m_decoder->close();
    }
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // 文件选择组
    m_fileGroup = new QGroupBox("文件选择");
    m_fileLayout = new QHBoxLayout(m_fileGroup);
    
    m_openButton = new QPushButton("打开音频文件");
    m_fileLabel = new QLabel("未选择文件");
    m_fileLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    
    m_fileLayout->addWidget(m_openButton);
    m_fileLayout->addWidget(m_fileLabel, 1);
    
    // 播放控制组
    m_controlGroup = new QGroupBox("播放控制");
    m_controlLayout = new QHBoxLayout(m_controlGroup);
    
    m_playPauseButton = new QPushButton("播放");
    m_stopButton = new QPushButton("停止");
    
    m_playPauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    
    m_controlLayout->addWidget(m_playPauseButton);
    m_controlLayout->addWidget(m_stopButton);
    m_controlLayout->addStretch();
    
    // 进度控制组
    m_progressGroup = new QGroupBox("播放进度");
    m_progressMainLayout = new QVBoxLayout(m_progressGroup);
    
    // 进度条
    m_progressSlider = new QSlider(Qt::Horizontal);
    m_progressSlider->setRange(0, 1000); // 使用0-1000的范围提供更精确的控制
    m_progressSlider->setValue(0);
    m_progressSlider->setEnabled(false);
    
    // 时间标签布局
    m_progressLayout = new QHBoxLayout();
    m_currentTimeLabel = new QLabel("00:00");
    m_totalTimeLabel = new QLabel("00:00");
    
    m_progressLayout->addWidget(m_currentTimeLabel);
    m_progressLayout->addStretch();
    m_progressLayout->addWidget(m_totalTimeLabel);
    
    m_progressMainLayout->addWidget(m_progressSlider);
    m_progressMainLayout->addLayout(m_progressLayout);
    
    // 音量控制组
    m_volumeGroup = new QGroupBox("音量控制");
    m_volumeLayout = new QHBoxLayout(m_volumeGroup);
    
    m_volumeLabel = new QLabel("音量:");
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(150);
    
    QLabel *volumeValueLabel = new QLabel("80%");
    connect(m_volumeSlider, &QSlider::valueChanged, [volumeValueLabel](int value) {
        volumeValueLabel->setText(QString("%1%").arg(value));
    });
    
    m_volumeLayout->addWidget(m_volumeLabel);
    m_volumeLayout->addWidget(m_volumeSlider);
    m_volumeLayout->addWidget(volumeValueLabel);
    m_volumeLayout->addStretch();
    
    // 声道平衡控制组
    m_balanceGroup = new QGroupBox("声道平衡");
    m_balanceLayout = new QHBoxLayout(m_balanceGroup);
    
    m_balanceLabel = new QLabel("平衡:");
    m_balanceSlider = new QSlider(Qt::Horizontal);
    m_balanceSlider->setRange(-100, 100); // -100到100，0为中间
    m_balanceSlider->setValue(0);
    m_balanceSlider->setFixedWidth(150);
    
    // 添加刻度标记
    m_balanceSlider->setTickPosition(QSlider::TicksBelow);
    m_balanceSlider->setTickInterval(50); // 每50个单位一个刻度
    
    m_balanceValueLabel = new QLabel("中");
    m_balanceValueLabel->setFixedWidth(30);
    
    // 添加左右标识
    QLabel *leftLabel = new QLabel("左");
    leftLabel->setStyleSheet("QLabel { font-size: 10px; color: gray; }");
    QLabel *rightLabel = new QLabel("右");
    rightLabel->setStyleSheet("QLabel { font-size: 10px; color: gray; }");
    
    m_balanceLayout->addWidget(m_balanceLabel);
    m_balanceLayout->addWidget(leftLabel);
    m_balanceLayout->addWidget(m_balanceSlider);
    m_balanceLayout->addWidget(rightLabel);
    m_balanceLayout->addWidget(m_balanceValueLabel);
    m_balanceLayout->addStretch();
    
    // 信息显示组
    m_infoGroup = new QGroupBox("音频信息");
    m_infoLayout = new QVBoxLayout(m_infoGroup);
    
    m_audioInfoLabel = new QLabel("无音频文件");
    m_audioInfoLabel->setStyleSheet("QLabel { color: gray; }");
    
    m_bufferProgress = new QProgressBar();
    m_bufferProgress->setVisible(false);
    m_bufferProgress->setFormat("缓冲中... %p%");
    
    m_infoLayout->addWidget(m_audioInfoLabel);
    m_infoLayout->addWidget(m_bufferProgress);
    
    // 添加所有组到主布局
    m_mainLayout->addWidget(m_fileGroup);
    m_mainLayout->addWidget(m_controlGroup);
    m_mainLayout->addWidget(m_progressGroup);
    m_mainLayout->addWidget(m_volumeGroup);
    m_mainLayout->addWidget(m_balanceGroup);
    m_mainLayout->addWidget(m_infoGroup);
    m_mainLayout->addStretch();
    
    // 状态栏
    m_statusLabel = new QLabel("准备就绪");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupConnections()
{
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::playPause);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stop);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    
    // 进度条信号
    connect(m_progressSlider, &QSlider::sliderPressed, this, &MainWindow::onProgressSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &MainWindow::onProgressSliderReleased);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &MainWindow::onProgressSliderMoved);
    
    // 平衡控制信号
    connect(m_balanceSlider, &QSlider::valueChanged, this, &MainWindow::onBalanceChanged);
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "选择音频文件",
        "",
        "音频文件 (*.mp3 *.wav *.flac *.aac *.ogg *.m4a);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // 停止当前播放
    if (m_player) {
        m_player->stop();
    }
    
    // 打开新文件
    if (m_decoder->openFile(fileName)) {
        m_currentFile = fileName;
        
        // 初始化播放器
        if (m_player->initialize(m_decoder->getSampleRate(), 
                                m_decoder->getChannels(), 
                                m_decoder->getBitsPerSample())) {
            
            // 设置音频格式用于播放进度跟踪
            m_player->setAudioFormat(m_decoder->getSampleRate(), m_decoder->getChannels());
            
            // 更新UI
            QFileInfo fileInfo(fileName);
            m_fileLabel->setText(fileInfo.fileName());
            m_fileLabel->setStyleSheet("QLabel { color: black; }");
            
            // 获取总时长
            m_totalDuration = m_decoder->getTotalDuration();
            
            QString audioInfo = QString("采样率: %1 Hz, 声道数: %2, 位深度: %3 bits, 时长: %4")
                                .arg(m_decoder->getSampleRate())
                                .arg(m_decoder->getChannels())
                                .arg(m_decoder->getBitsPerSample())
                                .arg(formatTime(m_totalDuration));
            m_audioInfoLabel->setText(audioInfo);
            m_audioInfoLabel->setStyleSheet("QLabel { color: black; }");
            
            // 更新时间显示和进度条
            m_totalTimeLabel->setText(formatTime(m_totalDuration));
            m_currentTimeLabel->setText("00:00");
            m_progressSlider->setValue(0);
            m_progressSlider->setEnabled(m_totalDuration > 0);
            
            m_playPauseButton->setEnabled(true);
            m_stopButton->setEnabled(true);
            
            m_statusLabel->setText("文件加载成功");
            
            // 预加载大量音频数据以确保播放流畅
            qDebug() << "开始预加载音频数据...";
            for (int i = 0; i < 20; ++i) {
                loadMoreAudioData();
                if (m_decoder->isAtEnd()) break;
            }
            qDebug() << "预加载完成";
        } else {
            QMessageBox::warning(this, "错误", "无法初始化音频播放器");
        }
    } else {
        QMessageBox::warning(this, "错误", "无法打开音频文件");
    }
}

void MainWindow::playPause()
{
    if (!m_player) {
        return;
    }
    
    if (m_isPlaying) {
        m_player->pause();
        m_playPauseButton->setText("播放");
        m_updateTimer->stop();
        m_isPlaying = false;
        m_statusLabel->setText("暂停");
    } else {
        // 播放前确保有足够的数据
        qDebug() << "播放前检查缓冲区...";
        for (int i = 0; i < 30 && !m_decoder->isAtEnd(); ++i) {
            loadMoreAudioData();
        }
        
        m_player->play();
        m_playPauseButton->setText("暂停");
        m_updateTimer->start();
        m_isPlaying = true;
        m_statusLabel->setText("播放中");
    }
}

void MainWindow::stop()
{
    if (!m_player || !m_decoder) {
        return;
    }
    
    m_player->stop();
    m_decoder->seekToStart();
    
    m_playPauseButton->setText("播放");
    m_isPlaying = false;
    m_updateTimer->stop();
    
    m_statusLabel->setText("已停止");
    
    // 重置进度显示和播放进度跟踪
    m_progressSlider->setValue(0);
    m_currentTimeLabel->setText("00:00");
    if (m_player) {
        m_player->resetPlayedDuration();
    }
    
    // 清空缓冲区并重新加载音频数据
    m_player->clearBuffer();
    qDebug() << "重新加载音频数据...";
    for (int i = 0; i < 15; ++i) {
        loadMoreAudioData();
        if (m_decoder->isAtEnd()) break;
    }
}

void MainWindow::onVolumeChanged(int volume)
{
    if (m_player) {
        m_player->setVolume(volume / 100.0);
    }
}

void MainWindow::onBalanceChanged(int balance)
{
    if (m_player) {
        // 将-100到100的范围转换为-1.0到1.0
        qreal balanceValue = balance / 100.0;
        m_player->setBalance(balanceValue);
        
        // 更新显示标签
        if (balance < -10) {
            m_balanceValueLabel->setText("左");
        } else if (balance > 10) {
            m_balanceValueLabel->setText("右");
        } else {
            m_balanceValueLabel->setText("中");
        }
        
        qDebug() << "声道平衡设置为:" << balanceValue;
    }
}

void MainWindow::onPlayerStateChanged()
{
    updateUI();
}

void MainWindow::onPlayerNeedMoreData()
{
    qDebug() << "播放器请求更多数据";
    // 更激进地加载数据
    for (int i = 0; i < 20 && !m_decoder->isAtEnd(); ++i) {
        loadMoreAudioData();
    }
}

void MainWindow::onDecoderError(const QString &message)
{
    QMessageBox::warning(this, "解码错误", message);
    m_statusLabel->setText("解码错误");
}

void MainWindow::updatePlayback()
{
    if (!m_decoder || !m_player) {
        return;
    }
    
    // 更新播放进度（只有在用户不拖动进度条时才更新）
    if (!m_isSliderPressed && m_totalDuration > 0 && m_player) {
        // 使用播放器的实际播放时长而不是解码器的处理进度
        double currentPos = m_player->getPlayedDuration();
        int sliderValue = (int)((currentPos / m_totalDuration) * 1000);
        m_progressSlider->setValue(sliderValue);
        m_currentTimeLabel->setText(formatTime(currentPos));
    }
    
    // 检查是否需要更多数据
    if (m_decoder->isAtEnd() && !m_player->isPlaying()) {
        // 播放结束
        stop();
        m_statusLabel->setText("播放完成");
    }
}

void MainWindow::loadMoreAudioData()
{
    if (!m_decoder || !m_player) {
        return;
    }
    
    // 每次加载大量音频帧，确保缓冲区充足
    int loaded = 0;
    QByteArray batchData; // 批量收集数据
    
    for (int i = 0; i < 50 && !m_decoder->isAtEnd(); ++i) {
        QByteArray pcmData = m_decoder->decodeNextFrame();
        if (!pcmData.isEmpty()) {
            batchData.append(pcmData);
            loaded++;
        }
    }
    
    // 一次性添加所有数据
    if (!batchData.isEmpty()) {
        m_player->addPcmData(batchData);
        qDebug() << "批量加载了" << loaded << "个音频帧，总数据:" << batchData.size() << "字节";
    }
}

void MainWindow::resetPlayer()
{
    if (m_player) {
        m_player->clearBuffer();
    }
}

void MainWindow::updateUI()
{
    // 根据播放状态更新UI
    bool hasFile = !m_currentFile.isEmpty();
    bool isPlaying = m_player && m_player->isPlaying();
    
    m_playPauseButton->setEnabled(hasFile);
    m_stopButton->setEnabled(hasFile);
    m_progressSlider->setEnabled(hasFile && m_totalDuration > 0);
    
    if (isPlaying) {
        m_playPauseButton->setText("暂停");
    } else {
        m_playPauseButton->setText("播放");
    }
}

QString MainWindow::formatTime(double seconds)
{
    int totalSeconds = (int)seconds;
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
                           .arg(secs, 2, 10, QLatin1Char('0'));
}

void MainWindow::onProgressSliderPressed()
{
    m_isSliderPressed = true;
}

void MainWindow::onProgressSliderReleased()
{
    m_isSliderPressed = false;
    
    // 定位到新位置
    if (m_decoder && m_totalDuration > 0) {
        double newPosition = (double)m_progressSlider->value() / 1000.0 * m_totalDuration;
        
        // 停止播放并清空缓冲区
        if (m_player) {
            bool wasPlaying = m_player->isPlaying();
            m_player->stop();
            m_player->clearBuffer();
            
            // 定位解码器
            if (m_decoder->seekToPosition(newPosition)) {
                // 重置播放进度跟踪到新位置
                m_player->setPlayedDuration(newPosition);
                
                // 重新加载数据
                for (int i = 0; i < 30 && !m_decoder->isAtEnd(); ++i) {
                    loadMoreAudioData();
                }
                
                // 如果之前在播放，继续播放
                if (wasPlaying) {
                    m_player->play();
                    m_playPauseButton->setText("暂停");
                    m_updateTimer->start();
                    m_isPlaying = true;
                }
                
                // 更新时间显示
                m_currentTimeLabel->setText(formatTime(newPosition));
                qDebug() << "定位到:" << formatTime(newPosition);
            }
        }
    }
}

void MainWindow::onProgressSliderMoved(int value)
{
    // 在拖动时实时显示时间
    if (m_totalDuration > 0) {
        double position = (double)value / 1000.0 * m_totalDuration;
        m_currentTimeLabel->setText(formatTime(position));
    }
}
