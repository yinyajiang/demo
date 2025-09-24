#include "mainwindow.h"
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_player(nullptr), m_isPlaying(false),
      m_isSliderPressed(false), m_totalDuration(0.0) {
  setWindowTitle("频播放器");
  setFixedSize(500, 800);
  setupUI();

  // 创建音频组件
  m_player = std::make_unique<AudioPlayer>(this);
  QObject::connect(m_player.get(), &AudioPlayer::signalTimeProgress, this,
                   &MainWindow::onTimeProgress);

  // 连接信号
  // connect(m_player, &AudioPlayer::stateChanged, this,
  // &MainWindow::onPlayerStateChanged);
}

MainWindow::~MainWindow() {
  if (m_player) {
    m_player->stop();
  }
}

void MainWindow::setupUI() {
  auto centralWidget = new QWidget;
  setCentralWidget(centralWidget);

  auto mainLayout = new QVBoxLayout(centralWidget);

  // 文件选择组
  auto fileGroup = new QGroupBox("文件选择");
  auto fileLayout = new QVBoxLayout(fileGroup);

  auto fileLayout1 = new QHBoxLayout();
  m_openButton1 = new QPushButton("打开音频文件1");
  m_fileLabel1 = new QLabel("");
  m_fileLabel1->setStyleSheet("QLabel { color: gray; font-style: italic; }");
  fileLayout1->addWidget(m_openButton1);
  fileLayout1->addWidget(m_fileLabel1, 1);
  connect(m_openButton1, &QPushButton::clicked, [this]() { onOpenFile(1); });

  auto fileLayout2 = new QHBoxLayout();
  m_openButton2 = new QPushButton("打开音频文件2");
  m_fileLabel2 = new QLabel("");
  m_fileLabel2->setStyleSheet("QLabel { color: gray; font-style: italic; }");
  fileLayout2->addWidget(m_openButton2);
  fileLayout2->addWidget(m_fileLabel2, 1);
  connect(m_openButton2, &QPushButton::clicked, [this]() { onOpenFile(2); });

  fileLayout->addLayout(fileLayout1);
  fileLayout->addLayout(fileLayout2);

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

  connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stop);
  connect(m_playPauseButton, &QPushButton::clicked, this,
          &MainWindow::playPause);

  // 播放进度组
  auto progressGroup = new QGroupBox("播放进度");
  auto progressMainLayout = new QVBoxLayout(progressGroup);

  // 进度条
  m_progressSlider = new QSlider(Qt::Horizontal);
  m_progressSlider->setRange(0, 1000); // 使用0-1000的范围提供更精确的控制
  m_progressSlider->setValue(0);
  m_progressSlider->setEnabled(false);

  connect(m_progressSlider, &QSlider::sliderPressed, this,
          &MainWindow::onProgressSliderPressed);
  connect(m_progressSlider, &QSlider::sliderReleased, this,
          &MainWindow::onProgressSliderReleased);
  connect(m_progressSlider, &QSlider::sliderMoved, this,
          &MainWindow::onProgressSliderMoved);

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

  // 音量控制组1
  auto volumeGroup1 = new QGroupBox("音量控制1");
  auto volumeLayout1 = new QHBoxLayout(volumeGroup1);

  m_volumeLabel1 = new QLabel("音量1:");
  m_volumeSlider1 = new QSlider(Qt::Horizontal);
  m_volumeSlider1->setRange(0, 100);
  m_volumeSlider1->setValue(100);
  m_volumeSlider1->setFixedWidth(150);

  auto volumeValueLabel1 =
      new QLabel(QString::number(m_volumeSlider1->value()) + "%");
  volumeLayout1->addWidget(m_volumeLabel1);
  volumeLayout1->addWidget(m_volumeSlider1);
  volumeLayout1->addWidget(volumeValueLabel1);
  volumeLayout1->addStretch();
  connect(m_volumeSlider1, &QSlider::valueChanged, [&](int value) {
    onVolumeChanged(1, value);
  });

  // 声道平衡控制组1
  auto balanceGroup1 = new QGroupBox("声道平衡1");
  auto balanceLayout1 = new QHBoxLayout(balanceGroup1);

  auto balanceLabel1 = new QLabel("平衡1:");
  m_balanceSlider1 = new QSlider(Qt::Horizontal);
  m_balanceSlider1->setRange(-100, 100); // -100到100，0为中间
  m_balanceSlider1->setValue(0);
  m_balanceSlider1->setFixedWidth(150);

  // 添加刻度标记
  m_balanceSlider1->setTickPosition(QSlider::TicksBelow);
  m_balanceSlider1->setTickInterval(50); // 每50个单位一个刻度

  m_balanceValueLabel1 = new QLabel("0");
  m_balanceValueLabel1->setFixedWidth(30);
  connect(m_balanceSlider1, &QSlider::valueChanged, [&](int value) {
    onBalanceChanged(1, value);
  });

  // 添加左右标识
  auto leftLabel1 = new QLabel("左1");
  leftLabel1->setStyleSheet("QLabel { font-size: 10px; color: gray; }");
  auto rightLabel1 = new QLabel("右1");
  rightLabel1->setStyleSheet("QLabel { font-size: 10px; color: gray; }");

  balanceLayout1->addWidget(balanceLabel1);
  balanceLayout1->addWidget(leftLabel1);
  balanceLayout1->addWidget(m_balanceSlider1);
  balanceLayout1->addWidget(rightLabel1);
  balanceLayout1->addWidget(m_balanceValueLabel1);
  balanceLayout1->addStretch();


  // 音量控制组2
  auto volumeGroup2 = new QGroupBox("音量控制2");
  auto volumeLayout2 = new QHBoxLayout(volumeGroup2);
  m_volumeLabel2 = new QLabel("音量2:");
  m_volumeSlider2 = new QSlider(Qt::Horizontal);
  m_volumeSlider2->setRange(0, 100);
  m_volumeSlider2->setValue(100);
  m_volumeSlider2->setFixedWidth(150);

  auto volumeValueLabel2 =
    new QLabel(QString::number(m_volumeSlider2->value()) + "%");
  connect(m_volumeSlider2, &QSlider::valueChanged,
        [volumeValueLabel2](int value) {
          volumeValueLabel2->setText(QString("%1%").arg(value));
        });

  volumeLayout2->addWidget(m_volumeLabel2);
  volumeLayout2->addWidget(m_volumeSlider2);
  volumeLayout2->addWidget(volumeValueLabel2);
  volumeLayout2->addStretch();

  connect(m_volumeSlider2, &QSlider::valueChanged, [&](int value) {
    onVolumeChanged(2, value);
  });

  // 声道平衡控制组2
  auto balanceGroup2 = new QGroupBox("声道平衡2");
  auto balanceLayout2 = new QHBoxLayout(balanceGroup2);

  auto balanceLabel2 = new QLabel("平衡2:");
  m_balanceSlider2 = new QSlider(Qt::Horizontal);
  m_balanceSlider2->setRange(-100, 100); // -100到100，0为中间
  m_balanceSlider2->setValue(0);
  m_balanceSlider2->setFixedWidth(150);

  // 添加刻度标记
  m_balanceSlider2->setTickPosition(QSlider::TicksBelow);
  m_balanceSlider2->setTickInterval(50); // 每50个单位一个刻度

  m_balanceValueLabel2 = new QLabel("0");
  m_balanceValueLabel2->setFixedWidth(30);

  connect(m_balanceSlider2, &QSlider::valueChanged, [&](int value) {
    onBalanceChanged(2, value);
  });

  // 添加左右标识
  auto leftLabel2 = new QLabel("左2");
  leftLabel2->setStyleSheet("QLabel { font-size: 10px; color: gray; }");
  auto rightLabel2 = new QLabel("右2");
  rightLabel2->setStyleSheet("QLabel { font-size: 10px; color: gray; }");

  balanceLayout2->addWidget(balanceLabel2);
  balanceLayout2->addWidget(leftLabel2);
  balanceLayout2->addWidget(m_balanceSlider2);
  balanceLayout2->addWidget(rightLabel2);
  balanceLayout2->addWidget(m_balanceValueLabel2);
  balanceLayout2->addStretch();


  // 速度控制组
  auto tempoGroup = new QGroupBox("速度控制");
  auto tempoLayout = new QHBoxLayout(tempoGroup);

  auto tempoLabel = new QLabel("速度:");
  m_tempoSlider = new QSlider(Qt::Horizontal);
  m_tempoSlider->setRange(0.1 * 100, 2.0 * 100);
  m_tempoSlider->setValue(100);
  m_tempoSlider->setFixedWidth(150);

  m_tempoValueLabel = new QLabel("1");
  m_tempoValueLabel->setFixedWidth(150);

  tempoLayout->addWidget(tempoLabel);
  tempoLayout->addWidget(m_tempoSlider);
  tempoLayout->addWidget(m_tempoValueLabel);
  tempoLayout->addStretch();

  connect(m_tempoSlider, &QSlider::valueChanged, this,
          &MainWindow::onTempoChanged);

  // 升降调控制组
  auto semitoneGroup = new QGroupBox("升降调");
  auto semitoneLayout = new QHBoxLayout(semitoneGroup);
  auto semitoneLabel = new QLabel("升降调:");
  m_semitoneSlider = new QSlider(Qt::Horizontal);
  m_semitoneSlider->setRange(-12, 12);
  m_semitoneSlider->setValue(0);
  m_semitoneSlider->setFixedWidth(150);
  m_semitoneValueLabel = new QLabel("0");
  m_semitoneValueLabel->setFixedWidth(150);
  semitoneLayout->addWidget(semitoneLabel);
  semitoneLayout->addWidget(m_semitoneSlider);
  semitoneLayout->addWidget(m_semitoneValueLabel);
  semitoneLayout->addStretch();
  connect(m_semitoneSlider, &QSlider::valueChanged, this,
          &MainWindow::onSemitoneChanged);

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
  mainLayout->addWidget(volumeGroup1);
  mainLayout->addWidget(balanceGroup1);
  mainLayout->addWidget(volumeGroup2);
  mainLayout->addWidget(balanceGroup2);
  mainLayout->addWidget(tempoGroup);
  mainLayout->addWidget(semitoneGroup);
  mainLayout->addWidget(infoGroup);
  mainLayout->addStretch();

  // 状态栏
  m_statusLabel = new QLabel("准备就绪");
  statusBar()->addWidget(m_statusLabel);
}

