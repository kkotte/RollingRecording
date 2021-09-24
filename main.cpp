#include "pch.h"

using namespace winrt;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Media::Capture;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Storage;

void OnError(MediaCapture const&, MediaCaptureFailedEventArgs const&)
{
    std::wcout << L"Failed callback invoked" << std::endl;
}

void PrintUsageAndExit()
{
    std::wcout << L"RollingRecording.exe -fileduration <minutes> -filestoSave <count>" << std::endl;
    exit(0);
}

char mytolower(int c)
{
    return (char)::tolower(c);
}

const string to_lower(string in)
{
    string out;
    out.resize(in.size());
    transform(in.begin(), in.end(), out.begin(), mytolower);
    return out;
}

wstring GetNextFileName()
{
    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);
    std::tm lt;
    localtime_s(&lt, &time);

    wstringstream ss;
    ss << L"RR_" << lt.tm_year 
                 << std::setfill(L'0') << std::setw(2) << lt.tm_mon 
                 << std::setfill(L'0') << std::setw(2) << lt.tm_mday << L"_" 
                 << std::setfill(L'0') << std::setw(2) << lt.tm_hour
                 << std::setfill(L'0') << std::setw(2) << lt.tm_min
                 << std::setfill(L'0') << std::setw(2) << lt.tm_sec
       << L".mp4";
    return ss.str();
}

bool g_stopCapture = false;
std::mutex g_stopCaptureLock;
std::condition_variable g_stopCaptureCv;

int main(int argc, char** argv)
{
    chrono::minutes fileDurationInMinutes(0);
    uint32_t filecount = 0;
    
    for (int i = 1; i < argc; i++)
    {
        string arg{ argv[i] };
        arg = to_lower(arg);

        if (arg == "-fileduration")
        {
            i++;
            if (fileDurationInMinutes.count() > 0 || i >= argc)
            {
                PrintUsageAndExit();
            }

            fileDurationInMinutes = chrono::minutes(atoi(argv[i]));
        }
        else if (arg == "-filestosave")
        {
            i++;
            if (filecount > 0 || i >= argc)
            {
                PrintUsageAndExit();
            }

            filecount = atoi(argv[i]);
        }
    }

    
    if (filecount == 0 || fileDurationInMinutes.count() == 0)
    {
        PrintUsageAndExit();
    }

    std::thread captureThread = std::thread([&]() {

        init_apartment();

        auto folder = KnownFolders::DocumentsLibrary();
        auto rrFolder = folder.CreateFolderAsync(L"RollingRecording", CreationCollisionOption::OpenIfExists).get();

        wstring* filenames = new wstring[filecount];
        int currentFileIndex = -1;

        while (!g_stopCapture)
        {
            currentFileIndex++;
            currentFileIndex %= filecount;
            if (!filenames[currentFileIndex].empty())
            {
                rrFolder.GetFileAsync(filenames[currentFileIndex]).get().DeleteAsync().get();
            }
            filenames[currentFileIndex] = GetNextFileName();

            MediaCapture mediaCapture;
            mediaCapture.Failed(OnError);

            mediaCapture.InitializeAsync().get();
            // Above call will throw on error

            auto file = rrFolder.CreateFileAsync(filenames[currentFileIndex]).get();
            wcout << L"Capturing to " << filenames[currentFileIndex] << L"..." << endl;
            mediaCapture.StartRecordToStorageFileAsync(MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Auto), file).get();

            unique_lock<mutex> lk(g_stopCaptureLock);
            /* bool stopCapturing = */ g_stopCaptureCv.wait_for(lk, std::chrono::minutes(fileDurationInMinutes), [&]() {return g_stopCapture; });
            mediaCapture.StopRecordAsync().get();
        }
    });

    wcout << L"Capturing " << fileDurationInMinutes.count() << " minute files. Will keep the last " << filecount << L" files." << endl;
    wcout << L"Press ENTER to stop recording." << endl;
    string dummy;
    cin >> dummy;

    g_stopCapture = true;
    g_stopCaptureCv.notify_all();

    if (captureThread.joinable())
    {
        captureThread.join();
    }

    return 0;
}
