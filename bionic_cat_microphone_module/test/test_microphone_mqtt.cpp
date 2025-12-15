#include <iostream>
#include <fstream>
#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <vector>
#include <thread>
#include <string>

#include "mqtt_client.hpp"
#include "bionic_cat_mqtt_msg.hpp"
#include "serializer.hpp"

using namespace BionicCat::MqttClient;
using namespace BionicCat::MsgsSerializer;
using BionicCat::MqttMsgs::AdtsStreamControlMsg;
using BionicCat::MqttMsgs::AdtsStreamDataMsg;
using BionicCat::MqttMsgs::Header;

static std::atomic<bool> g_stop{false};
static void on_signal(int){ g_stop.store(true); }

struct Options {
    std::string server = "tcp://localhost:1883";
    std::string controlTopic = "bionic_cat/microphone_control";      // 发送控制
    std::string dataTopic    = "bionic_cat/microphone_audio_frame";  // 接收数据
    int qos = 1;
    int run_seconds = 5; // 采集时长
    std::string outFile = "mic_capture.aac"; // 输出 ADTS 文件
};

static void printHelp(const char* prog){
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --server <url>        MQTT broker (tcp://host:1883)\n"
              << "  --ctrl <topic>        Control topic (default: bionic_cat/microphone_control)\n"
              << "  --data <topic>        Data topic (default: bionic_cat/microphone_audio_frame)\n"
              << "  --qos <0|1|2>         QoS (default:1)\n"
              << "  --secs <n>            Capture seconds (default:5)\n"
              << "  --out <file>          Output ADTS file (default: mic_capture.aac)\n"
              << "  --help                Show help\n"; }

static bool parseInt(const std::string& s, int& v){ try{ size_t p=0; int t=std::stoi(s,&p); if(p!=s.size()) return false; v=t; return true;}catch(...){return false;} }

static Options parseArgs(int argc,char* argv[]){ Options o; for(int i=1;i<argc;++i){ std::string a=argv[i]; if(a=="--help"){ printHelp(argv[0]); std::exit(0);} else if(a=="--server"&& i+1<argc){ o.server=argv[++i]; } else if(a=="--ctrl"&&i+1<argc){ o.controlTopic=argv[++i]; } else if(a.rfind("--ctrl=",0)==0){ o.controlTopic=a.substr(7);} else if(a=="--data"&&i+1<argc){ o.dataTopic=argv[++i]; } else if(a.rfind("--data=",0)==0){ o.dataTopic=a.substr(7);} else if(a=="--qos"&&i+1<argc){ int q; if(parseInt(argv[++i],q)&&q>=0&&q<=2) o.qos=q; } else if(a.rfind("--qos=",0)==0){ int q; if(parseInt(a.substr(6),q)&&q>=0&&q<=2) o.qos=q; } else if(a=="--secs"&&i+1<argc){ parseInt(argv[++i], o.run_seconds); } else if(a.rfind("--secs=",0)==0){ parseInt(a.substr(7), o.run_seconds);} else if(a=="--out"&&i+1<argc){ o.outFile=argv[++i]; } else if(a.rfind("--out=",0)==0){ o.outFile=a.substr(6);} else { std::cerr << "Unknown arg: "<<a<<"\n"; printHelp(argv[0]); std::exit(1);} } return o; }

int main(int argc,char* argv[]){ std::signal(SIGINT,on_signal); std::signal(SIGTERM,on_signal);
    Options opt = parseArgs(argc, argv);
    std::cout << "[Test] server="<<opt.server<<" ctrl="<<opt.controlTopic<<" data="<<opt.dataTopic<<" qos="<<opt.qos<<" secs="<<opt.run_seconds<<" out="<<opt.outFile<<"\n";

    MQTTSubscriber dataSub(opt.server, "mic_test_data_sub", opt.qos);
    std::atomic<uint64_t> frames{0};
    std::atomic<uint64_t> bytes{0};
    std::atomic<uint32_t> lastSeq{0};
    std::atomic<uint64_t> seqGapCount{0};
    std::ofstream ofs(opt.outFile, std::ios::binary);
    if(!ofs){ std::cerr << "Cannot open output file"<<std::endl; return 2; }

    dataSub.setMessageHandler([&](mqtt::const_message_ptr msg){
        try {
            auto &payload = msg->get_payload();
            auto m = Serializer::deserializeAdtsStreamData(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
            if(!m.payload.empty()){
                // 统计序号连续性
                if(frames.load() > 0) {
                    uint32_t expected = lastSeq.load() + 1;
                    if(m.seq != expected) seqGapCount++;
                }
                lastSeq = m.seq;
                ofs.write(reinterpret_cast<const char*>(m.payload.data()), (std::streamsize)m.payload.size());
                frames += m.frame_count; bytes += m.payload.size();
            }
        } catch(const std::exception& e){ std::cerr << "[Test] data decode error: "<< e.what()<<"\n"; }
    });

    if(!dataSub.connect() || !dataSub.subscribe(opt.dataTopic)) { std::cerr << "Data subscribe failed"<<std::endl; return 3; }

    MQTTPublisher ctrlPub(opt.server, "mic_test_ctrl_pub", opt.qos);
    if(!ctrlPub.connect()){ std::cerr << "Control publisher connect failed"<<std::endl; return 4; }

    AdtsStreamControlMsg start{};
    start.header.frame_id = "catlink";
    start.header.device_id = "A";
    start.header.timestamp = (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    start.is_start = true;
    start.sample_rate = 48000;
    start.channels = 1;
    start.bit_rate = 64000;
    start.aot = 2;

    auto startBin = Serializer::serializeAdtsStreamControl(start);
    ctrlPub.publish(opt.controlTopic, std::string(reinterpret_cast<const char*>(startBin.data()), startBin.size()), opt.qos, false);
    std::cout << "[Test] Sent start control"<<std::endl;
    auto captureStart = std::chrono::steady_clock::now();

    auto t0 = std::chrono::steady_clock::now();
    while(!g_stop.load()){
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-t0).count() >= opt.run_seconds) break;
    }

    AdtsStreamControlMsg stop = start; stop.is_start = false; stop.header.timestamp = (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto stopBin = Serializer::serializeAdtsStreamControl(stop);
    ctrlPub.publish(opt.controlTopic, std::string(reinterpret_cast<const char*>(stopBin.data()), stopBin.size()), opt.qos, false);
    std::cout << "[Test] Sent stop control"<<std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    dataSub.disconnect();
    ctrlPub.disconnect();
    ofs.close();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-captureStart).count();
    double seconds = ms / 1000.0;
    uint64_t f = frames.load();
    uint64_t b = bytes.load();
    double avgFrameBytes = f ? (double)b / f : 0.0;
    double avgBytesPerSec = seconds > 0 ? b / seconds : 0.0;
    double bitrateBps = avgBytesPerSec * 8.0;

    std::cout << "[Test] Done. frames="<< f
              << " bytes="<< b
              << " duration_ms="<< ms
              << " avg_frame_bytes="<< avgFrameBytes
              << " avg_bytes_per_sec="<< avgBytesPerSec
              << " est_bitrate_bps="<< bitrateBps
              << " seq_gap_count="<< seqGapCount.load()
              << " file="<< opt.outFile << "\n";
    return 0; }
