#include "AudioConverter.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>



 // wav header kısmı 
void AudioConverter::writeWavHeader(std::ofstream& outFile, uint32_t dataSize, uint16_t numChannels, uint32_t sampleRate, uint16_t bitsPerSample) {
    outFile.write("RIFF", 4);

    uint32_t chunkSize = 36 + dataSize; 
    outFile.write(reinterpret_cast<const char*>(&chunkSize), 4);

    outFile.write("WAVE", 4);
    outFile.write("fmt ", 4);

    uint32_t subchunk1Size = 16; // PCM
    outFile.write(reinterpret_cast<const char*>(&subchunk1Size), 4);

    uint16_t audioFormat = 1; // PCM
    outFile.write(reinterpret_cast<const char*>(&audioFormat), 2);

    outFile.write(reinterpret_cast<const char*>(&numChannels), 2);
    outFile.write(reinterpret_cast<const char*>(&sampleRate), 4);

    uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
    outFile.write(reinterpret_cast<const char*>(&byteRate), 4);

    uint16_t blockAlign = numChannels * bitsPerSample / 8;
    outFile.write(reinterpret_cast<const char*>(&blockAlign), 2);

    outFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    outFile.write("data", 4);
    outFile.write(reinterpret_cast<const char*>(&dataSize), 4);
}


// BİNARY WAV DÖNÜŞÜMÜ YAPAN KISIM 

