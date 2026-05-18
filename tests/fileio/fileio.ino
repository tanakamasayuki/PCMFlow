// Probe test: verify how host-profile fopen() resolves relative paths,
// and confirm that writing into a dedicated `output/` subdirectory works.
//
// host-only (sketch.yaml omits the esp32 profile). The `output/` folder
// is gitignored so written files can be inspected after the run.

#include <Arduino.h>
#include <stdio.h>
#include <filesystem>

static const char *kRelDir = "output";
static const char *kRelPath = "output/pcmflow_fileio_probe.bin";

static void write_probe()
{
    std::filesystem::path cwd = std::filesystem::current_path();
    Serial.print("CWD ");
    Serial.println(cwd.c_str());

    std::error_code ec;
    std::filesystem::create_directories(kRelDir, ec);
    if (ec)
    {
        Serial.print("FAIL mkdir ");
        Serial.println(ec.message().c_str());
        return;
    }

    FILE *fp = fopen(kRelPath, "wb");
    if (fp == nullptr)
    {
        Serial.print("FAIL fopen errno=");
        Serial.println(errno);
        return;
    }
    // 8 known bytes: "PCMFLOW!"
    const uint8_t payload[8] = {'P', 'C', 'M', 'F', 'L', 'O', 'W', '!'};
    const size_t written = fwrite(payload, 1, sizeof(payload), fp);
    fclose(fp);

    Serial.print("WROTE bytes=");
    Serial.print((unsigned)written);
    Serial.print(" file=");
    Serial.println(kRelPath);
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    write_probe();

    Serial.println("TEST done");
}

void loop()
{
    delay(1);
}
