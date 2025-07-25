
using ReactiveUI;
using System.Collections.ObjectModel;
using System.Reactive;
using System.Threading.Tasks;
using NetMQ;
using NetMQ.Sockets;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.IO;
using System.Runtime.InteropServices;

namespace AudioConverterUI
{     
    public class MainWindowViewModel : ReactiveObject
    {    
    [DllImport("libAudioConverterDll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ConvertBinToWav_C(
        string inputPath,
        string outputPath,
        double startSec,
        double durationSec);

    [DllImport("libAudioConverterDll", CallingConvention = CallingConvention.Cdecl)]
    public static extern double GetTotalDuration_C(string inputPath);

        private double _totalDurationSec;  // toplam süreyi saniye olarak tutuyoruz

        private string _totalDuration = "";
        public string TotalDuration
        {
            get => _totalDuration;
            set => this.RaiseAndSetIfChanged(ref _totalDuration, value);
        }

        private string? _selectedOption;
        public string? SelectedOption
        {
            get => _selectedOption;
            set => this.RaiseAndSetIfChanged(ref _selectedOption, value);
        }

        public ObservableCollection<string> Options { get; } = new ObservableCollection<string>();

        private string _statusMessage = "";
        public string StatusMessage
        {
            get => _statusMessage;
            set => this.RaiseAndSetIfChanged(ref _statusMessage, value);
        }

        public ReactiveCommand<Unit, Unit> ConvertCommand { get; }

        // Constructor
        public MainWindowViewModel()
        {
              
            ConvertCommand = ReactiveCommand.CreateFromTask(DoConvertAsync);
            _ = LoadDataAsync(); // başlat
        }

        private async Task LoadDataAsync()
        {
            await Task.Run(() =>
            {
                try
                {
                    // Masaüstü yolunu dinamik alıyoruz
                    string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);

                    // inputPath'ı dinamik olarak masaüstü yoluna göre ayarlıyoruz
                    string inputPath = Path.Combine(desktopPath, "12saat8bit.bin");

                    using var client = new RequestSocket();
                    client.Connect("tcp://localhost:5555");

                    var getOptionsReq = new
                    {
                        cmd = "GetDurationOptions",
                        inputPath = inputPath
                    };

                    client.SendFrame(JsonConvert.SerializeObject(getOptionsReq));
                    string reply = client.ReceiveFrameString();

                    var resp = JsonConvert.DeserializeObject<JObject>(reply);

                    _totalDurationSec = resp?["totalDuration"]?.Value<double>() ?? 0;
                    TotalDuration = FormatTime(_totalDurationSec); // hh:mm:ss formatında göster

                    Options.Clear();
                    var optionsArray = resp?["options"] as JArray;
                    if (optionsArray != null)
                    {
                        foreach (var option in optionsArray)
                            Options.Add(option.ToString());
                    }

                    StatusMessage = Options.Count > 0
                        ? "✅ Seçenekler yüklendi."
                        : "❌ Süre çok kısa, seçenek yok.";
                }
                catch (Exception ex)
                {
                    StatusMessage = "❌ Sunucuya bağlanılamadı: " + ex.Message;
                }
            });
        }

        private async Task DoConvertAsync()
        {
            if (SelectedOption == null)
            {
                StatusMessage = "⚠️ Lütfen bir süre seçin.";
                return;
            }

            double durationSec;
            if (SelectedOption == "Tamamı")
            {
                durationSec = _totalDurationSec;
            }
            else
            {
                if (!int.TryParse(SelectedOption.Split(' ')[0], out int hours))
                {
                    StatusMessage = "❌ Seçilen süre geçersiz.";
                    return;
                }
                durationSec = hours * 3600;
            }

            try
            {
                // Masaüstü yolunu dinamik olarak alıyoruz
                string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                
                // inputPath ve outputPath'ı dinamik olarak masaüstü yoluna göre ayarlıyoruz
                string inputPath = Path.Combine(desktopPath, "12saat8bit.bin");
                string outputPath = Path.Combine(desktopPath, "12saat8bit.wav");

                //string inputPath = "./12wsaat8bit.bin";

                // Eğer çıktı dosyası önceden varsa sil
                if (System.IO.File.Exists(outputPath))
                {
                    System.IO.File.Delete(outputPath);
                }

                using var client = new RequestSocket();
                client.Connect("tcp://localhost:5555");

                var convReq = new
                {
                    cmd = "ConvertBinToWav",
                    inputPath,
                    outputPath,
                    startSec = 0.0,
                    durationSec
                };

                var sw = System.Diagnostics.Stopwatch.StartNew();

                client.SendFrame(JsonConvert.SerializeObject(convReq));
                string reply = client.ReceiveFrameString();

                sw.Stop();
                double elapsedSeconds = sw.Elapsed.TotalSeconds;

                var convResp = JsonConvert.DeserializeObject<JObject>(reply);

                if (convResp != null && convResp.TryGetValue("success", out var successToken))
                {
                    int successValue = successToken.Value<int>();
                    if (successValue == 1)
                    {
                        StatusMessage = $"✅ Dönüşüm Başarılı! İşlem süresi: {elapsedSeconds:F2} sn";
                    }
                    else
                    {
                        string errorMsg = convResp.Value<string>("error") ?? "Bilinmeyen hata";
                        StatusMessage = "❌ Dönüşüm başarısız: " + errorMsg;
                    }
                }
                else
                {
                    StatusMessage = "❌ Dönüşüm başarısız: Sunucudan geçersiz cevap alındı.";
                }
            }
            catch (Exception ex)
            {
                StatusMessage = "❌ Dönüşüm sırasında hata: " + ex.Message;
            }
        }

        private string FormatTime(double totalSeconds)
        {
            TimeSpan ts = TimeSpan.FromSeconds(totalSeconds);
            return ts.ToString(@"hh\:mm\:ss");
        }
    }
}