void MainWindow::onOpenFile(int i) {
  QString fileName = QFileDialog::getOpenFileName(
      this, "选择音频文件", "",
      "音频文件 (*.mp3 *.wav *.flac *.aac *.ogg *.m4a);;所有文件 (*.*)");
  if (fileName.isEmpty()) {
    return;
  }
  if (i == 1) {
    m_fileLabel1->setText(QDir(fileName).dirName());
    m_file1 = fileName;
  } else {
    m_fileLabel2->setText(QDir(fileName).dirName());
    m_file2 = fileName;
  }
  if (m_file1.isEmpty() || m_file2.isEmpty()) {
    return;
  }

  std::vector<QString> filePaths;
  filePaths.push_back(m_file1);
  filePaths.push_back(m_file2);
  try {
    m_player->open(filePaths);
    m_playPauseButton->setEnabled(true);
    auto info = m_player->fetchFullAudioInfo(m_file1, 100);
    m_audioInfoLabel->setText(
        QString("BPM: %1, Key: %2, 通道: %3, 采样率: %4, 采样格式: %5,\r\n "
                "时长: %6, 耗时: %7ms")
            .arg(info.bpm)
            .arg(info.key_string)
            .arg(info.channels)
            .arg(info.sample_rate)
            .arg(info.sample_format)
            .arg(formatTime(info.duration_seconds))
            .arg(info.consume_time_ms));

    m_progressSlider->setEnabled(true);
    m_totalDuration = info.duration_seconds;
    m_progressSlider->setRange(0, 1000);
    m_totalTimeLabel->setText(formatTime(info.duration_seconds));
  } catch (const std::exception &e) {
    m_playPauseButton->setEnabled(false);
    m_audioInfoLabel->setText("错误: " + QString(e.what()));
  }
}

