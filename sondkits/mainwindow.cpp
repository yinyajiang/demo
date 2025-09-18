#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include "common.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_player(nullptr)
    , m_updateTimer(nullptr)
    , m_isPlaying(false)
    , m_isSliderPressed(false)
    , m_totalDuration(0.0)
{
    setupUI();
    setupConnections();
    
    // 创建音频组件
    m_player = std::make_unique<AudioPlayer>(this);
    
    // 创建更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(500); // 每500ms更新一次，减少频率
    
    // 连接信号
    //connect(m_player, &AudioPlayer::stateChanged, this, &MainWindow::onPlayerStateChanged);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updatePlayback);
    
    updateUI();
    
    setWindowTitle("频播放器");
    setMinimumSize(500, 600);
    resize(500, 600);
}

MainWindow::~MainWindow()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_player) {
        m_player->stop();
    }
}

void MainWindow::setupUI()
{
    auto centralWidget = new QWidget;
    setCentralWidget(centralWidget);
    
    auto mainLayout = new QVBoxLayout(centralWidget);
    
    // 文件选择组
    auto fileGroup = new QGroupBox("文件选择");
    auto fileLayout = new QHBoxLayout(fileGroup);
    
    m_openButton = new QPushButton("打开音频文件");
    m_fileLabel = new QLabel("未选择文件");
    m_fileLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    
    fileLayout->addWidget(m_openButton);
    fileLayout->addWidget(m_fileLabel, 1);
    
    // 播放控制组
    auto controlGroup = new QGroupBox("播放控制");
    auto controlLayout = new QHBoxLayout(controlGroup);
    
    m_playPauseButton = new QPushButton("播放");
    m_stopButton = new QPushButton("停止");
    
    m_playPauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    
    controlLayout->addWidget(m_playPauseButton);
    controlLayout->addWidget(m_stopButton);
    controlLayout->addStretch();
    
    // 播放进度组
    auto progressGroup = new QGroupBox("播放进度");
    auto progressMainLayout = new QVBoxLayout(progressGroup);
    
    // 进度条
    m_progressSlider = new QSlider(Qt::Horizontal);
    m_progressSlider->setRange(0, 1000); // 使用0-1000的范围提供更精确的控制
    m_progressSlider->setValue(0);
    m_progressSlider->setEnabled(false);
    
    // 时间标签布局
    auto timelabelLayout = new QHBoxLayout();
    m_currentTimeLabel = new QLabel("00:00");
    m_totalTimeLabel = new QLabel("00:00");
    
    timelabelLayout->addWidget(m_currentTimeLabel);
    timelabelLayout->addStretch();
    timelabelLayout->addWidget(m_totalTimeLabel);

    progressMainLayout->addWidget(m_progressSlider);
    progressMainLayout->addStretch();
    progressMainLayout->addLayout(timelabelLayout);
    
    // 音量控制组
    auto volumeGroup = new QGroupBox("音量控制");
    auto volumeLayout = new QHBoxLayout(volumeGroup);
    
    m_volumeLabel = new QLabel("音量:");
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(150);
    
    auto volumeValueLabel = new QLabel(QString::number(m_volumeSlider->value()) + "%");
    connect(m_volumeSlider, &QSlider::valueChanged, [volumeValueLabel](int value) {
        volumeValueLabel->setText(QString("%1%").arg(value));
    });
    
    volumeLayout->addWidget(m_volumeLabel);
    volumeLayout->addWidget(m_volumeSlider);
    volumeLayout->addWidget(volumeValueLabel);
    volumeLayout->addStretch();
    
    // 声道平衡控制组
    auto balanceGroup = new QGroupBox("声道平衡");
    auto balanceLayout = new QHBoxLayout(balanceGroup);
    
    auto balanceLabel = new QLabel("平衡:");
    m_balanceSlider = new QSlider(Qt::Horizontal);
    m_balanceSlider->setRange(-100, 100); // -100到100，0为中间
    m_balanceSlider->setValue(0);
    m_balanceSlider->setFixedWidth(150);
    
    // 添加刻度标记
    m_balanceSlider->setTickPosition(QSlider::TicksBelow);
    m_balanceSlider->setTickInterval(50); // 每50个单位一个刻度
    
    m_balanceValueLabel = new QLabel("0");
    m_balanceValueLabel->setFixedWidth(30);
    
    // 添加左右标识
    auto leftLabel = new QLabel("左");
    leftLabel->setStyleSheet("QLabel { font-size: 10px; color: gray; }");
    auto rightLabel = new QLabel("右");
    rightLabel->setStyleSheet("QLabel { font-size: 10px; color: gray; }");
    
    balanceLayout->addWidget(balanceLabel);
    balanceLayout->addWidget(leftLabel);
    balanceLayout->addWidget(m_balanceSlider);
    balanceLayout->addWidget(rightLabel);
    balanceLayout->addWidget(m_balanceValueLabel);
    balanceLayout->addStretch();

    //速度控制组
    auto tempoGroup = new QGroupBox("速度控制");
    auto tempoLayout = new QHBoxLayout(tempoGroup);
    
    auto tempoLabel = new QLabel("速度:");
    m_tempoSlider = new QSlider(Qt::Horizontal);
    m_tempoSlider->setRange(50, MAX_TEMPO*100);
    m_tempoSlider->setValue(100);
    m_tempoSlider->setFixedWidth(150);
    
    m_tempoValueLabel = new QLabel("100");
    m_tempoValueLabel->setFixedWidth(150);

    tempoLayout->addWidget(tempoLabel);
    tempoLayout->addWidget(m_tempoSlider);
    tempoLayout->addWidget(m_tempoValueLabel);
    tempoLayout->addStretch();


    
    // 信息显示组
    auto infoGroup = new QGroupBox("音频信息");
    auto infoLayout = new QVBoxLayout(infoGroup);
    
    m_audioInfoLabel = new QLabel("无音频文件");
    m_audioInfoLabel->setStyleSheet("QLabel { color: gray; }");
    
    m_bufferProgress = new QProgressBar();
    m_bufferProgress->setVisible(false);
    m_bufferProgress->setFormat("缓冲中... %p%");
    
    infoLayout->addWidget(m_audioInfoLabel);
    infoLayout->addWidget(m_bufferProgress);
    
    // 添加所有组到主布局
    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(controlGroup);
    mainLayout->addWidget(progressGroup);
    mainLayout->addWidget(volumeGroup);
    mainLayout->addWidget(balanceGroup);
    mainLayout->addWidget(tempoGroup);
    mainLayout->addWidget(infoGroup);
    mainLayout->addStretch();
    
    // 状态栏
    m_statusLabel = new QLabel("准备就绪");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupConnections()
{
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::playPause);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stop);
    connect(m_volumeSlider, &QSlider::valueChanged, this,
            &MainWindow::onVolumeChanged);
    connect(m_tempoSlider, &QSlider::valueChanged, this, &MainWindow::onTempoChanged);
    
    // 进度条信号
    connect(m_progressSlider, &QSlider::sliderPressed, this, &MainWindow::onProgressSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &MainWindow::onProgressSliderReleased);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &MainWindow::onProgressSliderMoved);
    
    // 平衡控制信号
    connect(m_balanceSlider, &QSlider::valueChanged, this, &MainWindow::onBalanceChanged);
}

