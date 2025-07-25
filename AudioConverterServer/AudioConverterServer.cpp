#include <zmq.hpp>
#include <string>
#include "json.hpp"
#include <iostream>
#include <vector>

using json = nlohmann::json;



#ifdef _WIN32
    #define DLLIMPORT __declspec(dllimport)
#else
    #define DLLIMPORT
#endif

extern "C" {
    DLLIMPORT double ConvertBinToWav_C(const char* inputPath, const char* outputPath, double startSec, double durationSec);
    DLLIMPORT double GetTotalDuration_C(const char* inputPath);
}


int main() {
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, zmq::socket_type::rep);
        socket.bind("tcp://*:5555");

        std::cout << "ZMQ Server started on port 5555\n";

        while (true) {
            zmq::message_t request;
            socket.recv(request, zmq::recv_flags::none);

            std::string reqStr(static_cast<char*>(request.data()), request.size());
            json reqJson = json::parse(reqStr);
            json response;

            if (reqJson.contains("cmd")) {
                std::string cmd = reqJson["cmd"];

                if (cmd == "ConvertBinToWav") {
                    std::string input = reqJson["inputPath"];
                    std::string output = reqJson["outputPath"];
                    double start = reqJson.value("startSec", 0.0);
                    double duration = reqJson.value("durationSec", -1.0);

                    double resultDuration = ConvertBinToWav_C(input.c_str(), output.c_str(), start, duration);
                    if (resultDuration > 0) {
                        response["success"] = 1;
                        response["convertedDuration"] = resultDuration;
                    } else {
                        response["success"] = 0;
                    }
                }
                else if (cmd == "GetTotalDuration") {
                    std::string input = reqJson["inputPath"];
                    double duration = GetTotalDuration_C(input.c_str());
                    response["duration"] = duration;
                }
                else if (cmd == "GetDurationOptions") {
                    std::string input = reqJson["inputPath"];
                    double totalDuration = GetTotalDuration_C(input.c_str());

                    std::vector<std::string> options;
                    double twoHoursInSec = 2 * 3600;

                    // 2 saatlik dilimler halinde, aşmayacak şekilde ekle
                    for (double t = twoHoursInSec; t <= totalDuration; t += twoHoursInSec) {
                        int hours = static_cast<int>(t / 3600);
                        options.push_back(std::to_string(hours) + " saat");
                    }
                    options.push_back("Tamamı");

                    response["totalDuration"] = totalDuration;
                    response["options"] = options;
                }
                else if (cmd == "Shutdown") {
                    response["success"] = true;
                    std::string respStr = response.dump();
                    socket.send(zmq::buffer(respStr), zmq::send_flags::none);
                    break;  // server kapansın
                }
                else {
                    response["error"] = "Unknown command";
                }
            }
            else {
                response["error"] = "Missing 'cmd' field";
            }

            std::string respStr = response.dump();
            socket.send(zmq::buffer(respStr), zmq::send_flags::none);
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "ZMQ Server exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}