double AudioConverter::ConvertBinToWavPartial(const std::string& inputBinPath, const std::string& outputWavPath, double startSec, double durationSec) {
    const uint16_t numChannels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitsPerSample = 8;
    const uint32_t bytesPerSample = bitsPerSample / 8;

    std::ifstream inFile(inputBinPath, std::ios::binary);
    if (!inFile) {
        std::cerr << "Input file could not be opened: " << inputBinPath << std::endl;
        return false;
    }

    
    inFile.seekg(0, std::ios::end);  // ddosyanın sonuna gidiyor  seek get ile 
    uint64_t fileSize = inFile.tellg(); // bulunduğu konumu byte olarak alıyor 
    inFile.seekg(0, std::ios::beg); // tekrar imlec başa geliyor 

    double totalDuration = static_cast<double>(fileSize) / (sampleRate * bytesPerSample * numChannels); // tolam byte kullanılarak saniye cinsinden süre hesaplamınyor 

    if (startSec < 0 || startSec >= totalDuration) {
        std::cerr << "Başlangıç zamanı hatalı." << std::endl;  
    }

    if (durationSec <= 0 || (startSec + durationSec) > totalDuration) {
        durationSec = totalDuration - startSec;
    }

    uint64_t startOffset = static_cast<uint64_t>(startSec * sampleRate * bytesPerSample * numChannels);  // kulanıcıdan gelen bilgiye göre başlangıç offseti belirleniyor
    uint64_t bytesToRead = static_cast<uint64_t>(durationSec * sampleRate * bytesPerSample * numChannels); // ne kadar veri ookunacağı belirleniyor

    inFile.seekg(startOffset, std::ios::beg);  // nseek get ile başlangıç noktasına  gittik 

    std::ofstream outFile(outputWavPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "Output file could not be created: " << outputWavPath << std::endl;
        return false;
    }
    

    writeWavHeader(outFile, static_cast<uint32_t>(bytesToRead), numChannels, sampleRate, bitsPerSample); // başlığı yazıyor 

    
    // 4 buffer ve yönetimi için değişkenler
    const int bufferCount = 4;
    const size_t bufferSize =  1024 * 1024; // 1 MB

    std::vector<std::vector<char>> buffers(bufferCount);
    for (auto& buf : buffers) {
        buf.resize(bufferSize);
    }

    bool buffersFilled[bufferCount] = { false, false, false, false };  // buffer veri içeriyor mu ? 
    bool buffersWritten[bufferCount] = { true, true, true, true}; //içindeki  veri diske yazıldı mı 
 
    // thread senkronizasyonu için utex
    std::mutex mtx; 
    std::condition_variable cv_read, cv_write;// yazıcı ve okuyucu threadleri bekletmek için mutexler

    bool readError = false; // hata var mı ? 
    bool doneReading = false; // okum aişleminin bitip bitmediği 

    uint64_t bytesLeft = bytesToRead; // daha okunacak byte sayısı 
    int readIndex = 0; // okuyucu hangi bufferda 
    int writeIndex = 0; // yazıcı hanfi bufferda

    auto reader = [&]() {
        while (bytesLeft > 0) {   // okunacak bytelar bitmediği sürece devam et 
            size_t toRead = (bytesLeft > bufferSize) ? bufferSize : static_cast<size_t>(bytesLeft);

            
            {
                std::unique_lock<std::mutex> lock(mtx); // buffer boşalana yani okuyucu işini biitrene kadar outputa yazıcı threadı bekletiyor 
                cv_write.wait(lock, [&]() { return buffersWritten[readIndex]; });
            }

            inFile.read(buffers[readIndex].data(), toRead); // dosyadan toread de kalan veri buffera okununr
            if (!inFile) {
                std::cerr << "Dosya okuma sırasında hata." << std::endl;
                std::unique_lock<std::mutex> lock(mtx);
                readError = true;
                doneReading = true;
                cv_read.notify_one();
                return;
            }

            {
                std::unique_lock<std::mutex> lock(mtx);  // buffer dolduysa artık yazılmaya hazır oluyor.
                buffersFilled[readIndex] = true;
                buffersWritten[readIndex] = false;     // buffer yazılmadı yani false . outputa yazma işlemi bitince true olacak
            }
            cv_read.notify_one();// yazıcıyı veri geldşğine yani yazmasına dair uyarıyoruz

            bytesLeft -= toRead;    // kalan byte sayısını 1 eksilttik 
            readIndex = (readIndex + 1) % bufferCount; // bir sonraki buffera okuma işlemi için geçildi 
        }

        {
            std::unique_lock<std::mutex> lock(mtx);  // okuma işlemmi artık bitti . yazcıyı uyaruyro 
            doneReading = true;
        }
        cv_read.notify_one();
    };

    auto writer = [&]() {
        while (true) { // yazıcı threadimiz sonsuz döngüde çalıştırılıyır 
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv_read.wait(lock, [&]() { return buffersFilled[writeIndex] || doneReading; });  // okuma bitene kadar bekliyor
                if (!buffersFilled[writeIndex]) { // buffer dolu mu ? diğer thread orda mı 
                    if (doneReading) break;
                    else continue;
                }
            }

            // Yazılacak byte sayısını hesapla
            size_t toWrite = buffers[writeIndex].size();   // default olarak buffer boyutunu alıyor ama bytestoread son değeri daha küçükse ona güncelliyor
            if (bytesToRead < toWrite) toWrite = static_cast<size_t>(bytesToRead);

            outFile.write(buffers[writeIndex].data(), toWrite); // bufferdaki veriyi outputh dosyama yazıyor 
            bytesToRead -= toWrite; // kalan yazılacak veriyi güncelliyorn 

            {
                std::unique_lock<std::mutex> lock(mtx);// buffer dolu mu ? yani o bufferdaki veri yazıcı tarafından taşınmı ş mı taşınmamış mı 
                buffersFilled[writeIndex] = false;  
                buffersWritten[writeIndex] = true;
            }
            cv_write.notify_one();

            writeIndex = (writeIndex + 1) % bufferCount;// bir sonraki buffera geçiyor
        }
    };
    
    std::thread tRead(reader);
    std::thread tWrite(writer);

    tRead.join();
    tWrite.join();

    inFile.close();
    outFile.close();

    if (readError) return false;
    return true;
}
// TOPLAM SÜRE HESAPLAMA 

double AudioConverter::GetTotalDuration(const std::string& inputBinPath) {
    const uint16_t numChannels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitsPerSample = 8;
    const uint32_t bytesPerSample = bitsPerSample / 8;

    std::ifstream inFile(inputBinPath, std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Input file could not be opened: " << inputBinPath << std::endl;
        return -1.0;
    }

    std::streamsize size = inFile.tellg();
    inFile.close();

    double totalDuration = static_cast<double>(size) / (sampleRate * bytesPerSample * numChannels);
    return totalDuration;
}

extern "C" int ConvertBinToWav_C(const char* inputPath, const char* outputPath, double startSec, double durationSec) {
    AudioConverter converter;
    bool result = converter.ConvertBinToWavPartial(std::string(inputPath), std::string(outputPath), startSec, durationSec);
    return result ? 1 : 0;
}

extern "C" double GetTotalDuration_C(const char* inputPath) {
    AudioConverter converter;
    return converter.GetTotalDuration(std::string(inputPath));
}