void MainWindow::onOpenFile()
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
    m_player->open(fileName.toStdString());
    m_player->play();
}

void MainWindow::playPause()
{

}

void MainWindow::stop()
{
    m_player->stop();

    m_playPauseButton->setText("播放");
    m_isPlaying = false;
    m_updateTimer->stop();
    
    m_statusLabel->setText("已停止");
    
    // 重置进度显示和播放进度跟踪
    m_progressSlider->setValue(0);
    m_currentTimeLabel->setText("00:00");
}

void MainWindow::onVolumeChanged(int volume)
{
    if (m_player) {
        m_player->setVolume(volume / 100.0);
    }
}

void MainWindow::onBalanceChanged(int balance)
{
    qreal balanceValue = balance / 100.0;
    m_player->setVolumeBalance(balanceValue);
    m_balanceValueLabel->setText(QString::number(balance));
}

void MainWindow::onTempoChanged(int tempo)
{
    m_player->setTempo(tempo / 100.0);
    m_tempoValueLabel->setText(QString::number(tempo / 100.0));
}

void MainWindow::onPlayerStateChanged()
{
    updateUI();
}


void MainWindow::onDecoderError(const QString &message)
{
    QMessageBox::warning(this, "解码错误", message);
    m_statusLabel->setText("解码错误");
}

void MainWindow::updatePlayback()
{

    // 更新播放进度（只有在用户不拖动进度条时才更新）
    if (!m_isSliderPressed && m_totalDuration > 0 && m_player) {
        // 使用播放器的实际播放时长而不是解码器的处理进度
        // double currentPos = m_player->getPlayedDuration();
        // int sliderValue = (int)((currentPos / m_totalDuration) * 1000);
        // m_progressSlider->setValue(sliderValue);
        // m_currentTimeLabel->setText(formatTime(currentPos));
    }
    
    // // 检查是否需要更多数据
    // if (m_decoder->isAtEnd() && !m_player->isPlaying()) {
    //     // 播放结束
    //     stop();
    //     m_statusLabel->setText("播放完成");
    // }
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
    if (m_player && m_totalDuration > 0) {
       
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
