#include "audioplayer.h"
#include "nlohmann/json.hpp"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QTimer>

void exportCommand(const QString &config_file);
void fetchCommand(const QString &filepath, bool base);


int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);
  QCommandLineParser parser;

  // 添加子命令
  parser.addPositionalArgument("command", "Command to execute (fetch, export)");
  parser.addPositionalArgument("args", "Command arguments", "[args...]");

  // 导出配置
  parser.addOption(QCommandLineOption("config", "config file", "config"));

  // 添加fetch命令的选项
  parser.addOption(
      QCommandLineOption("filepath", "Audio file path", "filepath"));
  parser.addOption(QCommandLineOption("base", "fetch base info", "base"));

  parser.process(a);

  const QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
    parser.showHelp(1);
  }

  const QString command = args.first();
  if (command == "fetchinfo" || command == "fetch") {
    // 处理fetch命令
    QString filepath = parser.value("filepath");
    if (filepath.isEmpty()) {
      qCritical() << "Error: --filepath is required for fetch command";
      parser.showHelp(1);
    }

    qDebug() << "Fetching audio info for:" << filepath;

    try {
      // 获取音频信息
      AudioInfo info =
          AudioPlayer::fetchFullAudioInfo(filepath, 1000); // 获取1000个采样点

      // 输出音频信息
      QTextStream out(stdout);
      out << "Audio Information:" << Qt::endl;
      out << "=================" << Qt::endl;
      out << "File: " << filepath << Qt::endl;
      out << "BPM: " << info.bpm << Qt::endl;
      out << "Key: " << info.key << " (" << info.key_string << ")" << Qt::endl;
      out << "Channels: " << info.channels << Qt::endl;
      out << "Sample Rate: " << info.sample_rate << " Hz" << Qt::endl;
      out << "Sample Format: " << info.sample_format << Qt::endl;
      out << "Duration: " << info.duration_seconds << " seconds" << Qt::endl;
      out << "Processing Time: " << info.consume_time_ms << " ms" << Qt::endl;
      out << "Sample Points: " << info.samples.size() << Qt::endl;

      if (!info.samples.empty()) {
        out << "First 10 sample values: ";
        for (int i = 0; i < std::min(10, static_cast<int>(info.samples.size()));
             ++i) {
          out << info.samples[i];
          if (i < std::min(10, static_cast<int>(info.samples.size())) - 1) {
            out << ", ";
          }
        }
        out << Qt::endl;
      }

    } catch (const std::exception &e) {
      qCritical() << "Error fetching audio info:" << e.what();
      return 1;
    }

  } else if (!parser.value("config").isEmpty()) {

    return 0;
  } else {
    qCritical() << "Unknown command:" << command;
    qCritical() << "Available commands: fetch, export";
    return 1;
  }
  return 0;
}
