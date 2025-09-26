#include "audioexporter.h"
#include "audioplayer.h"
#include "nlohmann/json.hpp"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "utils.h"
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/signal.h>
#include <unistd.h>
#endif
#include <functional>
#include <mutex>
std::mutex g_mutex;
std::function<void()> g_exit_signal_handler;
std::string makeResultJson(int code, const std::string &message,
                           const nlohmann::json &data = nlohmann::json());
void exportCommand(const QCommandLineParser &parser);
void fetchCommand(const QCommandLineParser &parser);
void setupSignalHandle();
void setExitSignalHandler(std::function<void()> handler);

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);
  QCommandLineParser parser;
  setupSignalHandle();

  // 添加子命令
  parser.addPositionalArgument("command", "Command to execute (fetch, export)");
  parser.addPositionalArgument("args", "Command arguments", "[args...]");

  // 导出配置
  parser.addOption(QCommandLineOption("config", "config file", "config"));

  // 添加fetch命令的选项
  parser.addOption(
      QCommandLineOption("filepath", "Audio file path", "filepath"));
  parser.addOption(QCommandLineOption("base", "fetch base info"));
  parser.addOption(
      QCommandLineOption("samplenum", "sample number", "samplenum", "0"));
  parser.process(a);

  const QStringList args = parser.positionalArguments();
  QString command = "export";
  if (!args.isEmpty()) {
    command = args.first();
  }

  try {
    if (command == "fetchinfo" || command == "fetch") {
      fetchCommand(parser);
    } else if (!parser.value("config").isEmpty()) {
      exportCommand(parser);
    } else {
      std::cout << makeResultJson(-1, "Unknown command") << std::endl;
      return -1;
    }
    return 0;
  } catch (const std::exception &e) {
    qDebug() << e.what();
    std::cout << makeResultJson(-1, e.what()) << std::endl;
    return -1;
  }
}

std::string makeResultJson(int code, const std::string &message,
                           const nlohmann::json &data) {
  nlohmann::json result;
  result["code"] = code;
  result["message"] = message;
  result["data"] = data;
  return result.dump(-1, ' ', true);
}

void exportCommand(const QCommandLineParser &parser) {
  QString config_file = parser.value("config");
  if (config_file.isEmpty()) {
    throw std::runtime_error("config is required");
  }
  // 使用std::filesystem::path更安全地处理路径
  std::filesystem::path config_path = config_file.toStdWString();
  std::ifstream ifile(config_path);
  nlohmann::json config = nlohmann::json::parse(ifile);

  AudioExporter exporter;
  std::vector<std::filesystem::path> streams;
  nlohmann::json streamcfg = config["streams"];
  int count = streamcfg.size();
  for (int i = 0; i < count; i++) {
    streams.push_back(u82fs(streamcfg[i]["path"]));
  }
  exporter.open(streams);
  exporter.setTempo(config["tempo"]);
  exporter.setSemitone(config["semitone"]);
  for (int i = 0; i < count; i++) {
    exporter.setVolume(i, streamcfg[i]["volume"]);
    exporter.setVolumeBalance(i, streamcfg[i]["volumeBalance"]);
  }

  std::filesystem::path outdir = u82fs(config["outdir"]);
  std::string title = config["title"];
  std::string ext = config["ext"];
  std::vector<ExportItem> exports;
  for (int i : config["exports"]) {
    std::string type;
    if (i == -1) {
      type = "mix";
    } else {
      type = streamcfg[i]["type"];
    }
    ExportItem item;
    item.index = i;
    item.dest = outdir / u82fs((title + "_" + type + "." + ext));
    exports.push_back(item);
  }

  nlohmann::json progress_json;
  exporter.setProgressCallback([&](float progress) {
    float v = std::max<float>(0.0f, std::min<float>(progress * 100, 100.0f));
    progress_json["progress"] = v;
    std::cout << makeResultJson(1, "progress", progress_json) << std::endl;
  });
  setExitSignalHandler([&]() {
    exporter.stop();
  });
  auto b = exporter.exportFiles(exports);
  if (b) {
    progress_json["progress"] = 100.0f;
    std::cout << makeResultJson(0, "success", progress_json) << std::endl;
    qDebug() << "progress success";
  } else {
    std::cout << makeResultJson(-1, "failed") << std::endl;
    qDebug() << "progress failed";
  }
}

void fetchCommand(const QCommandLineParser &parser) {
  QString filepath = parser.value("filepath");
  if (filepath.isEmpty()) {
    throw std::runtime_error("filepath is required");
  }
  qDebug() << "Fetching audio info for:" << filepath;
  bool base = parser.isSet("base");
  int samplenum = parser.value("samplenum").toInt();

  AudioFileInfo info = AudioExporter::fetchAudioInfo(filepath.toStdWString(),
                                                     samplenum, !base, !base);
  nlohmann::json data;
  data["bpm"] = info.bpm;
  data["key"] = info.key;
  data["key_string"] = info.key_string;
  data["channels"] = info.channels;
  data["sample_rate"] = info.sample_rate;
  data["duration"] = info.duration_seconds;
  data["sample_format"] = info.sample_format;
  data["size_mp3"] = info.convert_to_mp3_size;
  data["size_wav"] = info.convert_to_wav_size;
  data["thumb"] = info.thumbnail;
  data["samples"] = info.samples_points;
  std::cout << makeResultJson(0, "success", data) << std::endl;
}

void setExitSignalHandler(std::function<void()> handler) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_exit_signal_handler = handler;
}

void signalHandler(int signal) {
  qDebug() << "!!!! received exit signal !!!!!" << signal;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_exit_signal_handler) {
    g_exit_signal_handler();
  }
}

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD dwType) {
  switch (dwType) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT:
  case CTRL_LOGOFF_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    signalHandler(dwType);
    return TRUE;
  }
  return FALSE;
}
void setupWindowsSignalHandling() {
  if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
    std::cerr << "Warning: Could not set Windows console control handler"
              << std::endl;
  }
}
#endif

void setupSignalHandle() {
#ifdef _WIN32
  setupWindowsSignalHandling();
  signal(SIGABRT, signalHandler);
  signal(SIGSEGV, signalHandler);
#else
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGABRT, signalHandler);
  signal(SIGQUIT, signalHandler);
#endif
}
