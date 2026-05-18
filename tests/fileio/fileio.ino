// Probe: confirm host-profile filesystem semantics that PCMFlow's
// helpers rely on.
//
//   1. fopen() with a relative path resolves against the sketch
//      directory (`tests/fileio/`).
//   2. The lang-ship host's mocked `SD` (fs::FS) reads/writes a real
//      directory the test infrastructure can inspect afterwards.
//
// host-only (sketch.yaml omits the esp32 profile). `output/` and the
// SD root are gitignored; the Python side checks both locations.

#include <Arduino.h>
#include <stdio.h>
#include <filesystem>

#include <FS.h>
#include <SD.h>

static const char *kFopenRelDir  = "output";
static const char *kFopenRelPath = "output/pcmflow_fileio_probe.bin";

static const uint8_t kFopenPayload[8] = {'P','C','M','F','L','O','W','!'};
static const uint8_t kSdPayload[8]    = {'S','D','-','P','R','O','B','E'};

static const char *kSdPath = "/pcmflow_sd_probe.bin";

// --------- fopen probe -----------------------------------------------------

static void probe_fopen()
{
    std::filesystem::path cwd = std::filesystem::current_path();
    Serial.print("CWD ");
    Serial.println(cwd.c_str());

    std::error_code ec;
    std::filesystem::create_directories(kFopenRelDir, ec);
    if (ec) {
        Serial.print("FAIL mkdir ");
        Serial.println(ec.message().c_str());
        return;
    }

    FILE *fp = fopen(kFopenRelPath, "wb");
    if (fp == nullptr) {
        Serial.print("FAIL fopen errno=");
        Serial.println(errno);
        return;
    }
    const size_t written = fwrite(kFopenPayload, 1, sizeof(kFopenPayload), fp);
    fclose(fp);

    Serial.print("WROTE bytes=");
    Serial.print((unsigned)written);
    Serial.print(" file=");
    Serial.println(kFopenRelPath);
}

// --------- SD round-trip probe --------------------------------------------

static void probe_sd()
{
    if (!SD.begin()) {
        // lang-ship host's mock SD.begin() succeeds unconditionally; this
        // branch is defensive.
        Serial.println("FAIL SD.begin()");
        return;
    }

    // Write
    {
        fs::File f = SD.open(kSdPath, FILE_WRITE);
        if (!f) {
            Serial.print("FAIL SD.open(write) ");
            Serial.println(kSdPath);
            return;
        }
        const size_t written = f.write(kSdPayload, sizeof(kSdPayload));
        f.close();
        Serial.print("SD WROTE bytes=");
        Serial.print((unsigned)written);
        Serial.print(" path=");
        Serial.println(kSdPath);
    }

    // Read back
    {
        fs::File f = SD.open(kSdPath, FILE_READ);
        if (!f) {
            Serial.print("FAIL SD.open(read) ");
            Serial.println(kSdPath);
            return;
        }
        uint8_t buf[16] = {0};
        const int n = f.read(buf, sizeof(buf));
        f.close();

        Serial.print("SD READ bytes=");
        Serial.print(n);
        Serial.print(" payload=");
        for (int i = 0; i < n; ++i) Serial.print((char)buf[i]);
        Serial.println();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    probe_fopen();
    probe_sd();

    Serial.println("TEST done");
}

void loop()
{
    delay(1);
}
