#ifndef AUDIOCONVERTER_HPP
#define AUDIOCONVERTER_HPP

#include <string>
#include <fstream>
#include <cstdint>

#ifdef _WIN32
  #ifdef AUDIOCONVERTERDLL_EXPORTS
    #define AUDIOCONVERTER_API __declspec(dllexport)
  #else
    #define AUDIOCONVERTER_API __declspec(dllimport)
  #endif
#else
  #define AUDIOCONVERTER_API __attribute__((visibility("default")))
#endif

class AUDIOCONVERTER_API AudioConverter {
public:
    double ConvertBinToWavPartial(const std::string& inputBinPath, const std::string& outputWavPath, double startSec, double durationSec);
    double GetTotalDuration(const std::string& inputBinPath);

private:
    void writeWavHeader(std::ofstream& outFile, uint32_t dataSize, uint16_t numChannels, uint32_t sampleRate, uint16_t bitsPerSample);
};

#ifdef __cplusplus
extern "C" {
#endif


AUDIOCONVERTER_API int ConvertBinToWav_C(const char* inputPath, const char* outputPath, double startSec, double durationSec);
AUDIOCONVERTER_API double GetTotalDuration_C(const char* inputPath);

#ifdef __cplusplus
}
#endif

#endif // AUDIOCONVERTER_HPP