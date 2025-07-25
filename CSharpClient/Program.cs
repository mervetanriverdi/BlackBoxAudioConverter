using System;
using System.Diagnostics;
using System.Threading;
using NetMQ;
using NetMQ.Sockets;
using Newtonsoft.Json;
using System.Collections.Generic;
using System.IO;


class Program
{
    static Process? serverProcess = null;

    static void Main()
    {
        StartServer();

        StartUI();

        Console.WriteLine("UI kapandı. Server kapatılıyor...");
        StopServer();
    }

    static void StartServer()
    {
        if (serverProcess == null || serverProcess.HasExited)
        {
            serverProcess = new Process();

            string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            
            // AudioConverterServerExecutable'in tam yolunu oluşturuyoruz
            string serverExecutablePath = Path.Combine(desktopPath, "BlackBoxAudioConverter", "AudioConverterServer", "AudioConverterServerExecutable");

            serverProcess.StartInfo.FileName = serverExecutablePath;
            serverProcess.StartInfo.UseShellExecute = false;
            serverProcess.StartInfo.CreateNoWindow = true;
            serverProcess.Start();
            Console.WriteLine("Server başlatıldı.");
        }
    }

    static void StartUI()
    {
        var uiProcess = new Process();
        
        string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);

        // Proje klasörünü dinamik olarak alıyoruz
        string uiProjectPath = Path.Combine(desktopPath, "BlackBoxAudioConverter", "AudioConverterUI");

        // UI'yi başlatıyoruz
        uiProcess.StartInfo.FileName = "dotnet";
        uiProcess.StartInfo.Arguments = $"run --project {uiProjectPath}";
        uiProcess.StartInfo.UseShellExecute = false;
        uiProcess.StartInfo.CreateNoWindow = false; // UI görünsün
        uiProcess.Start();
        uiProcess.WaitForExit(); // UI kapanana kadar bekle
    }

    static void StopServer()
    {
        try
        {
            using var client = new NetMQ.Sockets.RequestSocket();
            client.Connect("tcp://localhost:5555");
            var shutdownReq = new { cmd = "Shutdown" };
            string json = Newtonsoft.Json.JsonConvert.SerializeObject(shutdownReq);
            client.SendFrame(json);
            client.ReceiveFrameString();
            Console.WriteLine("Server kapatıldı.");
        }
        catch
        {
            Console.WriteLine("Server zaten kapalı veya bağlantı hatası.");
        }
    }
}
