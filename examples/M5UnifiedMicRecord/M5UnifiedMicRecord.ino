// PCMFlow example: M5UnifiedMicRecord
//
// Records mic audio on an M5Stack Core2 while button A is held, applies
// PCMFlow's format conversion (mono -> stereo, 16 kHz -> 44.1 kHz),
// and writes the result as a WAV file to the SD card.
//
// Pipeline:
//   M5.Mic (16 kHz mono, int16) -> PCMSource adapter
//      -> PCMFlow (channel + rate conversion, gain)
//      -> WavWriter -> FileByteSink -> SD (/rec_NNN.wav)
//
// Hold Button A : record
// Release       : stop and save

#include <M5Unified.h>
#include <SD.h>
#include <PCMFlow.h>

// ---------------------------------------------------------------------------
// Mic source — exposes M5.Mic as a PCMSource (16 kHz mono, signed 16-bit).
// ---------------------------------------------------------------------------
class M5MicSource : public PCMSource
{
public:
    static constexpr uint32_t kRate = 16000;
    static constexpr uint8_t kChannels = 1;

    bool begin()
    {
        format_ = {kRate, kChannels, 16};
        // M5.Mic.begin() should already have been called by the sketch.
        return M5.Mic.isEnabled();
    }

    const PCMFormat &format() const override { return format_; }
    bool isEof() const override { return false; } // a live mic never ends
    bool isReady() const override { return M5.Mic.isEnabled(); }

    size_t readFrames(void *out, size_t frameCount) override
    {
        if (!M5.Mic.isEnabled() || out == nullptr || frameCount == 0)
            return 0;

        int16_t *dst = static_cast<int16_t *>(out);
        // M5.Mic.record() queues a capture into the caller buffer; wait for
        // it to finish before returning the frames to PCMFlow. For sample
        // simplicity we capture one block synchronously per call.
        if (!M5.Mic.record(dst, frameCount, kRate, /*stereo=*/false))
            return 0;
        while (M5.Mic.isRecording())
            delay(1);
        return frameCount;
    }

private:
    PCMFormat format_{};
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static PCMFlow audio;
static M5MicSource mic;

static fs::File g_file;
static FileByteSink g_sink;
static WavWriter g_wav;
static bool g_recording = false;
static uint32_t g_fileIndex = 1;

static constexpr size_t kChunkFrames = 256; // output frames per write

// ---------------------------------------------------------------------------
// Recording lifecycle
// ---------------------------------------------------------------------------
static void start_recording()
{
    char path[24];
    snprintf(path, sizeof(path), "/rec_%03lu.wav", (unsigned long)g_fileIndex);
    g_file = SD.open(path, FILE_WRITE);
    if (!g_file)
    {
        Serial.print("SD.open failed: ");
        Serial.println(path);
        return;
    }
    g_sink.setFile(g_file);

    if (!g_wav.begin(&g_sink, audio.outputFormat()))
    {
        Serial.print("WavWriter.begin failed, error=");
        Serial.println((int)g_wav.lastError());
        g_file.close();
        return;
    }

    audio.flushBuffer(); // drop anything captured between recordings
    g_recording = true;
    M5.Display.fillRect(0, 0, M5.Display.width(), 24, TFT_RED);
    M5.Display.setCursor(4, 4);
    M5.Display.print("REC ");
    M5.Display.print(path);
    Serial.print("Recording -> ");
    Serial.println(path);
}

static void stop_recording()
{
    g_recording = false;
    g_wav.end();
    g_sink.clear();
    g_file.close();

    M5.Display.fillRect(0, 0, M5.Display.width(), 24, TFT_DARKGREY);
    M5.Display.setCursor(4, 4);
    M5.Display.print("saved rec_");
    M5.Display.print(g_fileIndex);
    M5.Display.print(".wav");
    Serial.print("Saved. frames=");
    Serial.println((unsigned long)g_wav.framesWritten());

    g_fileIndex++;
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------
void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Mic.begin();
    Serial.begin(115200);

    M5.Display.setTextSize(2);
    M5.Display.fillRect(0, 0, M5.Display.width(), 24, TFT_DARKGREY);
    M5.Display.setCursor(4, 4);
    M5.Display.print("hold A to rec");

    if (!SD.begin(GPIO_NUM_4, SPI, 25000000))
    {
        Serial.println("SD.begin failed");
        M5.Display.setCursor(4, 40);
        M5.Display.print("SD: not mounted");
    }

    // Output format: showcase PCMFlow's conversion — mic is 16 kHz mono,
    // the WAV file is written as 44.1 kHz stereo.
    audio.setOutputFormat({44100, 2, 16});
    audio.setGain(1.0f);
    audio.setBufferFrames(4096);

    if (!mic.begin())
    {
        Serial.println("M5.Mic.begin failed");
        return;
    }
    audio.setInputSource(mic);
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed() && !g_recording)
        start_recording();

    if (g_recording)
    {
        audio.pump();
        while (audio.availableFrames() >= kChunkFrames)
        {
            static int16_t buf[kChunkFrames * 2]; // stereo
            const size_t got = audio.readFrames(buf, kChunkFrames);
            if (got == 0)
                break;
            g_wav.writeFrames(buf, got);
        }
    }

    if (M5.BtnA.wasReleased() && g_recording)
    {
        // Drain any remaining frames before closing the file.
        audio.pump();
        while (audio.availableFrames() > 0)
        {
            static int16_t buf[kChunkFrames * 2];
            const size_t got = audio.readFrames(buf, kChunkFrames);
            if (got == 0)
                break;
            g_wav.writeFrames(buf, got);
        }
        stop_recording();
    }
}