void MainWindow::playPause() {
  if (m_player->isPlaying()) {
    m_player->pause();
    m_playPauseButton->setText("播放");
  } else {
    m_player->play();
    m_playPauseButton->setText("暂停");
  }
}

void MainWindow::stop() {
  m_player->stop();

  m_playPauseButton->setText("播放");
  m_isPlaying = false;

  // 重置进度显示和播放进度跟踪
  m_progressSlider->setValue(0);
  m_currentTimeLabel->setText("00:00");
}

void MainWindow::onVolumeChanged(int stream_index, int volume) {
  m_player->setVolume(stream_index - 1, volume / 100.0);
}

void MainWindow::onBalanceChanged(int stream_index, int balance) {
  qreal balanceValue = balance / 100.0;
  m_player->setVolumeBalance(stream_index - 1, balanceValue);
  if (stream_index == 1) {
    m_balanceValueLabel1->setText(QString::number(balance));
  } else {
    m_balanceValueLabel2->setText(QString::number(balance));
  }
}

void MainWindow::onTempoChanged(int tempo) {
  m_player->setTempo(tempo / 100.0);
  m_tempoValueLabel->setText(QString::number(tempo / 100.0));
}

void MainWindow::onPlayerStateChanged() {}

void MainWindow::onDecoderError(const QString &message) {
  QMessageBox::warning(this, "解码错误", message);
  m_statusLabel->setText("解码错误");
}

void MainWindow::onTimeProgress(int64_t time_seconds) {
  // 更新播放进度（只有在用户不拖动进度条时才更新）
  if (!m_isSliderPressed && m_totalDuration > 0 && m_player) {
    int sliderValue = (int)((time_seconds / m_totalDuration) * 1000);
    m_progressSlider->setValue(sliderValue);
    m_currentTimeLabel->setText(formatTime(time_seconds));
  }
}

QString MainWindow::formatTime(double seconds) {
  int totalSeconds = (int)seconds;
  int minutes = totalSeconds / 60;
  int secs = totalSeconds % 60;
  return QString("%1:%2")
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(secs, 2, 10, QLatin1Char('0'));
}

void MainWindow::onProgressSliderPressed() { m_isSliderPressed = true; }

void MainWindow::onProgressSliderReleased() {
  m_isSliderPressed = false;

  if (m_totalDuration > 0) {
    double position =
        (double)m_progressSlider->value() / 1000 * m_totalDuration;
    int64_t position_ms = (int64_t)(position * 1000);
    m_player->seek(position_ms);
  }
}

void MainWindow::onProgressSliderMoved(int value) {
  // 在拖动时实时显示时间
  if (m_totalDuration > 0) {
    double position = (double)value / 1000.0 * m_totalDuration;
    m_currentTimeLabel->setText(formatTime(position));
  }
}

void MainWindow::onSemitoneChanged(int semitone) {
  m_player->setSemitone(semitone);
  m_semitoneValueLabel->setText(QString::number(semitone));
}
